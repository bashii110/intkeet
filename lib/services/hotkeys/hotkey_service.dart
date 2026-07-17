import 'package:ai_overlay_assistant/models/config_models.dart';

/// Thrown when `RegisterHotKey` fails, typically a conflict with another
/// app's registration (02-ARCHITECTURE.md §6: "surface failure to Dart
/// with the conflicting binding so the user can rebind").
class HotkeyConflictException implements Exception {
  const HotkeyConflictException(this.binding, this.message);
  final HotkeyBinding binding;
  final String message;

  @override
  String toString() => 'HotkeyConflictException: $message';
}

/// Translates [HotkeyBinding] config into native `RegisterHotKey` calls
/// via the overlay bridge's MethodChannel transport. Real implementation
/// lands in Phase 3 alongside the settings UI.
abstract class HotkeyService {
  Future<void> register(HotkeyBinding binding);
  Future<void> unregister(HotkeyAction action);
  Stream<HotkeyAction> get onTriggered;
}
