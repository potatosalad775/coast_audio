import 'dart:io';

import 'package:audio_session/audio_session.dart';
import 'package:coast_audio/coast_audio.dart';
import 'package:example/main_page.dart';
import 'package:example/models/audio_state.dart';
import 'package:example/pages/backend_page.dart';
import 'package:flutter/material.dart';

/// Pass file paths as command-line arguments to auto-load them in the player.
/// Example: flutter run -d linux -- /path/to/audio.m4a /path/to/other.wav
Future<void> main(List<String> args) async {
  AudioResourceManager.isDisposeLogEnabled = true;

  WidgetsFlutterBinding.ensureInitialized();
  if (Platform.isAndroid || Platform.isIOS) {
    final session = await AudioSession.instance;
    await session.configure(const AudioSessionConfiguration(avAudioSessionCategory: AVAudioSessionCategory.playAndRecord));
    await session.setActive(true);
  }

  runApp(App(initialFilePaths: args));
}

class App extends StatefulWidget {
  const App({super.key, this.initialFilePaths = const []});

  final List<String> initialFilePaths;

  static AppState of(BuildContext context) {
    return context.findAncestorStateOfType<AppState>()!;
  }

  @override
  State<App> createState() => AppState();
}

class AppState extends State<App> {
  AudioState _state = const AudioStateInitial();

  AudioState get audioState => _state;

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'coast_audio',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.blueGrey),
        useMaterial3: true,
      ),
      darkTheme: ThemeData.dark(
        useMaterial3: true,
      ),
      home: switch (_state) {
        AudioStateInitial() => const BackendPage(),
        AudioStateConfigured() => MainPage(
            audio: _state as AudioStateConfigured,
            initialFilePaths: widget.initialFilePaths,
          ),
      },
    );
  }

  void applyAudioState(AudioState state) {
    setState(() {
      _state = state;
    });
  }
}
