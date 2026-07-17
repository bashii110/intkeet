import 'dart:async';
import 'dart:ffi' as ffi;
import 'dart:io';
import 'dart:ui' show Rect;

import 'package:ai_overlay_assistant/services/logging/app_logger.dart';
import 'package:ai_overlay_assistant/services/overlay/overlay_bridge.dart';
import 'package:flutter/services.dart';

// Must match the extern "C" signature in windows/ffi/overlay_api.h.
typedef _GetBridgeSchemaVersionNative = ffi.Int32 Function();
typedef _GetBridgeSchemaVersionDart = int Function();

/// Real transport, backing [OverlayBridge] once Phase 1's native window
/// exists. Method/event channel names and payload shapes here are the
/// single source of truth shared with
/// `windows/ffi/method_channel_handlers.cpp` — if you change one side,
/// change the other (03-RULES.md §4: bridge calls are versioned; payload
/// shapes are documented, not ad-hoc).
class WindowsOverlayBridge implements OverlayBridge {
  WindowsOverlayBridge({
    MethodChannel? methodChannel,
    EventChannel? eventChannel,
    ffi.DynamicLibrary? nativeLibraryOverride,
  })  : _log = AppLogger('WindowsOverlayBridge'),
        _methodChannel = methodChannel ??
            const MethodChannel('ai_overlay_assistant/overlay'),
        _eventChannel = eventChannel ??
            const EventChannel('ai_overlay_assistant/overlay_events') {
    _verifySchemaVersion(nativeLibraryOverride);
    _events = _eventChannel
        .receiveBroadcastStream()
        .map(_decodeEvent)
        .where((event) => event != null)
        .cast<OverlayEvent>();
  }

  final AppLogger _log;
  final MethodChannel _methodChannel;
  final EventChannel _eventChannel;
  late final Stream<OverlayEvent> _events;

  void _verifySchemaVersion(ffi.DynamicLibrary? override) {
    if (!Platform.isWindows && override == null) {
      // Allows this class to be imported (not constructed against a real
      // DLL) on non-Windows dev machines running `flutter analyze`.
      return;
    }
    try {
      final lib = override ?? ffi.DynamicLibrary.process();
      final getVersion = lib
          .lookupFunction<_GetBridgeSchemaVersionNative,
              _GetBridgeSchemaVersionDart>('AiOverlay_GetBridgeSchemaVersion');
      final nativeVersion = getVersion();
      if (nativeVersion != kBridgeSchemaVersion) {
        throw OverlayBridgeException(
          'Bridge schema mismatch: native=$nativeVersion '
          'dart=$kBridgeSchemaVersion. Rebuild native and Dart together.',
        );
      }
    } on OverlayBridgeException {
      rethrow;
    } catch (e) {
      // Missing symbol, etc. — loud failure per 03-RULES.md §4, not a
      // silent skip, since a mismatch here means every other call below
      // is talking to an incompatible native build.
      throw OverlayBridgeException('Bridge schema handshake failed: $e');
    }
  }

  OverlayEvent? _decodeEvent(dynamic raw) {
    if (raw is! Map) return null;
    final type = raw['type'] as String?;
    switch (type) {
      case 'hotkeyPressed':
        return HotkeyPressedEvent(raw['actionId'] as String? ?? '');
      case 'overlayMoved':
        return OverlayMovedEvent(_rectFromMap(raw));
      case 'dpiChanged':
        return DpiChangedEvent((raw['scaleFactor'] as num?)?.toDouble() ?? 1.0);
      default:
        _log.warn('Unknown event type from native: $type');
        return null;
    }
  }

  Rect _rectFromMap(Map raw) {
    final x = (raw['x'] as num?)?.toDouble() ?? 0;
    final y = (raw['y'] as num?)?.toDouble() ?? 0;
    final width = (raw['width'] as num?)?.toDouble() ?? 0;
    final height = (raw['height'] as num?)?.toDouble() ?? 0;
    return Rect.fromLTWH(x, y, width, height);
  }

  Map<String, dynamic> _boundsToArgs(Rect bounds) {
    return {
      'x': bounds.left.round(),
      'y': bounds.top.round(),
      'width': bounds.width.round(),
      'height': bounds.height.round(),
    };
  }

  Future<T> _invoke<T>(String method, [Map<String, dynamic>? args]) async {
    try {
      return await _methodChannel.invokeMethod<T>(method, args) as T;
    } on PlatformException catch (e) {
      throw OverlayBridgeException('$method failed: ${e.message}');
    } on MissingPluginException {
      throw const OverlayBridgeException(
        'Native overlay plugin not registered — is this running on '
        'Windows with OverlayMethodChannelPlugin registered at startup?',
      );
    }
  }

  @override
  Future<void> show({Rect? bounds}) async {
    await _invoke<void>('show', bounds != null ? _boundsToArgs(bounds) : null);
  }

  @override
  Future<void> hide() async {
    await _invoke<void>('hide');
  }

  @override
  Future<void> setBounds(Rect bounds) async {
    await _invoke<void>('setBounds', _boundsToArgs(bounds));
  }

  @override
  Future<void> setClickThrough(bool enabled) async {
    await _invoke<void>('setClickThrough', {'enabled': enabled});
  }

  @override
  Future<bool> setCaptureExcluded(bool enabled) async {
    return _invoke<bool>('setCaptureExcluded', {'enabled': enabled});
  }

  @override
  Future<void> setOpacity(double opacity) async {
    await _invoke<void>('setOpacity', {'opacity': opacity});
  }

  @override
  Stream<OverlayEvent> get events => _events;
}
