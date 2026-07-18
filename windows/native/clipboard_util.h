#pragma once
// Documented Win32 clipboard sequence:
// https://learn.microsoft.com/windows/win32/dataxchg/using-the-clipboard

#include <windows.h>

#include <string>

namespace ai_overlay {

// Writes `text` to the system clipboard as CF_UNICODETEXT. `owner` should
// be the overlay's own HWND (required by OpenClipboard). Returns false
// (and logs GetLastError()) on any failure in the sequence; never leaves
// the clipboard open on an error path.
bool WriteTextToClipboard(HWND owner, const std::wstring& text);

}  // namespace ai_overlay
