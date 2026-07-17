#include "dpi.h"

#include "../logging/native_log.h"

namespace ai_overlay {

UINT GetWindowDpiSafe(HWND hwnd) {
  if (hwnd == nullptr) return static_cast<UINT>(kBaselineDpi);

  const UINT dpi = ::GetDpiForWindow(hwnd);
  if (dpi == 0) {
    NativeLog::Warn("dpi", "GetDpiForWindow returned 0; falling back to 96.");
    return static_cast<UINT>(kBaselineDpi);
  }
  return dpi;
}

}  // namespace ai_overlay
