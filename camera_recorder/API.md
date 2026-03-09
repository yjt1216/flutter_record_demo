# Camera Recorder Plugin API 文档

## 概述

Camera Recorder Plugin 是一个专为Windows平台设计的Flutter摄像头录制插件，提供摄像头预览、视频录制和拍照功能。

## 架构说明

### 插件层次结构
```
Flutter应用
    ↓
CameraWindowsRecorder (Dart层)
    ↓ (通过Pigeon通信)
CameraPlugin (C++层)
    ↓
Windows Media Foundation API
```

### 核心组件
- **CameraWindowsRecorder**: Dart层的主要API类
- **CameraPlugin**: C++层的实际实现类
- **camera_recorder_plugin_c_api.cpp**: C API包装器，用于Flutter插件注册

## 核心类

### CameraWindowsRecorder

主要的摄像头录制控制器类。

#### 静态方法

##### `registerWith()`
```dart
static void registerWith()
```
注册插件到Flutter平台。必须在应用启动时调用。

#### 实例方法

##### 摄像头管理

**`availableCameras()`**
```dart
Future<List<CameraDescription>> availableCameras()
```
获取系统中所有可用的摄像头列表。

**`createCamera(cameraDescription, resolutionPreset, {enableAudio})`**
```dart
Future<int> createCamera(
  CameraDescription cameraDescription,
  ResolutionPreset? resolutionPreset, {
  bool enableAudio = false,
})
```
创建摄像头实例。
- `cameraDescription`: 摄像头描述信息
- `resolutionPreset`: 分辨率预设，可选
- `enableAudio`: 是否启用音频录制
- **返回**: 摄像头ID，用于后续操作

**`initializeCamera(cameraId, {imageFormatGroup})`**
```dart
Future<void> initializeCamera(
  int cameraId, {
  ImageFormatGroup imageFormatGroup = ImageFormatGroup.unknown,
})
```
初始化指定的摄像头。
- `cameraId`: 摄像头ID
- `imageFormatGroup`: 图像格式组

**`dispose(cameraId)`**
```dart
Future<void> dispose(int cameraId)
```
释放摄像头资源。

##### 预览功能

**`buildPreview(cameraId)`**
```dart
Widget buildPreview(int cameraId)
```
构建摄像头预览Widget。
- `cameraId`: 摄像头ID
- **返回**: 预览Widget

##### 录制功能

**`startVideoRecording(cameraId, {maxVideoDuration, filePath})`**
```dart
Future<void> startVideoRecording(
  int cameraId, {
  Duration? maxVideoDuration,
  String? filePath,
})
```
开始视频录制。
- `cameraId`: 摄像头ID
- `maxVideoDuration`: 最大录制时长（已弃用）
- `filePath`: 保存路径，默认为 `D:/VideoFiles/default_recording.mp4`

**`stopVideoRecording(cameraId)`**
```dart
Future<XFile> stopVideoRecording(int cameraId)
```
停止视频录制并返回录制的视频文件。
- `cameraId`: 摄像头ID
- **返回**: 录制的视频文件

**`takePicture(cameraId)`**
```dart
Future<XFile> takePicture(int cameraId)
```
拍摄照片。
- `cameraId`: 摄像头ID
- **返回**: 拍摄的照片文件

##### 事件监听

**`onCameraInitialized(cameraId)`**
```dart
Stream<CameraInitializedEvent> onCameraInitialized(int cameraId)
```
监听摄像头初始化完成事件。

**`onCameraError(cameraId)`**
```dart
Stream<CameraErrorEvent> onCameraError(int cameraId)
```
监听摄像头错误事件。

**`onVideoRecordedEvent(cameraId)`**
```dart
Stream<VideoRecordedEvent> onVideoRecordedEvent(int cameraId)
```
监听视频录制完成事件。

## 数据类型

### CameraDescription

摄像头描述信息。

```dart
class CameraDescription {
  final String name;                    // 摄像头名称
  final CameraLensDirection lensDirection;  // 镜头方向
  final int sensorOrientation;         // 传感器方向
}
```

### ResolutionPreset

分辨率预设枚举。

```dart
enum ResolutionPreset {
  low,        // 低分辨率
  medium,     // 中等分辨率
  high,       // 高分辨率
  veryHigh,   // 很高分辨率
  ultraHigh,  // 超高分辨率
  max,        // 最大分辨率
}
```

### CameraLensDirection

摄像头镜头方向。

```dart
enum CameraLensDirection {
  front,  // 前置摄像头
  back,   // 后置摄像头
  external, // 外置摄像头
}
```

### ImageFormatGroup

图像格式组。

```dart
enum ImageFormatGroup {
  unknown,
  yuv420,
  bgra8888,
  jpeg,
}
```

## 事件类型

### CameraInitializedEvent

摄像头初始化完成事件。

```dart
class CameraInitializedEvent extends CameraEvent {
  final int cameraId;
  final double previewWidth;
  final double previewHeight;
  final ExposureMode exposureMode;
  final bool exposurePointSupported;
  final FocusMode focusMode;
  final bool focusPointSupported;
}
```

### CameraErrorEvent

摄像头错误事件。

```dart
class CameraErrorEvent extends CameraEvent {
  final int cameraId;
  final String description;
}
```

### VideoRecordedEvent

视频录制完成事件。

```dart
class VideoRecordedEvent extends CameraEvent {
  final int cameraId;
  final String path;
}
```

## 使用示例

### 基本使用流程

```dart
// 1. 注册插件
CameraWindowsRecorder.registerWith();

// 2. 创建实例
final recorder = CameraWindowsRecorder();

// 3. 获取可用摄像头
final cameras = await recorder.availableCameras();

// 4. 创建摄像头实例
final cameraId = await recorder.createCamera(
  cameras.first,
  ResolutionPreset.high,
  enableAudio: true,
);

// 5. 初始化摄像头
await recorder.initializeCamera(cameraId);

// 6. 显示预览
Widget preview = recorder.buildPreview(cameraId);

// 7. 开始录制
await recorder.startVideoRecording(
  cameraId,
  filePath: 'D:/Videos/my_video.mp4',
);

// 8. 停止录制
final videoFile = await recorder.stopVideoRecording(cameraId);

// 9. 拍照
final photoFile = await recorder.takePicture(cameraId);

// 10. 释放资源
await recorder.dispose(cameraId);
```

### 事件监听

```dart
// 监听初始化事件
recorder.onCameraInitialized(cameraId).listen((event) {
  print('摄像头初始化完成: ${event.previewWidth}x${event.previewHeight}');
});

// 监听错误事件
recorder.onCameraError(cameraId).listen((event) {
  print('摄像头错误: ${event.description}');
});

// 监听录制完成事件
recorder.onVideoRecordedEvent(cameraId).listen((event) {
  print('视频录制完成: ${event.path}');
});
```

## 错误处理

### 常见异常

**CameraException**
```dart
try {
  await recorder.initializeCamera(cameraId);
} on CameraException catch (e) {
  print('摄像头错误: ${e.code} - ${e.description}');
}
```

**PlatformException**
```dart
try {
  final cameras = await recorder.availableCameras();
} on PlatformException catch (e) {
  print('平台错误: ${e.code} - ${e.message}');
}
```

## 注意事项

1. **平台限制**: 此插件仅支持Windows平台
2. **权限要求**: 需要摄像头和麦克风访问权限
3. **资源管理**: 使用完毕后必须调用`dispose()`释放资源
4. **文件路径**: 确保录制路径存在且有写入权限
5. **线程安全**: 所有方法都是异步的，需要在主线程调用

## 性能建议

1. **分辨率选择**: 根据需求选择合适的分辨率预设
2. **资源释放**: 及时释放不使用的摄像头资源
3. **错误处理**: 实现完善的错误处理机制
4. **预览优化**: 避免频繁重建预览Widget
