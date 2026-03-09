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
