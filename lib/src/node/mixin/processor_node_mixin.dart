import 'package:coast_audio/coast_audio.dart';

/// An audio node that reads from an input bus and calls [process] method automatically.
mixin ProcessorNodeMixin on SingleInNodeMixin {
  @override
  AudioReadResult read(AudioOutputBus outputBus, AudioBuffer buffer) {
    final originalResult = inputBus.connectedBus!.read(buffer);
    // Skip limit() allocation when upstream returned a full buffer.
    final limited = originalResult.frameCount == buffer.sizeInFrames
        ? buffer
        : buffer.limit(originalResult.frameCount);
    return process(limited, originalResult.isEnd);
  }

  /// Process the input buffer and return the result.
  ///
  /// The [buffer] is the input buffer to process.
  /// The [isEnd] is true if the input buffer is the last buffer of the input stream.
  AudioReadResult process(AudioBuffer buffer, bool isEnd);
}
