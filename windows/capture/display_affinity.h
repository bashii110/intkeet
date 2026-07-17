#pragma once
// Thread-affinity: SetWindowDisplayAffinity must be called from a thread
// with access to the target HWND — call via OverlayManager's command
// queue, same as OverlayWindow methods.
//
// 03-RULES.md §1 rule 2 (binding on this whole file): UI copy and this
// class's own comments must always describe capture exclusion as
// "best-effort, may not work with all capture methods" — never
// "invisible" or "undetectable". Do not extend this class with anything
// whose primary purpose is defeating a specific third-party capture or
// monitoring tool; that is out of scope per 01-SRD.md §2.2.

#include <windows.h>

namespace ai_overlay {

enum class CaptureAffinityState {
  kNotSupported,  // OS build predates WDA_EXCLUDEFROMCAPTURE
  kDisabled,
  kEnabled,
};

// Isolated capability class (02-ARCHITECTURE.md §4): queries OS version,
// applies/reverts WDA_EXCLUDEFROMCAPTURE, and reports the *actually
// applied* state back rather than assuming success — SetWindowPos-family
// calls can silently no-op on unsupported configurations.
class DisplayAffinityController {
 public:
  DisplayAffinityController() = default;

  // Returns true if the running OS build supports
  // WDA_EXCLUDEFROMCAPTURE (requires Windows 10 2004+). Documented at:
  // https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-setwindowdisplayaffinity
  //
  // Phase 0 stub: always reports unsupported until Phase 1 implements
  // the actual version check.
  bool IsSupportedOnThisOs() const;

  // Applies or reverts the affinity flag on `hwnd`. Always re-queries the
  // actual affinity via GetWindowDisplayAffinity afterward and returns
  // that — never just echoes back the requested state
  // (01-SRD.md §7: some capture paths may still show the window; this
  // must never be silently misreported as success).
  CaptureAffinityState Apply(HWND hwnd, bool exclude_from_capture);

  CaptureAffinityState current_state() const { return current_state_; }

 private:
  CaptureAffinityState current_state_ = CaptureAffinityState::kNotSupported;
};

}  // namespace ai_overlay
