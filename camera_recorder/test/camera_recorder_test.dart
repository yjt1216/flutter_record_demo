import 'package:flutter_test/flutter_test.dart';
import 'package:camera_recorder/camera_window_recorder.dart';
import 'package:camera_recorder/camera_platform_interface/camera_platform_interface.dart';

void main() {
  group('CameraWindowsRecorder', () {
    test('instance creation', () {
      final CameraWindowsRecorder recorder = CameraWindowsRecorder();
      expect(recorder, isNotNull);
      expect(recorder, isA<CameraWindowsRecorder>());
    });

    test('has required properties', () {
      final CameraWindowsRecorder recorder = CameraWindowsRecorder();
      expect(recorder.hostCameraHandlers, isA<Map<int, HostCameraMessageHandler>>());
      expect(recorder.cameraEventStreamController, isNotNull);
    });

    test('registerWith sets instance', () {
      // 测试注册方法
      CameraWindowsRecorder.registerWith();
      expect(CameraPlatform.instance, isA<CameraWindowsRecorder>());
    });
  });
}
