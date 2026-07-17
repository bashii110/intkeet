import 'package:ai_overlay_assistant/services/logging/app_logger.dart';

/// Facade over the local SQLite store (02-ARCHITECTURE.md §9).
///
/// Phase 0 scope: interface + in-memory stub only, so the rest of the app
/// (providers, UI) can be built and unit-tested against this contract
/// before the real drift schema/migrations land in Phase 3.
///
/// SEC-5 (01-SRD.md §6): the real implementation must encrypt at rest
/// (SQLCipher or DPAPI wrapping) for conversation history. Nothing in
/// this interface should leak a raw file path to callers outside
/// services/storage, so that constraint can be swapped in without
/// touching call sites.
abstract class LocalDatabase {
  Future<void> open();
  Future<void> close();

  // Narrow, typed methods are added per-feature as Phase 3 implements
  // the real schema (conversations/messages/templates/configs/hotkeys).
  // Deliberately not exposing a raw `execute(String sql)` escape hatch —
  // callers outside this service must not hand-roll SQL.
}

/// Thrown by [LocalDatabase] implementations. UI layer catches at the
/// boundary per 03-RULES.md §2 ("Error handling").
class StorageException implements Exception {
  const StorageException(this.message, {this.cause});
  final String message;
  final Object? cause;

  @override
  String toString() => 'StorageException: $message'
      '${cause != null ? ' (cause: $cause)' : ''}';
}

/// In-memory stand-in used until Phase 3's drift-backed implementation
/// lands, and reused directly as the fake in unit tests.
class InMemoryLocalDatabase implements LocalDatabase {
  InMemoryLocalDatabase() : _log = AppLogger('LocalDatabase.inMemory');

  final AppLogger _log;
  bool _isOpen = false;

  @override
  Future<void> open() async {
    _log.debug('opening in-memory stub database');
    _isOpen = true;
  }

  @override
  Future<void> close() async {
    if (!_isOpen) return;
    _log.debug('closing in-memory stub database');
    _isOpen = false;
  }
}
