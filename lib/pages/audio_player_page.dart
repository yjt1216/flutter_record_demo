import 'package:flutter/material.dart';
import 'package:just_audio/just_audio.dart';
import 'package:just_audio_platform_interface/just_audio_platform_interface.dart';

class AudioPlayerPage extends StatefulWidget {
  const AudioPlayerPage({super.key});

  @override
  State<AudioPlayerPage> createState() => _AudioPlayerPageState();
}

class _AudioPlayerPageState extends State<AudioPlayerPage> {
  late final AudioPlayer _player;
  String _statusText = 'Idle';

  @override
  void initState() {
    super.initState();
    _player = AudioPlayer();
    _init();
  }

  Future<void> _init() async {
    try {
      setState(() => _statusText = 'Loading');
      // 使用 assets 音频
      await _player.setAsset('assets/audio/common/countdown10s.mp3');
      setState(() => _statusText = 'Ready');
    } catch (e) {
      setState(() => _statusText = 'Error: $e');
    }
  }

  @override
  void dispose() {
    _player.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Audio Player (assets)')),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Text('Status: $_statusText'),
            const SizedBox(height: 12),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceEvenly,
              children: [
                ElevatedButton(
                  onPressed: () async {
                    await _player.play();
                  },
                  child: const Text('Play'),
                ),
                ElevatedButton(
                  onPressed: () async {
                    await _player.pause();
                  },
                  child: const Text('Pause'),
                ),
                ElevatedButton(
                  onPressed: () async {
                    await _player.stop();
                  },
                  child: const Text('Stop'),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}


