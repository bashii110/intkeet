#include "clipboard_util.h"

#include "../logging/native_log.h"

namespace ai_overlay {

bool WriteTextToClipboard(HWND owner, const std::wstring& text) {
  if (!::OpenClipboard(owner)) {
    NativeLog::ErrorWithLastError("Clipboard", "OpenClipboard failed");
    return false;
  }

  bool ok = false;
  if (::EmptyClipboard()) {
    const size_t byte_count = (text.size() + 1) * sizeof(wchar_t);
    const HGLOBAL mem = ::GlobalAlloc(GMEM_MOVEABLE, byte_count);
    if (mem != nullptr) {
      void* locked = ::GlobalLock(mem);
      if (locked != nullptr) {
        memcpy(locked, text.c_str(), byte_count);
        ::GlobalUnlock(mem);
        // Ownership of `mem` transfers to the clipboard on success — do
        // not GlobalFree it ourselves in that case.
        if (::SetClipboardData(CF_UNICODETEXT, mem) != nullptr) {
          ok = true;
        } else {
          NativeLog::ErrorWithLastError("Clipboard", "SetClipboardData failed");
          ::GlobalFree(mem);
        }
      } else {
        NativeLog::ErrorWithLastError("Clipboard", "GlobalLock failed");
        ::GlobalFree(mem);
      }
    } else {
      NativeLog::ErrorWithLastError("Clipboard", "GlobalAlloc failed");
    }
  } else {
    NativeLog::ErrorWithLastError("Clipboard", "EmptyClipboard failed");
  }

  ::CloseClipboard();
  return ok;
}

}  // namespace ai_overlay
