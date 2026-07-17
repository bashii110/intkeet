import 'package:ai_overlay_assistant/services/logging/app_logger.dart';
import 'package:ai_overlay_assistant/services/overlay/overlay_bridge.dart';
import 'package:ai_overlay_assistant/services/storage/local_database.dart';
import 'package:ai_overlay_assistant/services/storage/secure_key_storage.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

/// Composition root (02-ARCHITECTURE.md §3: `app/` owns bootstrap,
/// routing, DI/composition root). Every cross-cutting service is wired
/// here as a provider; `ui/` never constructs a service directly.
///
/// Phase 0: wires the fakes/in-memory stubs defined alongside each
/// interface. Phase 1-4 swap these `overrideWithValue`s for real
/// implementations in `main.dart` without touching any widget.

final loggerProvider = Provider.family<AppLogger, String>((ref, tag) {
  return AppLogger(tag);
});

final localDatabaseProvider = Provider<LocalDatabase>((ref) {
  throw UnimplementedError(
    'localDatabaseProvider must be overridden in main.dart with a real '
    'implementation once Phase 3 lands. Tests should override with '
    'InMemoryLocalDatabase directly.',
  );
});

final secureKeyStorageProvider = Provider<SecureKeyStorage>((ref) {
  throw UnimplementedError(
    'secureKeyStorageProvider must be overridden in main.dart. Tests '
    'should override with InMemorySecureKeyStorage directly.',
  );
});

final overlayBridgeProvider = Provider<OverlayBridge>((ref) {
  throw UnimplementedError(
    'overlayBridgeProvider must be overridden once the native bridge '
    '(Phase 1/2) exists. Development/tests use FakeOverlayBridge.',
  );
});
