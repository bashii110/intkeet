#pragma once
// Native-side counterpart to lib/services/logging/app_logger.dart.
// Same rule applies here as on the Dart side (03-RULES.md §2): never log
// secrets, full clipboard contents, or full OCR text.
//
// Phase 0: console/OutputDebugString sink only. A rotating file sink
// under %LOCALAPPDATA%\AiOverlayAssistant\ (02-ARCHITECTURE.md §9) lands
// alongside the Dart file sink in Phase 3, sharing the same log file so
// a single support bundle covers both surfaces.

#include <string>

namespace ai_overlay {

class NativeLog {
 public:
  static void Debug(const char* tag, const std::string& message);
  static void Info(const char* tag, const std::string& message);
  static void Warn(const char* tag, const std::string& message);

  // Decodes GetLastError() alongside the message, per 03-RULES.md §3
  // "every Win32 call that can fail is checked; failures are logged with
  // GetLastError() decoded."
  static void ErrorWithLastError(const char* tag, const std::string& message);
};

}  // namespace ai_overlay
