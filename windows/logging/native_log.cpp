#include "native_log.h"

#include <windows.h>

#include <sstream>

namespace ai_overlay {

namespace {

void Emit(const char* level, const char* tag, const std::string& message) {
  std::ostringstream line;
  line << "[" << level << "] " << tag << ": " << message << "\n";
  // OutputDebugStringA is the Phase 0 sink — visible in DebugView/VS
  // Output window. Replaced/augmented by the rotating file sink in
  // Phase 3 without changing any call site.
  ::OutputDebugStringA(line.str().c_str());
}

}  // namespace

void NativeLog::Debug(const char* tag, const std::string& message) {
#ifndef NDEBUG
  Emit("DEBUG", tag, message);
#else
  (void)tag;
  (void)message;
#endif
}

void NativeLog::Info(const char* tag, const std::string& message) {
  Emit("INFO", tag, message);
}

void NativeLog::Warn(const char* tag, const std::string& message) {
  Emit("WARN", tag, message);
}

void NativeLog::ErrorWithLastError(const char* tag, const std::string& message) {
  const DWORD error_code = ::GetLastError();
  std::ostringstream full;
  full << message << " | GetLastError=" << error_code;
  Emit("ERROR", tag, full.str());
}

}  // namespace ai_overlay
