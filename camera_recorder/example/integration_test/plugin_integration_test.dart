// This is a basic Flutter integration test.
//
// Since integration tests run in a full Flutter application, they can interact
// with the host side of a plugin implementation, unlike Dart unit tests.
//
// For more information about Flutter integration tests, please see
// https://flutter.dev/to/integration-testing


import 'package:flutter_test/flutter_test.dart';
// 临时跳过集成测试依赖，防止工作区缺少 integration_test 包时报错
// import 'package:integration_test/integration_test.dart';

import 'package:flutter/services.dart';

void main() {
  // IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('getPlatformVersion test', (WidgetTester tester) async {
    const MethodChannel channel = MethodChannel('camera_recorder');
    final String? version = await channel.invokeMethod('getPlatformVersion');
    // The version string depends on the host platform running the test, so
    // just assert that some non-empty string is returned.
    expect(version?.isNotEmpty, true);
  });
}
