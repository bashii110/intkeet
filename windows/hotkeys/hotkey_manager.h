#pragma once
// Thread-affinity: RegisterHotKey/UnregisterHotKey must be called from
// the thread that owns the message loop consuming WM_HOTKEY — i.e. the
// OverlayThread (02-ARCHITECTURE.md §6). This class is composed into
// OverlayManager, never constructed standalone in production.

#include <windows.h>

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

namespace ai_overlay {

struct HotkeyRegistration {
  int32_t id = 0;
  uint32_t modifiers = 0;   // MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN
  uint32_t virtual_key = 0;
  std::string action_id;    // Opaque string forwarded to Dart on trigger
};

// Wraps RegisterHotKey (documented, per-thread, no admin rights, no
// global keyboard hook — 02-ARCHITECTURE.md §6). The WH_KEYBOARD_LL
// opt-in "advanced hotkeys" path is a separate class added in Phase 5,
// not part of this one, to keep this class's default path free of the
// AV-false-positive/perf caveats that come with a global hook.
class HotkeyManager {
 public:
  using TriggerCallback = std::function<void(const std::string& action_id)>;

  explicit HotkeyManager(TriggerCallback on_triggered);
  ~HotkeyManager();

  HotkeyManager(const HotkeyManager&) = delete;
  HotkeyManager& operator=(const HotkeyManager&) = delete;

  // Attempts registration; returns false on conflict (e.g. another app
  // already owns that combination) without crashing or silently no-op'ing
  // — caller surfaces the failure to Dart per 02-ARCHITECTURE.md §6.
  //
  // Phase 0 stub: always returns false ("not yet implemented"); Phase 3
  // wires the real RegisterHotKey call:
  // https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-registerhotkey
  bool Register(const HotkeyRegistration& registration);

  bool Unregister(int32_t id);

  // Dispatch point for WM_HOTKEY messages pumped by OverlayManager's
  // message loop. No-op until Phase 3.
  void HandleHotkeyMessage(WPARAM wparam);

 private:
  TriggerCallback on_triggered_;
  std::unordered_map<int32_t, HotkeyRegistration> registrations_;
};

}  // namespace ai_overlay
