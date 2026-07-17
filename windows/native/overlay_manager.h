#pragma once
// Thread-affinity: public methods are safe to call from ANY thread — they
// marshal work onto the dedicated OverlayThread via the command queue.
// Never touch OverlayWindow/HotkeyManager/DisplayAffinityController
// directly from outside this class.

#include <windows.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "../capture/display_affinity.h"
#include "../hotkeys/hotkey_manager.h"
#include "animation.h"
#include "overlay_window.h"

namespace ai_overlay {

using OverlayCommand = std::function<void()>;

// Events forwarded up to Dart (02-ARCHITECTURE.md §5: onHotkeyPressed,
// onRegionSelected, onOverlayMoved, DPI change). Kept as a simple tagged
// callback rather than a class hierarchy on this side of the FFI
// boundary — the FFI layer (ffi/overlay_api.cpp) is what turns these
// into the flat calls Dart's OverlayBridge expects.
struct OverlayEventSink {
  std::function<void(const std::string& action_id)> on_hotkey_pressed;
  std::function<void(const WindowBounds& bounds)> on_moved;
  std::function<void(double scale_factor)> on_dpi_changed;
};

// The only class Dart's bridge talks to (02-ARCHITECTURE.md §4). Owns
// OverlayThread and composes OverlayWindow, HotkeyManager, and
// DisplayAffinityController rather than owning their logic directly
// (03-RULES.md §3).
class OverlayManager {
 public:
  OverlayManager() = default;
  ~OverlayManager();

  OverlayManager(const OverlayManager&) = delete;
  OverlayManager& operator=(const OverlayManager&) = delete;

  void set_event_sink(OverlayEventSink sink) { event_sink_ = std::move(sink); }

  // Starts OverlayThread, which creates the (initially hidden)
  // OverlayWindow and enters its message loop. Safe to call from the
  // Flutter platform thread at app startup.
  bool Start();

  // Signals the thread to exit and joins it. Must not be called from
  // OverlayThread itself.
  void Stop();

  bool is_running() const { return running_.load(); }

  // --- Thread-safe command API (marshals onto OverlayThread) ---
  void Show(WindowBounds bounds);
  void Hide();
  void SetBounds(WindowBounds bounds);
  void SetClickThrough(bool enabled);

  // Runs synchronously relative to the caller by design: the result
  // (actually-applied state) must reach Dart in the same round trip
  // (OverlayBridge.setCaptureExcluded returns Future<bool>), so this
  // blocks the calling thread until the OverlayThread has applied it.
  // Bounded wait — see .cpp for the timeout/failure contract.
  CaptureAffinityState SetCaptureExcludedBlocking(bool enabled);

  void RegisterHotkey(const HotkeyRegistration& registration);

 private:
  void PostCommand(OverlayCommand command);
  void Run();  // OverlayThread entry point.
  void DrainCommandQueue();

  static constexpr UINT kWakeMessage = WM_APP + 1;

  std::thread overlay_thread_;
  DWORD overlay_thread_id_ = 0;
  std::atomic<bool> running_{false};

  std::mutex queue_mutex_;
  std::queue<OverlayCommand> command_queue_;

  // Constructed on OverlayThread inside Run() — Win32 windows must be
  // created and destroyed on the thread that pumps their messages.
  std::unique_ptr<OverlayWindow> window_;
  std::unique_ptr<HotkeyManager> hotkey_manager_;
  DisplayAffinityController display_affinity_;
  std::unique_ptr<Animator> animator_;

  OverlayEventSink event_sink_;
};

}  // namespace ai_overlay
