import 'package:coast_audio/coast_audio.dart';
import 'package:coast_audio/src/interop/aac_decoder.dart';

export 'package:coast_audio/src/interop/aac_decoder.dart'
    show AacFormatException;

/// An audio decoder for AAC format in M4A/MP4 containers.
///
/// This decoder uses the FDK-AAC library to decode AAC audio and minimp4
/// to demux the MP4 container. It supports AAC-LC, HE-AAC, and HE-AAC v2.
///
/// The output format is always 16-bit signed integer PCM.
class AacAudioDecoder extends AudioDecoder with AudioResourceMixin {
  /// Creates a new AAC decoder for the given data source.
  ///
  /// Throws [AacFormatException] if the data source does not contain a valid
  /// M4A/MP4 file with an AAC audio track.
  AacAudioDecoder({
    required AudioInputDataSource dataSource,
    this.cacheCursorAndLength = true,
  })  : _dataSource = dataSource,
        _native = AacDecoder(dataSource: dataSource) {
    final captured = _native;
    setResourceFinalizer(() {
      captured.dispose();
    });

    if (cacheCursorAndLength) {
      _cachedLengthInFrames = _native.lengthInFrames;
      _cachedCursorInFrames = _native.cursorInFrames;
    }
  }

  final AudioInputDataSource _dataSource;
  final AacDecoder _native;

  int? _cachedLengthInFrames;
  int? _cachedCursorInFrames;

  var _isCursorDirty = false;

  /// Whether to cache the cursor and length of the audio.
  /// If false, [cursorInFrames] and [lengthInFrames] will be calculated on each access.
  final bool cacheCursorAndLength;

  @override
  int get cursorInFrames =>
      _cachedCursorInFrames ?? _native.cursorInFrames;

  @override
  set cursorInFrames(int value) {
    if (_cachedCursorInFrames != null) {
      _cachedCursorInFrames = value;
      _isCursorDirty = true;
      return;
    }

    _native.cursorInFrames = value;
  }

  @override
  int? get lengthInFrames =>
      _cachedLengthInFrames ?? _native.lengthInFrames;

  @override
  bool get canSeek => _dataSource.canSeek;

  @override
  late final AudioFormat outputFormat = _native.outputFormat;

  @override
  AudioDecodeResult decode({required AudioBuffer destination}) {
    if (_isCursorDirty) {
      _native.cursorInFrames = _cachedCursorInFrames!;
      _isCursorDirty = false;
    }

    if (destination.sizeInFrames == 0) {
      return const AudioDecodeResult(frameCount: 0, isEnd: false);
    }

    final result = _native.decode(destination);
    if (_cachedCursorInFrames != null) {
      _cachedCursorInFrames = _cachedCursorInFrames! + result.frameCount;
    }
    return result;
  }
}
