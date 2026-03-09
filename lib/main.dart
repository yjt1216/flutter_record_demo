
import 'package:camera_recorder/camera_recorder.dart';
import 'package:flutter/material.dart';
import 'pages/audio_player_page.dart';

void main() {
  // 注册 camera_windows_recorder 插件
  CameraWindowsRecorder.registerWith();
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  // This widget is the root of your application.
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Flutter Demo',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.deepPurple),
      ),
      home: const MyHomePage(title: 'Flutter Demo Home Page'),
    );
  }
}

class MyHomePage extends StatefulWidget {
  const MyHomePage({super.key, required this.title});

  // This widget is the home page of your application. It is stateful, meaning
  // that it has a State object (defined below) that contains fields that affect
  // how it looks.

  // This class is the configuration for the state. It holds the values (in this
  // case the title) provided by the parent (in this case the App widget) and
  // used by the build method of the State. Fields in a Widget subclass are
  // always marked "final".

  final String title;

  @override
  State<MyHomePage> createState() => _MyHomePageState();
}

class _MyHomePageState extends State<MyHomePage> {
  final CameraPlatform _cameraPlatform = CameraPlatform.instance;
  List<CameraDescription> _cameras = <CameraDescription>[];
  bool _loading = false;
  CameraDescription? _selectedCamera;
  int? _cameraId; // 保存摄像头ID
  final TextEditingController _pathCtrl = TextEditingController(text: 'D:/VideoFiles/test_direct_save.mp4');
  bool _recording = false;
  bool _opened = false;
  double _fps = 30.0;
  int _previewW = 0;
  int _previewH = 0;
  bool _mirrorEnabled = false; // 默认镜像开启

  @override
  void initState() {
    super.initState();
    // 启动时默认查找摄像头
    _loadCameras();
  }

  Future<void> _loadCameras() async {
    setState(() { _loading = true; });
    try {
      final cams = await _cameraPlatform.availableCameras();
      setState(() {
        _cameras = cams;
        if (_cameras.isNotEmpty) {
          _selectedCamera = _cameras.first;
        }
      });
    } catch (e) {
      if (!mounted) return;
      debugPrint("加载摄像头失败: $e");
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('加载摄像头失败: $e')));
    } finally {
      if (mounted) setState(() { _loading = false; });
    }
  }

  Future<void> _openCamera() async {
    // 若尚未查找或没有选择，先尝试查找一次
    if (_cameras.isEmpty) {
      await _loadCameras();
      if (_cameras.isEmpty) {
        if (mounted) {
          debugPrint("未找到摄像头");
          ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('未找到摄像头')));
        }
        return;
      }
    }
    if (_selectedCamera == null && _cameras.isNotEmpty) {
      _selectedCamera = _cameras.first;
    }
    if (_selectedCamera == null) {
      if (mounted) {
        debugPrint("未选择摄像头");
        ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('未选择摄像头')));
      }
      return;
    }
    try {
      // 使用 CameraPlatform 创建摄像头实例，设置640×480分辨率
      final cameraId = await _cameraPlatform.createCameraWithSettings(
        _selectedCamera!,
        MediaSettings(
          resolutionPreset: ResolutionPreset.medium, // 或者使用其他预设
          fps: 30,
          videoBitrate: 2000000, // 2Mbps
          enableAudio: false,
        ),
      );

      if (mounted) {
        setState(() { 
          _opened = true; 
          _cameraId = cameraId;
        });
        debugPrint("摄像头已打开，ID: $cameraId");
        ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('摄像头已打开')));
      }
      {
        // 初始化摄像头
        await _cameraPlatform.initializeCamera(cameraId);
        await _cameraPlatform.setMirrorPreview(_cameraId!, false);
        // 使用 buildPreview 获取预览纹理
        if (mounted) {
          setState(() { 
            // 使用 cameraId 作为 textureId
            _previewW = 640; 
            _previewH = 480; 
          });
          // 设置默认 FPS 为 30
          if (mounted) setState(() { _fps = 30.0; });
        }
        
        // 简单轮询 FPS 3 秒
        for (int i = 0; i < 30 && mounted && _opened; i++) {
          // 注意：CameraPlatform 可能没有直接的 FPS 获取方法
          // 这里暂时跳过 FPS 更新
          await Future.delayed(const Duration(milliseconds: 100));
        }
      }
    } catch (e) {
      if (!mounted) return;
      debugPrint("打开摄像头异常: $e");
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('打开摄像头异常: $e')));
    }
  }

  Future<void> _startRecording() async {
    if (_cameraId == null) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('请先打开摄像头')));
      }
      return;
    }
    
    final path = _pathCtrl.text.trim();
    if (path.isEmpty) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('请输入保存路径')));
      }
      return;
    }
    
    try {
      // 使用 CameraPlatform，支持指定路径
      final path = _pathCtrl.text.trim();
      await _cameraPlatform.startVideoRecording(_cameraId!, filePath: path);
      if (mounted) setState(() { _recording = true; });
      if (mounted) {
        debugPrint("开始录像，摄像头ID: $_cameraId，保存路径: $path");
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('开始录像，保存到: $path')));
      }
    } catch (e) {
      if (mounted) {
        debugPrint("开始录像失败: $e");
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('开始录像失败: $e')));
      }
    }
  }

  Future<void> _stopRecording() async {
    if (_cameraId == null) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('没有正在录制的视频')));
      }
      return;
    }
    
    try {
      // 使用 CameraPlatform
      final videoFile = await _cameraPlatform.stopVideoRecording(_cameraId!);
      if (mounted) setState(() { _recording = false; });
      if (mounted) {
        debugPrint("停止录像，保存到: ${videoFile.path}");
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('录像已保存到: ${videoFile.path}')));
      }
    } catch (e) {
      if (mounted) {
        debugPrint("停止录像失败: $e");
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('停止录像失败: $e')));
      }
    }
  }

  Future<void> _takePicture() async {
    if (_cameraId == null) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('请先打开摄像头')));
      }
      return;
    }

    final rawPath = _pathCtrl.text.trim();
    String picturePath = 'D:/VideoFiles/';
    if (rawPath.isNotEmpty) {
      final dotIndex = rawPath.lastIndexOf('.');
      picturePath =
          dotIndex == -1 ? '$rawPath.jpg' : '${rawPath.substring(0, dotIndex)}.jpg';
    }

    try {
      final picture =
          await _cameraPlatform.takePicture(_cameraId!, filePath: picturePath.isEmpty ? null : picturePath);
      if (mounted) {
        ScaffoldMessenger.of(context)
            .showSnackBar(SnackBar(content: Text('截图已保存到: ${picture.path}')));
      }
    } catch (e) {
      if (mounted) {
        debugPrint('截图失败: $e');
        ScaffoldMessenger.of(context)
            .showSnackBar(SnackBar(content: Text('截图失败: $e')));
      }
    }
  }

  Future<void> _closeCamera() async {
    if (_cameraId != null) {
      try {
        await _cameraPlatform.dispose(_cameraId!);
        debugPrint("摄像头已关闭，ID: $_cameraId");
      } catch (e) {
        debugPrint("关闭摄像头异常: $e");
      }
    }
    if (mounted) {
      setState(() { 
        _opened = false; 
        _cameraId = null;
      });
      debugPrint("摄像头已关闭");
      ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('摄像头已关闭')));
    }
  }

  @override
  Widget build(BuildContext context) {
    // This method is rerun every time setState is called, for instance as done
    // by the _incrementCounter method above.
    //
    // The Flutter framework has been optimized to make rerunning build methods
    // fast, so that you can just rebuild anything that needs updating rather
    // than having to individually change instances of widgets.
    return Scaffold(
      appBar: AppBar(
        // TRY THIS: Try changing the color here to a specific color (to
        // Colors.amber, perhaps?) and trigger a hot reload to see the AppBar
        // change color while the other colors stay the same.
        backgroundColor: Theme.of(context).colorScheme.inversePrimary,
        // Here we take the value from the MyHomePage object that was created by
        // the App.build method, and use it to set our appbar title.
        title: Text(widget.title),
      ),
      body: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            ElevatedButton(
              onPressed: () {
                Navigator.of(context).push(
                  MaterialPageRoute(
                    builder: (_) => const AudioPlayerPage(),
                  ),
                );
              },
              child: const Text('打开音频播放（assets/common/countdown10s.mp3）'),
            ),
            const SizedBox(height: 12),
            Row(
              children: [
                 ElevatedButton.icon(
                    onPressed: _loading ? null : _loadCameras,
                    icon: const Icon(Icons.search),
                    label: const Text('查找摄像头'),
                  ),
                  const SizedBox(width: 12),
                  if (_loading) const SizedBox(width: 18, height: 18, child: CircularProgressIndicator(strokeWidth: 2)),
                ],
              ),
              const SizedBox(height: 16),
              if (_cameras.isNotEmpty)
                DropdownButton<CameraDescription>(
                  value: _selectedCamera,
                  items: _cameras.map((cam) {
                    return DropdownMenuItem<CameraDescription>(
                      value: cam,
                      child: Text(cam.name),
                    );
                  }).toList(),
                  onChanged: (v) => setState(() { _selectedCamera = v; }),
                ),
              const SizedBox(height: 12),
              TextField(
                controller: _pathCtrl,
                decoration: const InputDecoration(
                  labelText: '保存路径',
                  hintText: '例如: C:/Users/Public/Videos/demo.mp4',
                ),
              ),
              const SizedBox(height: 12),
              Wrap(spacing: 8, runSpacing: 8, children: [
                ElevatedButton(onPressed: _openCamera, child: const Text('打开摄像头')),
                ElevatedButton(onPressed: _opened && !_recording ? _startRecording : null, child: const Text('开始录像')),
                ElevatedButton(onPressed: _recording ? _stopRecording : null, child: const Text('停止录像')),
                ElevatedButton(onPressed: _opened ? _takePicture : null, child: const Text('截图')),
                ElevatedButton(onPressed: _opened ? _closeCamera : null, child: const Text('关闭摄像头')),
              ]),
              const SizedBox(height: 8),
              Row(
                children: [
                  const Text('镜像预览: '),
                  Switch(
                    value: _mirrorEnabled,
                    onChanged: _opened ? (value) async {
                      setState(() {
                        _mirrorEnabled = value;
                      });
                      if (_cameraId != null) {
                        try {
                          await _cameraPlatform.setMirrorPreview(_cameraId!, value);
                          if (mounted) {
                            ScaffoldMessenger.of(context).showSnackBar(
                              SnackBar(content: Text(value ? '已开启镜像' : '已关闭镜像')),
                            );
                          }
                        } catch (e) {
                          if (mounted) {
                            debugPrint("设置镜像失败: $e");
                            ScaffoldMessenger.of(context).showSnackBar(
                              SnackBar(content: Text('设置镜像失败: $e')),
                            );
                          }
                        }
                      }
                    } : null,
                  ),
                  const SizedBox(width: 8),
                  Text(_mirrorEnabled ? '开启' : '关闭'),
                ],
              ),
              const SizedBox(height: 8),
              Row(children: [
                const Text('状态: '),
                Text(_opened ? '已打开' : '未打开'),
                const SizedBox(width: 12),
                Text(_recording ? '录制中' : '未录制'),
                const SizedBox(width: 12),
                Text('FPS: ${_fps.toStringAsFixed(0)}'),
              ]),
              const SizedBox(height: 16),
              Expanded(
                child: Center(
                  child: _cameraId != null && _opened && _previewW > 0 && _previewH > 0
                      ? AspectRatio(
                          aspectRatio: _previewW / _previewH,
                          child: _cameraPlatform.buildPreview(_cameraId!),
                        )
                      : _cameraId != null && _opened
                          ? _cameraPlatform.buildPreview(_cameraId!)
                          : const Text('预览区域（待实现）'),
                ),
              ),
            ],
          ),
        ),
    );
  }
}
