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
#include "../content/content_model.h"
#include "../hotkeys/hotkey_manager.h"
#include "animation.h"
#include "overlay_content_renderer.h"
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

// 02-ARCHITECTURE.md §7: native is the default; WebView2 is an explicit
// opt-in behind the ENABLE_WEBVIEW2_OVERLAY build flag (03-RULES.md §6).
enum class RendererMode { kNative, kWebView2 };

// The only class Dart's bridge talks to (02-ARCHITECTURE.md §4). Owns
// OverlayThread and composes OverlayWindow, HotkeyManager,
// DisplayAffinityController, and (Phase 2) the active
// OverlayContentRenderer + ContentModel, rather than owning their logic
// directly (03-RULES.md §3).
class OverlayManager {
 public:
  explicit OverlayManager(RendererMode renderer_mode = RendererMode::kNative);
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

  // --- Phase 2 content API (also thread-safe / marshals via the queue) ---
  // This is the call the FFI fast path (ffi/overlay_api.cpp) hits on
  // every streamed token — kept as its own narrow method rather than a
  // generic PostCommand exposure so that path stays cheap and doesn't
  // allocate a new std::function capture per call on the hot path.
  void AppendContentToken(std::wstring token);
  void ResetContent();

  // Process-wide accessor for the FFI fast path (ffi/overlay_api.cpp).
  // This project creates exactly one OverlayManager per process
  // (OverlayMethodChannelPlugin owns it); the FFI surface needs a way to
  // reach that same instance without MethodChannel's JSON round trip for
  // the high-frequency token-streaming path (02-ARCHITECTURE.md §5).
  static OverlayManager* ActiveInstance();

 private:
  void PostCommand(OverlayCommand command);
  void Run();  // OverlayThread entry point.
  void DrainCommandQueue();
  void OnWindowPaint();
  void OnWindowClick(POINT client_pt);
  void OnWindowResize(int width, int height);

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

  const RendererMode renderer_mode_;
  std::unique_ptr<OverlayContentRenderer> renderer_;
  ContentModel content_model_;

  OverlayEventSink event_sink_;
};

}  // namespace ai_overlay
