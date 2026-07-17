/// Shared structured logger for the whole app.
///
/// Rules this class exists to enforce (03-RULES.md §2 "Logging"):
///  - never `print()` in committed code — this is the only sanctioned sink
///  - never log secrets, full clipboard contents, or full OCR text at
///    info level or above (callers must hash/truncate before passing in)
///  - leveled: debug (dev only) / info / warn / error
library;

enum LogLevel { debug, info, warn, error }

/// A single emitted log record. Kept as a plain, serializable shape so
/// sinks (console, rotating file — see services/logging/file_sink.dart in
/// a later phase) can consume it without depending on `dart:developer`
/// directly in test code.
class LogRecord {
  const LogRecord({
    required this.level,
    required this.tag,
    required this.message,
    required this.timestamp,
    this.error,
    this.stackTrace,
  });

  final LogLevel level;
  final String tag;
  final String message;
  final DateTime timestamp;
  final Object? error;
  final StackTrace? stackTrace;

  @override
  String toString() {
    final buf = StringBuffer(
      '[${timestamp.toIso8601String()}] '
      '${level.name.toUpperCase().padRight(5)} $tag: $message',
    );
    if (error != null) buf.write(' | error=$error');
    return buf.toString();
  }
}

/// Sink interface — Phase 0 ships a console sink; a rotating-file sink
/// (per 02-ARCHITECTURE.md §9, %LOCALAPPDATA%\AiOverlayAssistant\) lands
/// in Phase 3 alongside the settings/storage work. Kept as an interface
/// now so that addition doesn't require touching call sites.
abstract class LogSink {
  void write(LogRecord record);
}

class ConsoleLogSink implements LogSink {
  const ConsoleLogSink();

  @override
  void write(LogRecord record) {
    // ignore: avoid_print
    // This is the one sanctioned print in the codebase — the sink itself.
    // debugPrint is not used here to keep this package Flutter-independent
    // (native/CLI test targets can construct an AppLogger too).
    // ignore: avoid_print
    print(record.toString());
  }
}

/// App-wide logger. Construct one per component with a short, stable
/// [tag] (e.g. "OverlayBridge", "AiProvider.openai") so log lines are
/// filterable.
class AppLogger {
  AppLogger(this.tag, {List<LogSink>? sinks, this.minLevel = LogLevel.info})
      : _sinks = sinks ?? const [ConsoleLogSink()];

  final String tag;
  final LogLevel minLevel;
  final List<LogSink> _sinks;

  void debug(String message) => _emit(LogLevel.debug, message);
  void info(String message) => _emit(LogLevel.info, message);
  void warn(String message) => _emit(LogLevel.warn, message);

  void error(String message, {Object? error, StackTrace? stackTrace}) {
    _emit(LogLevel.error, message, error: error, stackTrace: stackTrace);
  }

  void _emit(
    LogLevel level,
    String message, {
    Object? error,
    StackTrace? stackTrace,
  }) {
    if (level.index < minLevel.index) return;
    final record = LogRecord(
      level: level,
      tag: tag,
      message: message,
      timestamp: DateTime.now(),
      error: error,
      stackTrace: stackTrace,
    );
    for (final sink in _sinks) {
      sink.write(record);
    }
  }
}

/// Truncates/redacts a value that must never appear in full in logs
/// (clipboard text, OCR results, API keys). Use this at any call site
/// that logs something derived from those sources.
String redactForLog(String value, {int keepChars = 12}) {
  if (value.length <= keepChars) return '*' * value.length;
  return '${value.substring(0, keepChars)}…[${value.length} chars total]';
}
