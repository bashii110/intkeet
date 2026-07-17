import 'dart:async';
import 'dart:ui' show Rect;

/// Bridge schema version — bumped whenever a native/Dart payload shape
/// changes, so a mismatch is caught during development (03-RULES.md §4).
const int kBridgeSchemaVersion = 1;

sealed class OverlayEvent {
  const OverlayEvent();
}

class HotkeyPressedEvent extends OverlayEvent {
  const HotkeyPressedEvent(this.actionId);
  final String actionId;
}

class RegionSelectedEvent extends OverlayEvent {
  const RegionSelectedEvent(this.bounds);
  final Rect bounds;
}

class OverlayMovedEvent extends OverlayEvent {
  const OverlayMovedEvent(this.bounds);
  final Rect bounds;
}

class DpiChangedEvent extends OverlayEvent {
  const DpiChangedEvent(this.scaleFactor);
  final double scaleFactor;
}

/// Thrown when a native call fails; callers must not assume success
/// (02-ARCHITECTURE.md §10 — "Dart never assumes success").
class OverlayBridgeException implements Exception {
  const OverlayBridgeException(this.message, {this.nativeErrorCode});
  final String message;
  final int? nativeErrorCode;

  @override
  String toString() => 'OverlayBridgeException: $message'
      '${nativeErrorCode != null ? ' (code=$nativeErrorCode)' : ''}';
}

/// Contract lives here per 02-ARCHITECTURE.md §5 so the rest of the app
/// is agnostic to whether MethodChannel or the FFI fast path is active
/// underneath, and so it's mockable for unit tests.
abstract class OverlayBridge {
  Future<void> show({Rect? bounds});
  Future<void> hide();
  Future<void> setBounds(Rect bounds);
  Future<void> setClickThrough(bool enabled);

  /// Returns the *actually applied* state, per DisplayAffinityController's
  /// contract (never assume the OS honored the request).
  Future<bool> setCaptureExcluded(bool enabled);

  Future<void> setOpacity(double opacity);

  Stream<OverlayEvent> get events;
}

/// In-memory fake — lets `lib/ui/overlay` widgets and Riverpod providers
/// be developed and unit-tested on any platform before the real native
/// window exists (Phase 1) or without spinning up Windows CI.
class FakeOverlayBridge implements OverlayBridge {
  final _eventsController = StreamController<OverlayEvent>.broadcast();
  bool _captureExcluded = false;

  @override
  Future<void> show({Rect? bounds}) async {}

  @override
  Future<void> hide() async {}

  @override
  Future<void> setBounds(Rect bounds) async {}

  @override
  Future<void> setClickThrough(bool enabled) async {}

  @override
  Future<bool> setCaptureExcluded(bool enabled) async {
    // Fake honors the request unconditionally; real impl reports the
    // OS-version-gated actual state (see DisplayAffinityController).
    _captureExcluded = enabled;
    return _captureExcluded;
  }

  @override
  Future<void> setOpacity(double opacity) async {}

  @override
  Stream<OverlayEvent> get events => _eventsController.stream;

  void dispose() => _eventsController.close();
}
