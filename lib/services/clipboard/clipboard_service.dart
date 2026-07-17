/// Opt-in clipboard watcher (FR-21). Off by default; enabling it always
/// requires an explicit, dated user action recorded in settings
/// (03-RULES.md §1 rule 4 — consent gates stay gates).
///
/// Implemented via polling through the native bridge in Phase 5
/// (`services/clipboard/` per 02-ARCHITECTURE.md §3 comment: "polling
/// via native bridge"). Phase 0 only defines the contract.
abstract class ClipboardService {
  bool get isMonitoring;

  /// Starts monitoring. Caller (settings UI) is responsible for having
  /// already recorded the consent action before calling this — this
  /// method does not itself prompt.
  Future<void> startMonitoring();

  /// Kill switch — FR-21 requires this to be reachable in one click from
  /// the UI, and to take effect immediately with zero further clipboard
  /// reads after disable (04-PHASES.md Phase 5 gate).
  Future<void> stopMonitoring();

  Stream<String> get onClipboardTextChanged;
}
