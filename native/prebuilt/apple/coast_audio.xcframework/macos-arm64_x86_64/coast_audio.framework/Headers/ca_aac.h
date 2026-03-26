#ifndef CA_AAC_H
#define CA_AAC_H

#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
#define CA_AAC_API __declspec(dllexport)
#else
#define CA_AAC_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Result codes */
#define CA_AAC_OK              0
#define CA_AAC_ERROR          -1
#define CA_AAC_INVALID_ARGS   -2
#define CA_AAC_FORMAT_ERROR   -3
#define CA_AAC_DECODE_ERROR   -4
#define CA_AAC_END_OF_STREAM  -5

/* Seek origins (matching miniaudio convention) */
#define CA_AAC_SEEK_ORIGIN_START   0
#define CA_AAC_SEEK_ORIGIN_CURRENT 1
#define CA_AAC_SEEK_ORIGIN_END     2

/* Callback types for reading data from Dart */
typedef int (*ca_aac_read_proc)(void* pUserData, void* pBufferOut, int bytesToRead, int* pBytesRead);
typedef int (*ca_aac_seek_proc)(void* pUserData, int64_t byteOffset, int origin);

/* Audio format information */
typedef struct {
    int sampleRate;
    int channels;
    int bitsPerSample;  /* 16 for int16 output */
} ca_aac_format;

/* Opaque decoder handle */
typedef struct ca_aac_decoder ca_aac_decoder;

/* Returns the size of ca_aac_decoder struct (for Dart allocation) */
CA_AAC_API size_t ca_aac_decoder_sizeof(void);

/* Initialize the decoder. Reads and parses the MP4 container, sets up AAC decoder. */
CA_AAC_API int ca_aac_decoder_init(
    ca_aac_decoder* pDecoder,
    ca_aac_read_proc onRead,
    ca_aac_seek_proc onSeek,
    void* pUserData
);

/* Uninitialize and free internal resources. */
CA_AAC_API void ca_aac_decoder_uninit(ca_aac_decoder* pDecoder);

/* Get the output audio format. Only valid after init. */
CA_AAC_API ca_aac_format ca_aac_decoder_get_format(const ca_aac_decoder* pDecoder);

/* Get total length in PCM frames. Returns 0 if unknown. */
CA_AAC_API int ca_aac_decoder_get_length_in_pcm_frames(const ca_aac_decoder* pDecoder, int64_t* pLength);

/* Get current cursor position in PCM frames. */
CA_AAC_API int ca_aac_decoder_get_cursor_in_pcm_frames(const ca_aac_decoder* pDecoder, int64_t* pCursor);

/* Seek to a specific PCM frame position. */
CA_AAC_API int ca_aac_decoder_seek_to_pcm_frame(ca_aac_decoder* pDecoder, int64_t frameIndex);

/* Read decoded PCM frames into output buffer.
   pFramesOut: output buffer (int16 interleaved)
   frameCount: max frames to read
   pFramesRead: out param, actual frames read
   Returns CA_AAC_OK or CA_AAC_END_OF_STREAM when done. */
CA_AAC_API int ca_aac_decoder_read_pcm_frames(
    ca_aac_decoder* pDecoder,
    void* pFramesOut,
    int64_t frameCount,
    int64_t* pFramesRead
);

#ifdef __cplusplus
}
#endif

#endif /* CA_AAC_H */
