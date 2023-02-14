import 'dart:math' as math;

import 'package:dart_audio_graph/dart_audio_graph.dart';

abstract class WaveFunction {
  const WaveFunction();

  double process(AudioTime time);
}

class SineFunction extends WaveFunction {
  const SineFunction();

  @override
  double process(AudioTime time) {
    return math.sin(2 * math.pi * time.seconds);
  }
}

class CosineFunction extends WaveFunction {
  const CosineFunction();

  @override
  double process(AudioTime time) {
    return math.cos(2 * math.pi * time.seconds);
  }
}

class SquareFunction extends WaveFunction {
  const SquareFunction();

  @override
  double process(AudioTime time) {
    final t = time.seconds - time.seconds.floorToDouble();
    if (t <= 0.5) {
      return 1;
    } else {
      return -1;
    }
  }
}

class TriangleFunction extends WaveFunction {
  const TriangleFunction();

  @override
  double process(AudioTime time) {
    final t = time.seconds - time.seconds.floorToDouble();
    return 2 * (2 * (t - 0.5)).abs() - 1;
  }
}
