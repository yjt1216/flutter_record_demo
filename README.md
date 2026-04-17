# Flutter Record Demo

一个基于Flutter的Windows摄像头录制演示应用，集成了自定义的camera_recorder插件。

先修改调试 该demo中摄像插件，功能完善后 提取拷贝插件

## 项目概述

本项目展示了如何在Flutter Windows应用中实现摄像头录制功能，包括：
- 🎥 实时摄像头预览
- 📹 高质量视频录制
- 📸 快速拍照功能
- 🔧 摄像头设备管理

## 技术架构

### 插件架构
```
Flutter应用 (本项目)
    ↓
CameraWindowsRecorder (Dart层)
    ↓ (通过Pigeon通信)
CameraPlugin (C++层)
    ↓
Windows Media Foundation API
```

### 核心组件
- **camera_recorder插件**: 自定义的Windows摄像头录制插件
- **CameraWindowsRecorder**: Dart层的主要API类
- **CameraPlugin**: C++层的实际实现类

## 快速开始

### 环境要求
- Flutter SDK 3.3.0+
- Visual Studio 2022
- Windows 10/11

### 运行应用
```bash
# 获取依赖
flutter pub get

# 运行Windows应用
flutter run -d windows

# 构建Windows应用
flutter build windows
```

## 使用示例

下面示例基于本仓库的 `camera_recorder` 插件（Windows）。

### 1) MP4(H264) 录制（建议高码率，接近系统相机）

> 关键点：**`videoBitrate` 直接影响画质**。系统相机常见 1080p@30fps 约 15–20Mbps；如果用 2Mbps 很容易出现明显的压缩糊/“蒙雾”观感。
>
> 经验结论（本项目/部分设备）：**分辨率预设建议至少 `ResolutionPreset.veryHigh`**；`veryHigh` 以下可能出现“蒙雾/发灰/观感变差”。

```dart
import 'dart:io' show Directory, Platform;

import 'package:camera_recorder/camera_recorder.dart';

final CameraPlatform camera = CameraPlatform.instance;

Future<void> recordMp4HighBitrate() async {
  final cams = await camera.availableCameras();
  final id = await camera.createCameraWithSettings(
    cams.first,
    MediaSettings(
      resolutionPreset: ResolutionPreset.veryHigh, // 优先 1080p
      fps: 30,
      videoBitrate: 17300000, // 约 17.3Mbps
      enableAudio: false,
    ),
  );

  await camera.initializeCamera(id);

  // 输出到 D:\VideoFiles\record_<timestamp>.mp4
  final ts = DateTime.now().millisecondsSinceEpoch;
  final path = Platform.isWindows
      ? (await Directory(r'D:\VideoFiles').create(recursive: true),
          r'D:\VideoFiles\record_' + ts.toString() + '.mp4').$2
      : 'record_$ts.mp4';

  await camera.startVideoRecording(id, filePath: path);
  // ... wait ...
  final xfile = await camera.stopVideoRecording(id);
  print('saved: ${xfile.path}');
}
```

### 2) Raw(BGRA) 无压缩录制（交给外部调用者自行编码/压缩）

> 关键点：输出是 **`.bgra/.raw`** 原始帧文件（体积很大），旁边会生成同名 **`.json`** 元信息文件。

```dart
import 'dart:io' show Directory;
import 'package:camera_recorder/camera_recorder.dart';

final CameraPlatform camera = CameraPlatform.instance;

Future<void> recordRawBgra() async {
  final cams = await camera.availableCameras();
  final id = await camera.createCameraWithSettings(
    cams.first,
    const MediaSettings(
      resolutionPreset: ResolutionPreset.medium,
      fps: 30,
      enableAudio: false,
    ),
  );
  await camera.initializeCamera(id);

  await Directory(r'D:\VideoFiles').create(recursive: true);
  final ts = DateTime.now().millisecondsSinceEpoch;
  final rawPath = r'D:\VideoFiles\record_' + ts.toString() + '.bgra';

  await camera.startVideoRecording(id, filePath: rawPath); // 触发 Raw 模式
  // ... wait ...
  final xfile = await camera.stopVideoRecording(id);
  print('raw saved: ${xfile.path}');
  print('meta saved: ${xfile.path}.json');
}
```

### 3) Demo 页面

- **首页录像页**：`lib/main.dart`
- **录后镜像录制页**：`lib/pages/mirror_after_record_page.dart`

## 项目结构

```
flutter_record_demo/
├── lib/
│   └── main.dart                    # 主应用文件
├── camera_recorder/                 # 自定义摄像头插件
│   ├── lib/                        # Dart代码
│   ├── windows/                    # Windows原生实现
│   ├── example/                    # 插件示例
│   └── test/                       # 插件测试
├── windows/                        # Flutter Windows配置
└── build/                          # 构建输出
```

## 功能特性

- ✅ 摄像头设备枚举
- ✅ 实时预览显示
- ✅ 视频录制（MP4格式）
- ✅ 照片拍摄
- ✅ 多种分辨率支持
- ✅ 音频录制支持
- ✅ 错误处理和资源管理

## 文档

- [插件README](camera_recorder/README.md) - 详细的插件使用说明
- [API文档](camera_recorder/API.md) - 完整的API参考
- [开发指南](camera_recorder/DEVELOPMENT.md) - 开发者文档
- [更新日志](camera_recorder/CHANGELOG.md) - 版本更新记录

## 许可证

本项目采用MIT许可证。详见[LICENSE](LICENSE)文件。
