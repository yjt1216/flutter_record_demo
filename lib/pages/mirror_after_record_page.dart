import 'dart:io' show Process;

import 'package:camera_recorder/camera_recorder.dart';
import 'package:flutter/material.dart';
import 'package:path_provider/path_provider.dart';

/// 本地 FFmpeg 可执行路径（Windows 录后镜像转码用），按实际安装位置修改。
const String _windowsFfmpegPath =
    r'D:\testInstall\stroke_risk\ffmpeg-essentials_build\bin\ffmpeg.exe';

/// 录后镜像录制页：使用本项目的 camera_recorder 插件预览与录像，
/// 停止录像后若开启镜像则用本地 FFmpeg 做 hflip 转码（仅 Windows）。
class MirrorAfterRecordPage extends StatefulWidget {
  const MirrorAfterRecordPage({super.key});

  @override
  State<MirrorAfterRecordPage> createState() => _MirrorAfterRecordPageState();
}

class _MirrorAfterRecordPageState extends State<MirrorAfterRecordPage> {
  final CameraPlatform _cameraPlatform = CameraPlatform.instance;
  List<CameraDescription> _cameras = [];
  bool _loading = false;
  CameraDescription? _selectedCamera;
  int? _cameraId;
  bool _opened = false;
  Object? _initError;
  bool _isRecording = false;
  String? _outputVideoPath;
  int _previewW = 640;
  int _previewH = 480;
  bool _mirrorEnabled = true;

  @override
  void initState() {
    super.initState();
    _loadCameras();
  }

  Future<void> _loadCameras() async {
    setState(() {
      _loading = true;
      _initError = null;
    });
    try {
      final cams = await _cameraPlatform.availableCameras();
      if (!mounted) return;
      setState(() {
        _cameras = cams;
        if (_cameras.isNotEmpty && _selectedCamera == null) {
          _selectedCamera = _cameras.first;
        }
        _loading = false;
      });
    } catch (e, st) {
      if (!mounted) return;
      debugPrint('$e\n$st');
      setState(() {
        _initError = e;
        _loading = false;
      });
    }
  }

  Future<void> _openCamera() async {
    if (_cameras.isEmpty) {
      await _loadCameras();
      if (_cameras.isEmpty) {
        if (mounted) setState(() => _initError = '未找到摄像头');
        return;
      }
    }
    final camera = _selectedCamera ?? _cameras.first;
    setState(() => _initError = null);
    try {
      final cameraId = await _cameraPlatform.createCameraWithSettings(
        camera,
        MediaSettings(
          resolutionPreset: ResolutionPreset.medium,
          fps: 30,
          videoBitrate: 2000000,
          enableAudio: false,
        ),
      );
      if (!mounted) return;
      setState(() {
        _cameraId = cameraId;
        _opened = true;
      });
      await _cameraPlatform.initializeCamera(cameraId);
      await _cameraPlatform.setMirrorPreview(cameraId, _mirrorEnabled);
      if (!mounted) return;
      setState(() {
        _previewW = 640;
        _previewH = 480;
      });
    } catch (e, st) {
      if (!mounted) return;
      debugPrint('$e\n$st');
      setState(() => _initError = e);
    }
  }

  Future<void> _closeCamera() async {
    if (_cameraId == null) return;
    if (_isRecording) await _stopRecording();
    try {
      await _cameraPlatform.dispose(_cameraId!);
    } catch (_) {}
    if (!mounted) return;
    setState(() {
      _cameraId = null;
      _opened = false;
    });
  }

  @override
  void dispose() {
    if (_cameraId != null && _opened) {
      if (_isRecording) {
        _cameraPlatform.stopVideoRecording(_cameraId!).catchError((_) {});
      }
      _cameraPlatform.dispose(_cameraId!).catchError((_) {});
    }
    super.dispose();
  }

  Future<String> _getOutputPath() async {
    final dir = await getTemporaryDirectory();
    final timestamp = DateTime.now().millisecondsSinceEpoch;
    return '${dir.path}/mirror_video_$timestamp.mp4';
  }

  Future<void> _startRecording() async {
    if (_cameraId == null || !_opened || _isRecording) return;
    final outputPath = await _getOutputPath();
    try {
      await _cameraPlatform.startVideoRecording(_cameraId!, filePath: outputPath);
      if (!mounted) return;
      setState(() {
        _isRecording = true;
        _outputVideoPath = outputPath;
      });
    } catch (e) {
      if (mounted) {
        setState(() => _initError = '开始录像失败: $e');
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('开始录像失败: $e')),
        );
      }
    }
  }

  Future<void> _stopRecording() async {
    if (_cameraId == null || !_isRecording) return;
    try {
      final xFile = await _cameraPlatform.stopVideoRecording(_cameraId!);
      if (!mounted) return;
      String resultPath = xFile.path;

      if (_mirrorEnabled) {
        try {
          final dir = await getTemporaryDirectory();
          final timestamp = DateTime.now().millisecondsSinceEpoch;
          final mirroredPath =
              '${dir.path}/mirror_video_${timestamp}_mirrored.mp4';
          final pr = await Process.run(
            _windowsFfmpegPath,
            ['-i', xFile.path, '-vf', 'hflip', '-c:a', 'copy', '-y', mirroredPath],
            runInShell: false,
          );
          if (pr.exitCode == 0) {
            resultPath = mirroredPath;
          }
        } catch (_) {}
      }

      if (!mounted) return;
      setState(() {
        _isRecording = false;
        _outputVideoPath = resultPath;
      });
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('录像已保存: $resultPath')),
        );
      }
    } catch (e) {
      if (mounted) {
        setState(() => _isRecording = false);
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('停止录像失败: $e')),
        );
      }
    }
  }

  Future<void> _toggleRecording() async {
    if (_isRecording) {
      await _stopRecording();
    } else {
      await _startRecording();
    }
  }

  @override
  Widget build(BuildContext context) {
    if (_initError != null && !_opened) {
      return Scaffold(
        appBar: AppBar(title: const Text('录后镜像录制')),
        body: Center(
          child: Padding(
            padding: const EdgeInsets.all(24),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                Text(
                  '$_initError',
                  textAlign: TextAlign.center,
                  style: const TextStyle(color: Colors.red),
                ),
                const SizedBox(height: 16),
                ElevatedButton(onPressed: _loadCameras, child: const Text('重试')),
              ],
            ),
          ),
        ),
      );
    }

    if (!_opened) {
      return Scaffold(
        appBar: AppBar(title: const Text('录后镜像录制')),
        body: Padding(
          padding: const EdgeInsets.all(16),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              if (_loading)
                const Center(child: CircularProgressIndicator())
              else ...[
                if (_cameras.isNotEmpty) ...[
                  const Text('选择摄像头'),
                  const SizedBox(height: 8),
                  DropdownButton<CameraDescription>(
                    value: _selectedCamera,
                    items: _cameras
                        .map((c) => DropdownMenuItem(
                              value: c,
                              child: Text(c.name),
                            ))
                        .toList(),
                    onChanged: (v) => setState(() => _selectedCamera = v),
                  ),
                  const SizedBox(height: 16),
                ],
                ElevatedButton.icon(
                  onPressed: _loading ? null : _openCamera,
                  icon: const Icon(Icons.camera_alt),
                  label: const Text('打开摄像头'),
                ),
              ],
            ],
          ),
        ),
      );
    }

    return Scaffold(
      appBar: AppBar(
        title: const Text('录后镜像录制'),
        actions: [
          IconButton(
            icon: const Icon(Icons.close),
            onPressed: _closeCamera,
          ),
        ],
      ),
      body: Column(
        children: [
          Row(
            children: [
              const Text('镜像预览: '),
              Switch(
                value: _mirrorEnabled,
                onChanged: (value) async {
                  setState(() => _mirrorEnabled = value);
                  await _cameraPlatform.setMirrorPreview(_cameraId!, value);
                },
              ),
              Text(_mirrorEnabled ? '开启' : '关闭'),
            ],
          ),
          Expanded(
            child: Center(
              child: _cameraId != null && _previewW > 0 && _previewH > 0
                  ? AspectRatio(
                      aspectRatio: _previewW / _previewH,
                      child: _cameraPlatform.buildPreview(_cameraId!),
                    )
                  : _cameraId != null
                      ? _cameraPlatform.buildPreview(_cameraId!)
                      : const SizedBox(),
            ),
          ),
        ],
      ),
      floatingActionButton: FloatingActionButton(
        onPressed: _toggleRecording,
        backgroundColor: _isRecording ? Colors.red : Colors.blue,
        child: Icon(_isRecording ? Icons.stop : Icons.fiber_manual_record),
      ),
      bottomSheet: _outputVideoPath != null
          ? Padding(
              padding: const EdgeInsets.all(8.0),
              child: Text('视频已保存：\n$_outputVideoPath'),
            )
          : null,
    );
  }
}
