import 'package:ai_overlay_assistant/services/logging/app_logger.dart';

/// Wraps Windows Credential Locker (DPAPI-backed) for API keys and OAuth
/// tokens — FR-1 / SEC-1 in 01-SRD.md: keys are never stored in plaintext
/// SQLite, never logged.
///
/// Phase 0 scope: interface + in-memory fake. The real implementation
/// (Phase 3) calls the documented `CredWrite`/`CredRead`/`CredDelete`
/// Win32 credential APIs via the `win32` package — see the MS Learn
/// reference that will be linked next to that call per 03-RULES.md §1.
abstract class SecureKeyStorage {
  Future<void> write({required String key, required String value});
  Future<String?> read({required String key});
  Future<void> delete({required String key});
}

class SecureStorageException implements Exception {
  const SecureStorageException(this.message);
  final String message;

  @override
  String toString() => 'SecureStorageException: $message';
}

/// In-memory fake for Phase 0 / unit tests. NEVER used on a real Windows
/// build target — the real credential-locker-backed implementation is
/// wired in Phase 3 (`03-RULES.md` §1: no plaintext secrets, enforced by
/// a CI grep for `sk-`/`key=` literals as well).
class InMemorySecureKeyStorage implements SecureKeyStorage {
  InMemorySecureKeyStorage() : _log = AppLogger('SecureKeyStorage.inMemory');

  final AppLogger _log;
  final Map<String, String> _store = {};

  @override
  Future<void> write({required String key, required String value}) async {
    // Never log the value itself — only that a write happened.
    _log.debug('write key="$key" (${redactForLog(value)})');
    _store[key] = value;
  }

  @override
  Future<String?> read({required String key}) async {
    return _store[key];
  }

  @override
  Future<void> delete({required String key}) async {
    _log.debug('delete key="$key"');
    _store.remove(key);
  }
}
