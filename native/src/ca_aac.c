#include "ca_aac.h"

#define MINIMP4_IMPLEMENTATION
#include "minimp4.h"

#include "aacdecoder_lib.h"

#include <stdlib.h>
#include <string.h>

/* Maximum PCM samples per AAC frame (HE-AAC v2 can produce 2048) */
#define CA_AAC_MAX_FRAME_SAMPLES 2048
#define CA_AAC_MAX_CHANNELS 8
/* Internal PCM buffer size in samples (per channel) */
#define CA_AAC_PCM_BUF_SIZE (CA_AAC_MAX_FRAME_SAMPLES * CA_AAC_MAX_CHANNELS)

/* Internal decoder state */
struct ca_aac_decoder {
    /* Callbacks */
    ca_aac_read_proc onRead;
    ca_aac_seek_proc onSeek;
    void* pUserData;

    /* MP4 demuxer */
    MP4D_demux_t mp4;
    int audioTrackIndex;

    /* File data (minimp4 needs random access, so we buffer the whole file) */
    uint8_t* fileData;
    int64_t fileSize;
    int64_t fileReadPos;

    /* AAC decoder */
    HANDLE_AACDECODER aacDecoder;

    /* Output format */
    ca_aac_format format;

    /* Sample/frame tracking */
    int64_t totalPcmFrames;     /* Total PCM frames in the file */
    int64_t cursorInPcmFrames;  /* Current PCM frame position */
    int pcmFramesPerAacFrame;   /* PCM frames per AAC frame (typically 1024) */

    /* Internal PCM buffer for partial reads */
    INT_PCM pcmBuffer[CA_AAC_PCM_BUF_SIZE];
    int pcmBufferFrames;        /* Frames available in pcmBuffer */
    int pcmBufferOffset;        /* Frames already consumed from pcmBuffer */

    /* Current AAC sample index */
    unsigned int currentSample;

    /* Initialized flag */
    int initialized;
};

/* minimp4 read callback - reads from our buffered file data */
static int mp4_read_callback(int64_t offset, void* buffer, size_t size, void* token) {
    ca_aac_decoder* dec = (ca_aac_decoder*)token;
    if (offset + (int64_t)size > dec->fileSize) {
        size = (size_t)(dec->fileSize - offset);
    }
    if (size <= 0) return 0;
    memcpy(buffer, dec->fileData + offset, size);
    return (int)size;
}

/* Read entire file into memory via callbacks */
static int read_entire_file(ca_aac_decoder* dec) {
    /* First, try to determine file size by seeking to end */
    int64_t totalRead = 0;
    int64_t capacity = 256 * 1024; /* Start with 256KB */
    int bytesRead = 0;

    dec->fileData = (uint8_t*)malloc((size_t)capacity);
    if (!dec->fileData) return CA_AAC_ERROR;

    while (1) {
        if (totalRead >= capacity) {
            capacity *= 2;
            uint8_t* newBuf = (uint8_t*)realloc(dec->fileData, (size_t)capacity);
            if (!newBuf) {
                free(dec->fileData);
                dec->fileData = NULL;
                return CA_AAC_ERROR;
            }
            dec->fileData = newBuf;
        }

        int toRead = (int)(capacity - totalRead);
        if (toRead > 65536) toRead = 65536;

        int result = dec->onRead(dec->pUserData, dec->fileData + totalRead, toRead, &bytesRead);
        if (result != 0 || bytesRead <= 0) break;
        totalRead += bytesRead;
    }

    dec->fileSize = totalRead;
    return (totalRead > 0) ? CA_AAC_OK : CA_AAC_ERROR;
}

/* Find the audio track in the MP4 file */
static int find_audio_track(MP4D_demux_t* mp4) {
    for (unsigned i = 0; i < mp4->track_count; i++) {
        MP4D_track_t* track = &mp4->track[i];
        /* object_type_indication 0x40 = Audio ISO/IEC 14496-3 (AAC) */
        if (track->handler_type == MP4D_HANDLER_TYPE_SOUN &&
            track->object_type_indication == 0x40) {
            return (int)i;
        }
    }
    return -1;
}

CA_AAC_API size_t ca_aac_decoder_sizeof(void) {
    return sizeof(ca_aac_decoder);
}

CA_AAC_API int ca_aac_decoder_init(
    ca_aac_decoder* pDecoder,
    ca_aac_read_proc onRead,
    ca_aac_seek_proc onSeek,
    void* pUserData
) {
    if (!pDecoder || !onRead) return CA_AAC_INVALID_ARGS;

    memset(pDecoder, 0, sizeof(ca_aac_decoder));
    pDecoder->onRead = onRead;
    pDecoder->onSeek = onSeek;
    pDecoder->pUserData = pUserData;

    /* Step 1: Read entire file into memory (minimp4 needs random access) */
    int result = read_entire_file(pDecoder);
    if (result != CA_AAC_OK) return CA_AAC_FORMAT_ERROR;

    /* Step 2: Parse MP4 container */
    if (!MP4D_open(&pDecoder->mp4, mp4_read_callback, pDecoder, pDecoder->fileSize)) {
        free(pDecoder->fileData);
        pDecoder->fileData = NULL;
        return CA_AAC_FORMAT_ERROR;
    }

    /* Step 3: Find AAC audio track */
    pDecoder->audioTrackIndex = find_audio_track(&pDecoder->mp4);
    if (pDecoder->audioTrackIndex < 0) {
        MP4D_close(&pDecoder->mp4);
        free(pDecoder->fileData);
        pDecoder->fileData = NULL;
        return CA_AAC_FORMAT_ERROR;
    }

    MP4D_track_t* track = &pDecoder->mp4.track[pDecoder->audioTrackIndex];

    /* Step 4: Initialize FDK-AAC decoder */
    pDecoder->aacDecoder = aacDecoder_Open(TT_MP4_RAW, 1);
    if (!pDecoder->aacDecoder) {
        MP4D_close(&pDecoder->mp4);
        free(pDecoder->fileData);
        pDecoder->fileData = NULL;
        return CA_AAC_ERROR;
    }

    /* Step 5: Configure with AudioSpecificConfig from MP4 */
    if (track->dsi && track->dsi_bytes > 0) {
        UCHAR* conf[] = { track->dsi };
        UINT confSize[] = { track->dsi_bytes };
        AAC_DECODER_ERROR err = aacDecoder_ConfigRaw(pDecoder->aacDecoder, conf, confSize);
        if (err != AAC_DEC_OK) {
            aacDecoder_Close(pDecoder->aacDecoder);
            MP4D_close(&pDecoder->mp4);
            free(pDecoder->fileData);
            pDecoder->fileData = NULL;
            return CA_AAC_FORMAT_ERROR;
        }
    }

    /* Step 6: Decode first frame to get stream info */
    if (track->sample_count > 0) {
        unsigned frameBytes = 0;
        unsigned timestamp = 0;
        unsigned duration = 0;
        MP4D_file_offset_t offset = MP4D_frame_offset(
            &pDecoder->mp4, pDecoder->audioTrackIndex, 0,
            &frameBytes, &timestamp, &duration
        );

        if (frameBytes > 0 && offset + frameBytes <= (uint64_t)pDecoder->fileSize) {
            UCHAR* inBuffer = pDecoder->fileData + offset;
            UINT inSize = frameBytes;
            UINT bytesValid = inSize;

            aacDecoder_Fill(pDecoder->aacDecoder, &inBuffer, &inSize, &bytesValid);
            AAC_DECODER_ERROR err = aacDecoder_DecodeFrame(
                pDecoder->aacDecoder, pDecoder->pcmBuffer, CA_AAC_PCM_BUF_SIZE, 0
            );

            if (err == AAC_DEC_OK) {
                CStreamInfo* info = aacDecoder_GetStreamInfo(pDecoder->aacDecoder);
                if (info) {
                    pDecoder->format.sampleRate = info->sampleRate;
                    pDecoder->format.channels = info->numChannels;
                    pDecoder->format.bitsPerSample = 16;
                    pDecoder->pcmFramesPerAacFrame = info->frameSize;

                    /* Store decoded first frame in buffer */
                    pDecoder->pcmBufferFrames = info->frameSize;
                    pDecoder->pcmBufferOffset = 0;
                    pDecoder->currentSample = 1;
                }
            }
        }
    }

    /* Fallback format from track metadata if first decode didn't work */
    if (pDecoder->format.sampleRate == 0) {
#ifndef MINIMP4_NO_SAMPLEDATA
        pDecoder->format.sampleRate = (int)track->SampleDescription.audio.samplerate_hz;
        pDecoder->format.channels = (int)track->SampleDescription.audio.channelcount;
#else
        pDecoder->format.sampleRate = 44100;
        pDecoder->format.channels = 2;
#endif
        pDecoder->format.bitsPerSample = 16;
        pDecoder->pcmFramesPerAacFrame = 1024; /* AAC-LC default */
    }

    /* Calculate total PCM frames */
    if (pDecoder->pcmFramesPerAacFrame > 0) {
        pDecoder->totalPcmFrames = (int64_t)track->sample_count * pDecoder->pcmFramesPerAacFrame;
    }

    pDecoder->cursorInPcmFrames = 0;
    pDecoder->initialized = 1;

    return CA_AAC_OK;
}

CA_AAC_API void ca_aac_decoder_uninit(ca_aac_decoder* pDecoder) {
    if (!pDecoder) return;

    if (pDecoder->aacDecoder) {
        aacDecoder_Close(pDecoder->aacDecoder);
        pDecoder->aacDecoder = NULL;
    }

    if (pDecoder->initialized) {
        MP4D_close(&pDecoder->mp4);
    }

    if (pDecoder->fileData) {
        free(pDecoder->fileData);
        pDecoder->fileData = NULL;
    }

    pDecoder->initialized = 0;
}

CA_AAC_API ca_aac_format ca_aac_decoder_get_format(const ca_aac_decoder* pDecoder) {
    ca_aac_format fmt = {0, 0, 0};
    if (pDecoder && pDecoder->initialized) {
        fmt = pDecoder->format;
    }
    return fmt;
}

CA_AAC_API int ca_aac_decoder_get_length_in_pcm_frames(const ca_aac_decoder* pDecoder, int64_t* pLength) {
    if (!pDecoder || !pLength) return CA_AAC_INVALID_ARGS;
    *pLength = pDecoder->totalPcmFrames;
    return CA_AAC_OK;
}

CA_AAC_API int ca_aac_decoder_get_cursor_in_pcm_frames(const ca_aac_decoder* pDecoder, int64_t* pCursor) {
    if (!pDecoder || !pCursor) return CA_AAC_INVALID_ARGS;
    *pCursor = pDecoder->cursorInPcmFrames;
    return CA_AAC_OK;
}

CA_AAC_API int ca_aac_decoder_seek_to_pcm_frame(ca_aac_decoder* pDecoder, int64_t frameIndex) {
    if (!pDecoder || !pDecoder->initialized) return CA_AAC_INVALID_ARGS;
    if (frameIndex < 0) frameIndex = 0;
    if (frameIndex > pDecoder->totalPcmFrames) frameIndex = pDecoder->totalPcmFrames;

    /* Calculate which AAC sample contains this PCM frame */
    unsigned int targetSample = 0;
    int offsetInSample = 0;

    if (pDecoder->pcmFramesPerAacFrame > 0) {
        targetSample = (unsigned int)(frameIndex / pDecoder->pcmFramesPerAacFrame);
        offsetInSample = (int)(frameIndex % pDecoder->pcmFramesPerAacFrame);
    }

    MP4D_track_t* track = &pDecoder->mp4.track[pDecoder->audioTrackIndex];
    if (targetSample >= track->sample_count) {
        targetSample = track->sample_count > 0 ? track->sample_count - 1 : 0;
    }

    /* Reset AAC decoder state for clean seek */
    aacDecoder_SetParam(pDecoder->aacDecoder, AAC_TPDEC_CLEAR_BUFFER, 1);

    /* Decode the target AAC frame */
    unsigned frameBytes = 0;
    unsigned timestamp = 0;
    unsigned duration = 0;
    MP4D_file_offset_t offset = MP4D_frame_offset(
        &pDecoder->mp4, pDecoder->audioTrackIndex, targetSample,
        &frameBytes, &timestamp, &duration
    );

    if (frameBytes > 0 && offset + frameBytes <= (uint64_t)pDecoder->fileSize) {
        UCHAR* inBuffer = pDecoder->fileData + offset;
        UINT inSize = frameBytes;
        UINT bytesValid = inSize;

        aacDecoder_Fill(pDecoder->aacDecoder, &inBuffer, &inSize, &bytesValid);
        AAC_DECODER_ERROR err = aacDecoder_DecodeFrame(
            pDecoder->aacDecoder, pDecoder->pcmBuffer, CA_AAC_PCM_BUF_SIZE, 0
        );

        if (err == AAC_DEC_OK) {
            CStreamInfo* info = aacDecoder_GetStreamInfo(pDecoder->aacDecoder);
            pDecoder->pcmBufferFrames = info ? info->frameSize : pDecoder->pcmFramesPerAacFrame;
            pDecoder->pcmBufferOffset = offsetInSample;
            pDecoder->currentSample = targetSample + 1;
        } else {
            pDecoder->pcmBufferFrames = 0;
            pDecoder->pcmBufferOffset = 0;
            pDecoder->currentSample = targetSample + 1;
        }
    }

    pDecoder->cursorInPcmFrames = frameIndex;
    return CA_AAC_OK;
}

/* Decode the next AAC sample and fill the internal PCM buffer */
static int decode_next_aac_frame(ca_aac_decoder* pDecoder) {
    MP4D_track_t* track = &pDecoder->mp4.track[pDecoder->audioTrackIndex];

    if (pDecoder->currentSample >= track->sample_count) {
        return CA_AAC_END_OF_STREAM;
    }

    unsigned frameBytes = 0;
    unsigned timestamp = 0;
    unsigned duration = 0;
    MP4D_file_offset_t offset = MP4D_frame_offset(
        &pDecoder->mp4, pDecoder->audioTrackIndex, pDecoder->currentSample,
        &frameBytes, &timestamp, &duration
    );

    if (frameBytes == 0 || offset + frameBytes > (uint64_t)pDecoder->fileSize) {
        return CA_AAC_DECODE_ERROR;
    }

    UCHAR* inBuffer = pDecoder->fileData + offset;
    UINT inSize = frameBytes;
    UINT bytesValid = inSize;

    aacDecoder_Fill(pDecoder->aacDecoder, &inBuffer, &inSize, &bytesValid);
    AAC_DECODER_ERROR err = aacDecoder_DecodeFrame(
        pDecoder->aacDecoder, pDecoder->pcmBuffer, CA_AAC_PCM_BUF_SIZE, 0
    );

    if (err != AAC_DEC_OK) {
        return CA_AAC_DECODE_ERROR;
    }

    CStreamInfo* info = aacDecoder_GetStreamInfo(pDecoder->aacDecoder);
    pDecoder->pcmBufferFrames = info ? info->frameSize : pDecoder->pcmFramesPerAacFrame;
    pDecoder->pcmBufferOffset = 0;
    pDecoder->currentSample++;

    return CA_AAC_OK;
}

CA_AAC_API int ca_aac_decoder_read_pcm_frames(
    ca_aac_decoder* pDecoder,
    void* pFramesOut,
    int64_t frameCount,
    int64_t* pFramesRead
) {
    if (!pDecoder || !pFramesOut || !pFramesRead) return CA_AAC_INVALID_ARGS;
    if (!pDecoder->initialized) return CA_AAC_ERROR;

    int16_t* output = (int16_t*)pFramesOut;
    int64_t framesWritten = 0;
    int channels = pDecoder->format.channels;

    while (framesWritten < frameCount) {
        /* Consume from internal buffer first */
        int available = pDecoder->pcmBufferFrames - pDecoder->pcmBufferOffset;
        if (available > 0) {
            int64_t toWrite = frameCount - framesWritten;
            if (toWrite > available) toWrite = available;

            memcpy(
                output + framesWritten * channels,
                pDecoder->pcmBuffer + pDecoder->pcmBufferOffset * channels,
                (size_t)(toWrite * channels * sizeof(INT_PCM))
            );

            framesWritten += toWrite;
            pDecoder->pcmBufferOffset += (int)toWrite;
            continue;
        }

        /* Need to decode next AAC frame */
        int result = decode_next_aac_frame(pDecoder);
        if (result == CA_AAC_END_OF_STREAM) {
            break;
        }
        if (result != CA_AAC_OK) {
            /* Skip corrupted frame and try next */
            pDecoder->currentSample++;
            continue;
        }
    }

    pDecoder->cursorInPcmFrames += framesWritten;
    *pFramesRead = framesWritten;

    if (framesWritten == 0) {
        return CA_AAC_END_OF_STREAM;
    }

    return CA_AAC_OK;
}
