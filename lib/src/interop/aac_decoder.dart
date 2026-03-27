import 'dart:ffi';

import 'package:coast_audio/coast_audio.dart';
import 'package:coast_audio/src/interop/internal/generated/bindings.dart';

// Custom callback typedefs using Int32 instead of the ABI-dependent Int.
// This fixes "Exceptional return value must be a constant" errors in Dart 3.7+
// where Int's size is unknown at compile time. C int is 32-bit on all
// platforms supported by coast_audio.
typedef _AacReadProcNative = Int32 Function(
    Pointer<Void> pUserData,
    Pointer<Void> pBufferOut,
    Int32 bytesToRead,
    Pointer<Int32> pBytesRead);
typedef _AacSeekProcNative = Int32 Function(
    Pointer<Void> pUserData, Int64 byteOffset, Int32 origin);

const int _kAacError = -1;

/// Low-level AAC decoder interop wrapping the native ca_aac_decoder.
class AacDecoder {
  AacDecoder({
    required this.dataSource,
  }) {
    if (dataSource.canSeek) {
      dataSource.position = 0;
    }
    _pUserData = _AacDecoderCallback.register(this);
    // Cast Int32 callback pointers to the Int-based types expected by the
    // generated bindings. This is safe because Int is Int32 on all supported
    // platforms.
    final result = _interop.bindings.ca_aac_decoder_init(
      _pDecoder,
      _AacDecoderCallback.onRead.cast(),
      _AacDecoderCallback.onSeek.cast(),
      _pUserData,
    );

    if (result != 0) {
      _AacDecoderCallback.unregister(_pUserData);
      _interop.dispose();
      throw AacFormatException('Failed to initialize AAC decoder (error: $result)');
    }

    final fmt = _interop.bindings.ca_aac_decoder_get_format(_pDecoder);
    outputFormat = AudioFormat(
      sampleRate: fmt.sampleRate,
      channels: fmt.channels,
      sampleFormat: SampleFormat.int16,
    );

    _interop.onInitialized();
  }

  final AudioInputDataSource dataSource;

  final _interop = CoastAudioInterop();
  late final Pointer<Void> _pUserData;

  late final _pDecoder = _interop
      .allocateManaged<ca_aac_decoder>(_interop.bindings.ca_aac_decoder_sizeof())
      .cast<ca_aac_decoder>();
  late final _pFrames = _interop.allocateManaged<Int64>(sizeOf<Int64>());

  late final AudioFormat outputFormat;

  int get cursorInFrames {
    _interop.bindings
        .ca_aac_decoder_get_cursor_in_pcm_frames(_pDecoder, _pFrames);
    return _pFrames.value;
  }

  set cursorInFrames(int value) {
    _interop.bindings.ca_aac_decoder_seek_to_pcm_frame(_pDecoder, value);
  }

  int get lengthInFrames {
    _interop.bindings
        .ca_aac_decoder_get_length_in_pcm_frames(_pDecoder, _pFrames);
    return _pFrames.value;
  }

  AudioDecodeResult decode(AudioBuffer destination) {
    if (destination.sizeInFrames == 0) {
      return const AudioDecodeResult(frameCount: 0, isEnd: false);
    }

    final result = _interop.bindings.ca_aac_decoder_read_pcm_frames(
      _pDecoder,
      destination.pBuffer.cast(),
      destination.sizeInFrames,
      _pFrames,
    );

    final framesRead = _pFrames.value;

    return AudioDecodeResult(
      frameCount: framesRead,
      isEnd: result == -5, // CA_AAC_END_OF_STREAM
    );
  }

  void dispose() {
    _AacDecoderCallback.unregister(_pUserData);
    _interop.bindings.ca_aac_decoder_uninit(_pDecoder);
    _interop.dispose();
  }

  int _onRead(
      Pointer<Void> pBufferOut, int bytesToRead, Pointer<Int32> pBytesRead) {
    pBytesRead.value =
        dataSource.readBytes(pBufferOut.cast<Uint8>().asTypedList(bytesToRead));
    return 0;
  }

  int _onSeek(int byteOffset, int origin) {
    switch (origin) {
      case 0: // CA_AAC_SEEK_ORIGIN_START
        dataSource.position = byteOffset;
        return 0;
      case 1: // CA_AAC_SEEK_ORIGIN_CURRENT
        dataSource.position += byteOffset;
        return 0;
      case 2: // CA_AAC_SEEK_ORIGIN_END
        final length = dataSource.length;
        if (length == null) return -1;
        dataSource.position = length + byteOffset;
        return 0;
      default:
        return -1;
    }
  }
}

class _AacDecoderCallback {
  // Use custom typedefs with Int32 (fixed-size) instead of ffigen's Int
  // (ABI-dependent) to fix "Exceptional return value must be a constant"
  // errors in Dart 3.7+ where Int's size is unknown at compile time.
  static final onRead =
      Pointer.fromFunction<_AacReadProcNative>(_onRead, _kAacError);
  static final onSeek =
      Pointer.fromFunction<_AacSeekProcNative>(_onSeek, _kAacError);

  static final _instances = <Pointer<Void>, AacDecoder>{};

  static Pointer<Void> register(AacDecoder instance) {
    final pUserData = instance._interop.memory.allocator.allocate<Void>(1);
    _instances[pUserData] = instance;
    return pUserData;
  }

  static void unregister(Pointer<Void> pUserData) {
    final instance = _instances.remove(pUserData);
    instance?._interop.memory.allocator.free(pUserData);
  }

  static int _onRead(Pointer<Void> pUserData, Pointer<Void> pBufferOut,
      int bytesToRead, Pointer<Int32> pBytesRead) {
    final instance = _instances[pUserData];
    if (instance == null) return -1;
    return instance._onRead(pBufferOut, bytesToRead, pBytesRead);
  }

  static int _onSeek(Pointer<Void> pUserData, int byteOffset, int origin) {
    final instance = _instances[pUserData];
    if (instance == null) return -1;
    return instance._onSeek(byteOffset, origin);
  }
}

/// Exception thrown when AAC format parsing or decoding fails.
class AacFormatException implements Exception {
  const AacFormatException(this.message);
  final String message;

  @override
  String toString() => 'AacFormatException: $message';
}
