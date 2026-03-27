import 'dart:io';

import 'package:coast_audio/coast_audio.dart';
import 'package:test/test.dart';

import '../interop/internal/coast_audio_native_library.dart';

void main() {
  CoastAudioNative.initialize(library: resolveNativeLib());

  final testFilePath = Platform.environment['AAC_TEST_FILE'] ?? 'test/fixtures/test_audio.m4a';

  group('AacAudioDecoder', () {
    late AudioFileDataSource dataSource;

    setUp(() {
      final file = File(testFilePath);
      if (!file.existsSync()) {
        fail('Test file not found: $testFilePath. '
            'Generate one with: ffmpeg -f lavfi -i "sine=frequency=440:duration=3" -c:a aac -b:a 128k $testFilePath');
      }
      dataSource = AudioFileDataSource(file: file, mode: FileMode.read);
    });

    test('should initialize and detect AAC format', () {
      final decoder = AacAudioDecoder(dataSource: dataSource);

      expect(decoder.outputFormat.sampleRate, greaterThan(0));
      expect(decoder.outputFormat.channels, greaterThan(0));
      expect(decoder.outputFormat.sampleFormat, equals(SampleFormat.int16));

      print('AAC format detected: '
          '${decoder.outputFormat.sampleRate}Hz, '
          '${decoder.outputFormat.channels}ch, '
          '${decoder.outputFormat.sampleFormat}');

      AudioResourceManager.dispose(decoder.resourceId);
    });

    test('should report valid length', () {
      final decoder = AacAudioDecoder(dataSource: dataSource);

      final length = decoder.lengthInFrames;
      expect(length, isNotNull);
      expect(length!, greaterThan(0));

      final durationSeconds = length / decoder.outputFormat.sampleRate;
      print('AAC length: $length frames (~${durationSeconds.toStringAsFixed(2)}s)');

      // Our test file is ~3 seconds
      expect(durationSeconds, greaterThan(2.0));
      expect(durationSeconds, lessThan(5.0));

      AudioResourceManager.dispose(decoder.resourceId);
    });

    test('should decode PCM frames', () async {
      final decoder = AacAudioDecoder(dataSource: dataSource);
      final format = decoder.outputFormat;
      final frames = AllocatedAudioFrames(length: 4096, format: format);

      var totalFrames = 0;
      var decodeCount = 0;

      await AudioLoopClock().runWithBuffer(
        frames: frames,
        onTick: (clock, buffer) {
          final result = decoder.decode(destination: buffer);
          totalFrames += result.frameCount;
          decodeCount++;

          if (result.isEnd) {
            print('Decoded $totalFrames frames in $decodeCount iterations');
          }

          return !result.isEnd;
        },
      );

      expect(totalFrames, greaterThan(0));
      print('Total decoded: $totalFrames PCM frames '
          '(~${(totalFrames / format.sampleRate).toStringAsFixed(2)}s)');

      AudioResourceManager.dispose(decoder.resourceId);
    });

    test('should seek correctly', () {
      final decoder = AacAudioDecoder(dataSource: dataSource);

      // Seek to 1 second
      final targetFrame = decoder.outputFormat.sampleRate;
      decoder.cursorInFrames = targetFrame;
      expect(decoder.cursorInFrames, targetFrame);

      // Seek to beginning
      decoder.cursorInFrames = 0;
      expect(decoder.cursorInFrames, 0);

      // Decode after seek should produce frames
      final frames = AllocatedAudioFrames(length: 1024, format: decoder.outputFormat);
      final buffer = frames.lock();
      final result = decoder.decode(destination: buffer);
      frames.unlock();

      expect(result.frameCount, greaterThan(0));

      AudioResourceManager.dispose(decoder.resourceId);
    });

    test('should decode from AudioMemoryDataSource', () {
      // Test with in-memory buffer (same path the example app uses for content:// URIs)
      final fileBytes = File(testFilePath).readAsBytesSync();
      final memorySource = AudioMemoryDataSource(buffer: fileBytes);

      final decoder = AacAudioDecoder(dataSource: memorySource);

      expect(decoder.outputFormat.sampleRate, greaterThan(0));
      expect(decoder.outputFormat.channels, greaterThan(0));
      expect(decoder.lengthInFrames, greaterThan(0));

      // Decode a few frames to verify
      final frames = AllocatedAudioFrames(length: 4096, format: decoder.outputFormat);
      final buffer = frames.lock();
      final result = decoder.decode(destination: buffer);
      frames.unlock();

      expect(result.frameCount, greaterThan(0));
      print('Memory source decode OK: ${result.frameCount} frames');

      AudioResourceManager.dispose(decoder.resourceId);
    });
  });
}
