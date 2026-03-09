# 开发指南

本文档为camera_recorder插件的开发者提供详细的开发指南。

## 项目结构

```
camera_recorder/
├── lib/                                    # Dart代码
│   ├── camera_window_recorder.dart        # 主要实现类
│   ├── camera_platform_interface/         # 平台接口定义
│   │   ├── camera_platform_interface.dart # 接口导出文件
│   │   └── src/
│   │       ├── platform_interface/        # 平台接口基类
│   │       ├── method_channel/            # 方法通道实现
│   │       ├── events/                    # 事件定义
│   │       └── types/                     # 类型定义
│   └── src/
│       └── messages.g.dart                # Pigeon生成的通信代码
├── windows/                               # Windows原生实现
│   ├── camera_recorder_plugin_c_api.cpp  # C API包装器（Flutter入口）
│   ├── camera_recorder_plugin_c_api.h    # C API头文件
│   ├── camera_plugin.cpp                  # 摄像头插件主实现
│   ├── camera_plugin.h                    # 摄像头插件头文件
│   ├── capture_controller.cpp             # 摄像头控制器
│   ├── capture_controller.h               # 摄像头控制器头文件
│   ├── camera.cpp                         # 摄像头实现
│   ├── camera.h                           # 摄像头头文件
│   ├── record_handler.cpp                 # 录制处理器
│   ├── preview_handler.cpp                # 预览处理器
│   ├── photo_handler.cpp                  # 拍照处理器
│   ├── texture_handler.cpp                # 纹理处理器
│   ├── messages.g.cpp                     # Pigeon生成的C++代码
│   ├── messages.g.h                       # Pigeon生成的头文件
│   ├── CMakeLists.txt                     # CMake构建配置
│   └── include/                           # 头文件目录
├── example/                               # 示例应用
│   ├── lib/main.dart                      # 示例主文件
│   ├── integration_test/                  # 集成测试
│   └── pubspec.yaml                       # 示例依赖配置
├── test/                                  # 测试文件
│   └── camera_recorder_test.dart          # 单元测试
├── pigeons/                               # Pigeon配置
│   └── messages.dart                      # 消息定义
├── pubspec.yaml                           # 插件依赖配置
├── README.md                              # 用户文档
├── API.md                                 # API文档
├── CHANGELOG.md                           # 更新日志
└── DEVELOPMENT.md                         # 开发指南（本文件）
```

## 开发环境设置

### 必需工具

1. **Flutter SDK** (3.3.0+)
2. **Visual Studio 2022** (Community/Professional/Enterprise)
3. **Windows 10 SDK** (10.0.19041.0+)
4. **CMake** (3.16+)
5. **Git**

### 环境配置

1. 安装Flutter SDK并配置环境变量
2. 安装Visual Studio 2022，确保包含以下组件：
   - C++ CMake tools for Visual Studio
   - Windows 10/11 SDK
   - MSVC v143编译器工具集
3. 安装Git并配置

### 项目设置

```bash
# 克隆项目
git clone <repository-url>
cd camera_recorder

# 获取依赖
flutter pub get

# 生成Pigeon代码
flutter packages pub run pigeon --input pigeons/messages.dart
```

## 代码架构

### Dart层

#### CameraWindowsRecorder
主要的摄像头控制器类，继承自`CameraPlatform`。

**职责**:
- 管理摄像头生命周期
- 处理Flutter与原生代码通信
- 提供事件流
- 实现平台接口

**关键方法**:
```dart
class CameraWindowsRecorder extends CameraPlatform {
  // 摄像头管理
  Future<List<CameraDescription>> availableCameras();
  Future<int> createCamera(CameraDescription, ResolutionPreset?, {bool enableAudio});
  Future<void> initializeCamera(int cameraId);
  Future<void> dispose(int cameraId);
  
  // 预览和录制
  Widget buildPreview(int cameraId);
  Future<void> startVideoRecording(int cameraId, {String? filePath});
  Future<XFile> stopVideoRecording(int cameraId);
  Future<XFile> takePicture(int cameraId);
  
  // 事件流
  Stream<CameraInitializedEvent> onCameraInitialized(int cameraId);
  Stream<CameraErrorEvent> onCameraError(int cameraId);
  Stream<VideoRecordedEvent> onVideoRecordedEvent(int cameraId);
}
```

#### 平台接口
定义统一的摄像头平台接口，支持不同平台的实现。

### C++层

#### C API包装器 (camera_recorder_plugin_c_api.cpp)
Flutter插件系统的入口点，提供C风格API接口。

**关键功能**:
- Flutter插件注册入口
- C API到C++实现的桥接
- 插件生命周期管理

#### 摄像头控制器 (camera_plugin.cpp)
基于Windows Media Foundation的摄像头控制实现。

**关键功能**:
- 方法通道处理
- Pigeon API实现
- 摄像头枚举和初始化
- 预览流管理
- 录制控制

## 通信机制

### Pigeon
使用Pigeon进行类型安全的Flutter与原生代码通信。

#### 消息定义 (pigeon/messages.dart)
```dart
@HostApi()
abstract class CameraApi {
  Future<List<String?>> getAvailableCameras();
  Future<int> create(String cameraName, PlatformMediaSettings settings);
  Future<void> initialize(int cameraId);
  Future<void> dispose(int cameraId);
  Future<PlatformSize> getPreviewSize(int cameraId);
  Future<void> startVideoRecording(int cameraId, String filePath);
  Future<String> stopVideoRecording(int cameraId);
  Future<String> takePicture(int cameraId);
  // ... 更多方法
}
```

#### 生成代码
```bash
flutter packages pub run pigeon --input pigeons/messages.dart
```

## 构建系统

### CMake配置
Windows平台使用CMake进行构建。

**关键文件**: `windows/CMakeLists.txt`

```cmake
# 插件配置
add_library(camera_recorder_plugin SHARED
  camera_recorder_plugin.cpp
  capture_controller.cpp
  camera.cpp
  # ... 其他源文件
)

# 链接库
target_link_libraries(camera_recorder_plugin
  PRIVATE
  flutter
  flutter_wrapper_plugin
  mf
  mfplat
  mfreadwrite
  mfuuid
  # ... 其他库
)
```

### 构建命令
```bash
# 构建插件
flutter build windows

# 运行示例
flutter run -d windows

# 运行测试
flutter test
```

## 开发工作流

### 1. 功能开发

1. **定义接口** (如果需要)
   - 在`camera_platform_interface`中定义新接口
   - 更新相关类型定义

2. **实现Dart层**
   - 在`CameraWindowsRecorder`中实现新功能
   - 添加必要的错误处理

3. **更新Pigeon消息** (如果需要)
   - 在`pigeon/messages.dart`中添加新方法
   - 重新生成代码

4. **实现C++层**
   - 在相应的C++文件中实现功能
   - 确保错误处理正确

5. **添加测试**
   - 编写单元测试
   - 更新示例应用

### 2. 调试技巧

#### Dart层调试
```dart
// 启用详细日志
import 'package:flutter/services.dart';

ServicesBinding.instance.defaultBinaryMessenger.setMessageHandler(
  'camera_recorder',
  (data) async {
    print('Platform message: ${String.fromCharCodes(data!)}');
    return null;
  },
);
```

#### C++层调试
```cpp
// 使用OutputDebugString输出调试信息
#include <debugapi.h>

OutputDebugStringA("Debug message\n");
```

### 3. 测试策略

#### 单元测试
- 测试Dart层逻辑
- 模拟平台调用
- 验证错误处理

#### 集成测试
- 测试完整的摄像头功能
- 验证文件输出
- 测试资源清理

#### 手动测试
- 在不同摄像头设备上测试
- 测试各种分辨率设置
- 验证长时间录制稳定性

## 性能优化

### 内存管理
- 及时释放摄像头资源
- 避免内存泄漏
- 合理管理纹理资源

### 录制优化
- 选择合适的编码参数
- 优化文件I/O操作
- 减少不必要的内存拷贝

### 预览优化
- 使用硬件加速
- 优化纹理更新频率
- 减少CPU使用

## 错误处理

### 常见错误类型

1. **摄像头访问错误**
   - 权限不足
   - 设备被占用
   - 驱动问题

2. **录制错误**
   - 磁盘空间不足
   - 文件路径无效
   - 编码器错误

3. **初始化错误**
   - Media Foundation未初始化
   - 摄像头设备不可用
   - 参数无效

### 错误处理策略

```dart
try {
  await recorder.initializeCamera(cameraId);
} on CameraException catch (e) {
  // 处理摄像头特定错误
  print('Camera error: ${e.code} - ${e.description}');
} on PlatformException catch (e) {
  // 处理平台错误
  print('Platform error: ${e.code} - ${e.message}');
} catch (e) {
  // 处理其他错误
  print('Unexpected error: $e');
}
```

## 发布流程

### 1. 版本准备
1. 更新版本号
2. 更新CHANGELOG.md
3. 运行所有测试
4. 更新文档

### 2. 代码审查
1. 创建Pull Request
2. 代码审查
3. 修复问题
4. 合并代码

### 3. 发布
1. 创建Git标签
2. 发布到pub.dev（如果适用）
3. 更新文档网站

## 贡献指南

### 代码规范
- 遵循Dart官方代码规范
- 使用有意义的变量和函数名
- 添加必要的注释
- 保持代码简洁

### 提交规范
- 使用清晰的提交信息
- 一个提交只做一件事
- 包含必要的测试

### 文档更新
- 更新相关文档
- 添加使用示例
- 更新API文档

## 故障排除

### 常见问题

1. **构建失败**
   - 检查Visual Studio安装
   - 验证CMake配置
   - 清理构建缓存

2. **运行时错误**
   - 检查权限设置
   - 验证摄像头可用性
   - 查看错误日志

3. **性能问题**
   - 检查资源使用
   - 优化录制参数
   - 减少不必要的操作

### 调试工具
- Visual Studio调试器
- Flutter Inspector
- Windows事件查看器
- 性能分析工具

## 参考资料

- [Flutter插件开发指南](https://flutter.dev/docs/development/packages-and-plugins/developing-packages)
- [Windows Media Foundation文档](https://docs.microsoft.com/en-us/windows/win32/medfound/)
- [Pigeon文档](https://pub.dev/packages/pigeon)
- [CMake文档](https://cmake.org/documentation/)
