# Camera Recorder Plugin

一个专为Windows平台设计的Flutter摄像头录制插件，基于Windows Media Foundation API实现。

## 功能特性

- 🎥 **摄像头预览** - 实时显示摄像头画面
- 📹 **视频录制** - 支持高质量视频录制
- 📸 **拍照功能** - 快速拍摄照片
- 🔧 **摄像头管理** - 自动检测和管理可用摄像头
- ⚡ **高性能** - 基于Windows Media Foundation，性能优异
- 🎯 **易于集成** - 简单的API设计，快速集成到Flutter应用
- 🎬 **30fps帧速率支持** - 优先选择30fps帧速率录制

## 平台支持

- ✅ Windows 10/11 (x64)
- ❌ Android (暂不支持)
- ❌ iOS (暂不支持)
- ❌ macOS (暂不支持)
- ❌ Linux (暂不支持)

## 安装

在您的`pubspec.yaml`文件中添加依赖：

```yaml
dependencies:
  camera_recorder:
    path: ./camera_recorder  # 本地路径
```

然后运行：

```bash
flutter pub get
```

## 快速开始

### 1. 基本设置

```dart
import 'package:camera_recorder/camera_window_recorder.dart';

void main() {
  // 注册插件
  CameraWindowsRecorder.registerWith();
  runApp(MyApp());
}
```

### 2. 获取可用摄像头

```dart
final recorder = CameraWindowsRecorder();
final cameras = await recorder.availableCameras();
print('可用摄像头数量: ${cameras.length}');
```

### 3. 创建摄像头实例

```dart
final cameraDescription = cameras.first;
final cameraId = await recorder.createCamera(
  cameraDescription,
  ResolutionPreset.high,
  enableAudio: true,
);
```

### 4. 初始化摄像头

```dart
await recorder.initializeCamera(cameraId);
```

### 5. 显示预览

```dart
Widget buildPreview() {
  return recorder.buildPreview(cameraId);
}
```

### 6. 开始录制

```dart
await recorder.startVideoRecording(
  cameraId,
  filePath: 'D:/Videos/my_recording.mp4',
);
```

### 7. 停止录制

```dart
final videoFile = await recorder.stopVideoRecording(cameraId);
print('视频保存到: ${videoFile.path}');
```

### 8. 拍照

```dart
final photoFile = await recorder.takePicture(cameraId);
print('照片保存到: ${photoFile.path}');
```

### 9. 释放资源

```dart
await recorder.dispose(cameraId);
```

## 完整示例

```dart
import 'package:flutter/material.dart';
import 'package:camera_recorder/camera_window_recorder.dart';

class CameraApp extends StatefulWidget {
  @override
  _CameraAppState createState() => _CameraAppState();
}

class _CameraAppState extends State<CameraApp> {
  CameraWindowsRecorder? _recorder;
  int? _cameraId;
  List<CameraDescription> _cameras = [];
  bool _isRecording = false;

  @override
  void initState() {
    super.initState();
    _recorder = CameraWindowsRecorder();
    _initializeCamera();
  }

  Future<void> _initializeCamera() async {
    try {
      // 获取可用摄像头
      final cameras = await _recorder!.availableCameras();
      if (cameras.isNotEmpty) {
        setState(() {
          _cameras = cameras;
        });
        
        // 创建摄像头实例
        final cameraId = await _recorder!.createCamera(
          cameras.first,
          ResolutionPreset.high,
          enableAudio: true,
        );
        
        // 初始化摄像头
        await _recorder!.initializeCamera(cameraId);
        
        setState(() {
          _cameraId = cameraId;
        });
      }
    } catch (e) {
      print('摄像头初始化失败: $e');
    }
  }

  Future<void> _startRecording() async {
    if (_cameraId != null) {
      await _recorder!.startVideoRecording(
        _cameraId!,
        filePath: 'D:/Videos/recording_${DateTime.now().millisecondsSinceEpoch}.mp4',
      );
      setState(() {
        _isRecording = true;
      });
    }
  }

  Future<void> _stopRecording() async {
    if (_cameraId != null && _isRecording) {
      final videoFile = await _recorder!.stopVideoRecording(_cameraId!);
      setState(() {
        _isRecording = false;
      });
      print('视频已保存: ${videoFile.path}');
    }
  }

  Future<void> _takePicture() async {
    if (_cameraId != null) {
      final photoFile = await _recorder!.takePicture(_cameraId!);
      print('照片已保存: ${photoFile.path}');
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('Camera Recorder'),
      ),
      body: Column(
        children: [
          // 摄像头预览
          if (_cameraId != null)
            Expanded(
              child: _recorder!.buildPreview(_cameraId!),
            ),
          
          // 控制按钮
          Padding(
            padding: EdgeInsets.all(16.0),
            child: Row(
              mainAxisAlignment: MainAxisAlignment.spaceEvenly,
              children: [
                ElevatedButton(
                  onPressed: _takePicture,
                  child: Text('拍照'),
                ),
                ElevatedButton(
                  onPressed: _isRecording ? _stopRecording : _startRecording,
                  child: Text(_isRecording ? '停止录制' : '开始录制'),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  @override
  void dispose() {
    if (_cameraId != null) {
      _recorder!.dispose(_cameraId!);
    }
    super.dispose();
  }
}
```

## API 参考

### CameraWindowsRecorder

主要的摄像头录制类。

#### 方法

##### `registerWith()`
注册插件到Flutter平台。

##### `availableCameras()`
获取可用的摄像头列表。
- **返回**: `Future<List<CameraDescription>>`

##### `createCamera(cameraDescription, resolutionPreset, {enableAudio})`
创建摄像头实例。
- **参数**:
  - `cameraDescription`: 摄像头描述
  - `resolutionPreset`: 分辨率预设
  - `enableAudio`: 是否启用音频录制
- **返回**: `Future<int>` - 摄像头ID

##### `initializeCamera(cameraId, {imageFormatGroup})`
初始化摄像头。
- **参数**:
  - `cameraId`: 摄像头ID
  - `imageFormatGroup`: 图像格式组

##### `buildPreview(cameraId)`
构建摄像头预览Widget。
- **参数**: `cameraId` - 摄像头ID
- **返回**: `Widget`

##### `startVideoRecording(cameraId, {maxVideoDuration, filePath})`
开始视频录制。
- **参数**:
  - `cameraId`: 摄像头ID
  - `maxVideoDuration`: 最大录制时长（已弃用）
  - `filePath`: 保存路径

##### `stopVideoRecording(cameraId)`
停止视频录制。
- **参数**: `cameraId` - 摄像头ID
- **返回**: `Future<XFile>` - 录制的视频文件

##### `takePicture(cameraId)`
拍摄照片。
- **参数**: `cameraId` - 摄像头ID
- **返回**: `Future<XFile>` - 拍摄的照片文件

##### `dispose(cameraId)`
释放摄像头资源。
- **参数**: `cameraId` - 摄像头ID

### 事件流

#### `onCameraInitialized(cameraId)`
摄像头初始化完成事件。

#### `onCameraError(cameraId)`
摄像头错误事件。

#### `onVideoRecordedEvent(cameraId)`
视频录制完成事件。

## 配置选项

### 分辨率预设

- `ResolutionPreset.low` - 低分辨率
- `ResolutionPreset.medium` - 中等分辨率（优先选择30fps）
- `ResolutionPreset.high` - 高分辨率
- `ResolutionPreset.veryHigh` - 很高分辨率
- `ResolutionPreset.ultraHigh` - 超高分辨率
- `ResolutionPreset.max` - 最大分辨率

### 30fps 帧速率支持

插件特别优化了对 30fps 帧速率的支持：

- **优先选择**：当摄像头支持 30fps 时，插件会优先选择该帧速率
- **自动回退**：如果摄像头不支持 30fps，会自动选择最接近的帧速率
- **向后兼容**：不影响其他帧速率设置的正常工作

#### 使用示例
```dart
// 创建摄像头实例，优先选择30fps帧速率
final cameraId = await recorder.createCameraWithSettings(
  cameraDescription,
  MediaSettings(
    resolutionPreset: ResolutionPreset.medium, // 对应480p高度
    fps: 30, // 指定30fps帧速率
    videoBitrate: 2000000, // 2Mbps
    enableAudio: false,
  ),
);
```

### 媒体设置

```dart
final mediaSettings = MediaSettings(
  resolutionPreset: ResolutionPreset.high,
  enableAudio: true,
  fps: 30,
  videoBitrate: 5000000, // 5 Mbps
  audioBitrate: 128000,  // 128 kbps
);
```

## 权限要求

### Windows

确保您的应用具有以下权限：

1. **摄像头访问权限** - 在Windows设置中允许应用访问摄像头
2. **麦克风权限** - 如果启用音频录制
3. **文件写入权限** - 用于保存录制的视频和照片

## 故障排除

### 常见问题

1. **摄像头无法初始化**
   - 检查摄像头是否被其他应用占用
   - 确认Windows摄像头权限设置
   - 验证摄像头驱动是否正常

2. **录制失败**
   - 检查文件路径是否有效
   - 确认磁盘空间充足
   - 验证文件写入权限

3. **预览不显示**
   - 确认摄像头已正确初始化
   - 检查`buildPreview`方法调用
   - 验证摄像头ID是否正确

### 调试技巧

启用详细日志：

```dart
import 'package:flutter/services.dart';

// 监听平台消息
ServicesBinding.instance.defaultBinaryMessenger.setMessageHandler(
  'camera_recorder',
  (data) async {
    print('Platform message: ${String.fromCharCodes(data!)}');
    return null;
  },
);
```

## 开发信息

### 项目结构

```
camera_recorder/
├── lib/
│   ├── camera_window_recorder.dart      # 主要实现
│   ├── camera_platform_interface/       # 平台接口
│   └── src/
│       └── messages.g.dart             # 生成的通信代码
├── windows/                            # Windows原生实现
│   ├── camera_recorder_plugin.cpp      # 插件主文件
│   ├── capture_controller.cpp          # 摄像头控制
│   └── ...
├── example/                            # 示例应用
└── test/                              # 测试文件
```

### 技术栈

- **Flutter**: 跨平台UI框架
- **Windows Media Foundation**: 摄像头和媒体处理
- **Pigeon**: Flutter与原生代码通信
- **CMake**: Windows构建系统

## 架构说明

### 插件架构
```
Flutter应用
    ↓
CameraWindowsRecorder (Dart层)
    ↓ (通过Pigeon通信)
CameraPlugin (C++层)
    ↓
Windows Media Foundation API
```

### 插件注册流程
```
Flutter插件系统
    ↓
generated_plugin_registrant.cc
    ↓
CameraRecorderPluginCApiRegisterWithRegistrar() (C API包装器)
    ↓
CameraPlugin::RegisterWithRegistrar() (实际C++实现)
    ↓
CameraPlugin (摄像头功能实现)
```

### 核心文件说明
- **camera_recorder_plugin_c_api.cpp**: C API包装器，Flutter插件系统入口
- **camera_plugin.cpp**: 实际的摄像头功能实现
- **camera_window_recorder.dart**: Dart层的高级封装

## 许可证

本项目采用MIT许可证。详见[LICENSE](LICENSE)文件。

## 贡献

欢迎提交Issue和Pull Request来改进这个插件！

## 更新日志

### v0.0.1
- 初始版本
- 支持Windows平台摄像头录制
- 基本的拍照和录像功能
- 摄像头预览支持

---

**注意**: 这是一个Windows专用插件，不支持其他平台。如果您需要跨平台支持，请考虑使用官方的camera插件。