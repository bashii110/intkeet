import 'dart:ui';

import 'package:ai_overlay_assistant/services/overlay/overlay_bridge.dart';
import 'package:ai_overlay_assistant/services/overlay/windows_overlay_bridge.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  const methodChannel = MethodChannel('ai_overlay_assistant/overlay');
  const eventChannel = EventChannel('ai_overlay_assistant/overlay_events');
  final messenger = TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger;

  final calls = <MethodCall>[];

  setUp(() {
    calls.clear();
    messenger.setMockMethodCallHandler(methodChannel, (call) async {
      calls.add(call);
      switch (call.method) {
        case 'setCaptureExcluded':
          return true;
        default:
          return null;
      }
    });
  });

  tearDown(() {
    messenger.setMockMethodCallHandler(methodChannel, null);
  });

  // Schema-version check is skipped on non-Windows test runners (see
  // WindowsOverlayBridge._verifySchemaVersion) so this constructs safely
  // in CI without a real native DLL loaded.
  WindowsOverlayBridge buildBridge() =>
      WindowsOverlayBridge(methodChannel: methodChannel, eventChannel: eventChannel);

  test('show() sends bounds using the documented x/y/width/height shape', () async {
    final bridge = buildBridge();
    await bridge.show(bounds: const Rect.fromLTWH(10, 20, 380, 520));

    expect(calls, hasLength(1));
    expect(calls.single.method, 'show');
    expect(calls.single.arguments, {'x': 10, 'y': 20, 'width': 380, 'height': 520});
  });

  test('hide() sends no arguments', () async {
    final bridge = buildBridge();
    await bridge.hide();

    expect(calls.single.method, 'hide');
    expect(calls.single.arguments, isNull);
  });

  test('setCaptureExcluded returns the actually-applied state from native', () async {
    final bridge = buildBridge();
    final applied = await bridge.setCaptureExcluded(true);

    expect(applied, isTrue);
    expect(calls.single.arguments, {'enabled': true});
  });

  test('a PlatformException is translated to OverlayBridgeException', () async {
    messenger.setMockMethodCallHandler(methodChannel, (call) async {
      throw PlatformException(code: 'boom', message: 'native failure');
    });
    final bridge = buildBridge();

    expect(() => bridge.hide(), throwsA(isA<OverlayBridgeException>()));
  });
}
