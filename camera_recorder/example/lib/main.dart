import 'package:camera_recorder/camera_recorder.dart';
import 'package:flutter/material.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  CameraWindowsRecorder.registerWith();
  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({super.key});

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  final CameraWindowsRecorder _recorder = CameraWindowsRecorder();
  int? _cameraId;
  String _status = '正在初始化…';
  bool _isRecording = false;

  @override
  void initState() {
    super.initState();
    _initCamera();
  }

  // 初始化摄像头
  Future<void> _initCamera() async {
    try {
      final cameras = await _recorder.availableCameras();
      if (cameras.isEmpty) {
        setState(() => _status = '未检测到摄像头');
        return;
      }

      final cameraId = await _recorder.createCameraWithSettings(
        cameras.first,
        const MediaSettings(
          resolutionPreset: ResolutionPreset.medium,
          videoWidth: 640,
          videoHeight: 480,
          fps: 30,
          enableAudio: false,
        ),
      );
      await _recorder.initializeCamera(cameraId);

      if (!mounted) return;
      setState(() {
        _cameraId = cameraId;
        _status = '已就绪：${cameras.first.name}（640×480 直录）';
      });
    } on CameraException catch (e) {
      if (!mounted) return;
      setState(() => _status = '摄像头错误：${e.description ?? e.code}');
    } catch (e) {
      if (!mounted) return;
      setState(() => _status = '初始化失败：$e');
    }
  }

  Future<void> _toggleRecording() async {
    final cameraId = _cameraId;
    if (cameraId == null) return;

    try {
      if (_isRecording) {
        final file = await _recorder.stopVideoRecording(cameraId);
        if (!mounted) return;
        setState(() {
          _isRecording = false;
          _status = '录像已保存：${file.path}';
        });
      } else {
        final path =
            'D:/VideoFiles/recording_${DateTime.now().millisecondsSinceEpoch}.mp4';
        await _recorder.startVideoRecording(cameraId, filePath: path);
        if (!mounted) return;
        setState(() {
          _isRecording = true;
          _status = '录制中（640×480）…';
        });
      }
    } on CameraException catch (e) {
      if (!mounted) return;
      setState(() => _status = '录制失败：${e.description ?? e.code}');
    }
  }

  @override
  void dispose() {
    final cameraId = _cameraId;
    if (cameraId != null) {
      _recorder.dispose(cameraId);
    }
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(
          title: const Text('Camera Recorder Example'),
        ),
        body: Column(
          children: [
            Expanded(
              child: _cameraId != null
                  ? _recorder.buildPreview(_cameraId!)
                  : Center(child: Text(_status)),
            ),
            Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                children: [
                  Text(_status, textAlign: TextAlign.center),
                  const SizedBox(height: 12),
                  if (_cameraId != null)
                    ElevatedButton(
                      onPressed: _toggleRecording,
                      child: Text(_isRecording ? '停止录制' : '开始录制'),
                    ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}
