#include "display_affinity.h"

#include "../logging/native_log.h"

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

namespace ai_overlay {

namespace {
// WDA_EXCLUDEFROMCAPTURE requires Windows 10 version 2004 (build 19041)
// or later. Checked via VerifyVersionInfoW + VerSetConditionMask — the
// documented, manifest-independent way to check a minimum build number:
// https://learn.microsoft.com/windows/win32/api/winbase/nf-winbase-verifyversioinfow
// https://learn.microsoft.com/windows/win32/api/winnt/nf-winnt-versetconditionmask
bool IsBuildAtLeast(DWORD major, DWORD minor, DWORD build) {
  OSVERSIONINFOEXW osvi{};
  osvi.dwOSVersionInfoSize = sizeof(osvi);
  osvi.dwMajorVersion = major;
  osvi.dwMinorVersion = minor;
  osvi.dwBuildNumber = build;

  DWORDLONG mask = 0;
  mask = ::VerSetConditionMask(mask, VER_MAJORVERSION, VER_GREATER_EQUAL);
  mask = ::VerSetConditionMask(mask, VER_MINORVERSION, VER_GREATER_EQUAL);
  mask = ::VerSetConditionMask(mask, VER_BUILDNUMBER, VER_GREATER_EQUAL);

  return ::VerifyVersionInfoW(
             &osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER,
             mask) != FALSE;
}
}  // namespace

bool DisplayAffinityController::IsSupportedOnThisOs() const {
  return IsBuildAtLeast(10, 0, 19041);
}

CaptureAffinityState DisplayAffinityController::Apply(HWND hwnd,
                                                        bool exclude_from_capture) {
  if (!IsSupportedOnThisOs()) {
    current_state_ = CaptureAffinityState::kNotSupported;
    NativeLog::Info("DisplayAffinityController",
                     "WDA_EXCLUDEFROMCAPTURE unsupported on this OS build.");
    return current_state_;
  }

  if (hwnd == nullptr) {
    current_state_ = CaptureAffinityState::kNotSupported;
    return current_state_;
  }

  const DWORD requested = exclude_from_capture ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE;
  if (!::SetWindowDisplayAffinity(hwnd, requested)) {
    // 03-RULES.md §3: every Win32 call that can fail is checked and
    // logged with GetLastError() decoded.
    NativeLog::ErrorWithLastError("DisplayAffinityController",
                                   "SetWindowDisplayAffinity failed");
  }

  // Never trust the request we just made — re-query the actual applied
  // state (01-SRD.md §7: some capture paths may still show the window
  // even when the flag was accepted; UI copy must reflect reality, not
  // intent). Documented at:
  // https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-getwindowdisplayaffinity
  DWORD actual = WDA_NONE;
  if (!::GetWindowDisplayAffinity(hwnd, &actual)) {
    NativeLog::ErrorWithLastError("DisplayAffinityController",
                                   "GetWindowDisplayAffinity failed");
    current_state_ = CaptureAffinityState::kNotSupported;
    return current_state_;
  }

  current_state_ = (actual == WDA_EXCLUDEFROMCAPTURE)
                        ? CaptureAffinityState::kEnabled
                        : CaptureAffinityState::kDisabled;
  return current_state_;
}

}  // namespace ai_overlay
