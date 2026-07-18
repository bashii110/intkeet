#pragma once
// Thread-affinity: must only be constructed/destroyed/pumped from the
// dedicated OverlayThread owned by OverlayManager (02-ARCHITECTURE.md §2).

#include <windows.h>
#include <dwmapi.h>

#include <cstdint>
#include <functional>
#include <string>

namespace ai_overlay {

struct WindowBounds {
  int32_t x = 0;
  int32_t y = 0;
  int32_t width = 380;   // 05-DESIGN.md §2.3 default overlay size
  int32_t height = 520;
};

// Fired on drag/resize completion so OverlayManager can forward an
// OverlayMovedEvent to Dart (02-ARCHITECTURE.md §5).
using BoundsChangedCallback = std::function<void(const WindowBounds&)>;
using DpiChangedCallback = std::function<void(double scale_factor)>;

// Phase 2 content hooks — OverlayWindow stays renderer-agnostic
// (03-RULES.md §3): it only reports "you should paint now" / "you were
// clicked here" / "you were resized to WxH"; OverlayManager decides what
// that means by wiring in the active OverlayContentRenderer.
using PaintCallback = std::function<void()>;
using ClickCallback = std::function<void(POINT client_pt)>;
using ResizeCallback = std::function<void(int width, int height)>;

// OverlayWindow: only knows how to *be* a window — create/destroy/move/
// resize/style (03-RULES.md §3 "one class, one responsibility"). Drag,
// resize, click-through, rounded corners, and blur are all things a
// *window* does to itself; hotkeys and capture exclusion are NOT owned
// here — those are HotkeyManager and DisplayAffinityController, composed
// in by OverlayManager.
class OverlayWindow {
 public:
  OverlayWindow();
  ~OverlayWindow();

  OverlayWindow(const OverlayWindow&) = delete;
  OverlayWindow& operator=(const OverlayWindow&) = delete;

  // Registers the window class (once per process) and creates the
  // borderless, layered, topmost, tool window described in 01-SRD.md
  // FR-10/FR-12/FR-13/FR-15.
  //
  // Style flags used, each documented:
  //   WS_POPUP          - borderless top-level window
  //     https://learn.microsoft.com/windows/win32/winmsg/window-styles
  //   WS_EX_LAYERED     - required for SetLayeredWindowAttributes/DWM
  //     composition-based transparency
  //     https://learn.microsoft.com/windows/win32/winmsg/extended-window-styles
  //   WS_EX_TOOLWINDOW  - hides the window from Alt+Tab and the taskbar
  //     (01-SRD.md FR-15); paired with NOT setting WS_EX_APPWINDOW
  //   WS_EX_TOPMOST     - always-on-top; also reasserted at runtime via
  //     SetWindowPos(HWND_TOPMOST) per SetTopmost()
  bool Create(const WindowBounds& initial_bounds, const std::wstring& title);

  void Destroy();

  bool SetBounds(const WindowBounds& bounds);
  bool SetTopmost(bool topmost);

  // Adds/removes WS_EX_TRANSPARENT at runtime (01-SRD.md FR-13). When
  // enabled, mouse input passes through to the window underneath; the
  // header-strip-only "move mode" described in 05-DESIGN.md §3 is
  // implemented one layer up, in OverlayManager, since it's a policy
  // decision about *when* to re-enable dragging, not a window-style fact.
  bool SetClickThrough(bool enabled);

  // Applies DWMWA_WINDOW_CORNER_PREFERENCE (Win11+; documented at
  // https://learn.microsoft.com/windows/win32/api/dwmapi/ne-dwmapi-dwm_window_corner_preference)
  // No-ops (returns false, does not throw/crash) on older builds.
  bool ApplyRoundedCorners();

  // Applies DWMWA_SYSTEMBACKDROP_TYPE = DWMSBT_TRANSIENTWINDOW
  // (Win11 22H2+; documented at
  // https://learn.microsoft.com/windows/win32/api/dwmapi/ne-dwmapi-dwm_systembackdrop_type).
  // See the header comment in overlay_window.cpp for why this replaces
  // the classic SetWindowCompositionAttribute acrylic call — that API is
  // undocumented and disallowed by 03-RULES.md §1 rule 1.
  // Returns false (solid dark fallback stays active) when unsupported.
  bool ApplyAcrylicBackdrop();

  HWND hwnd() const { return hwnd_; }
  bool is_created() const { return hwnd_ != nullptr; }

  void set_on_bounds_changed(BoundsChangedCallback cb) { on_bounds_changed_ = std::move(cb); }
  void set_on_dpi_changed(DpiChangedCallback cb) { on_dpi_changed_ = std::move(cb); }
  void set_on_paint(PaintCallback cb) { on_paint_ = std::move(cb); }
  void set_on_click(ClickCallback cb) { on_click_ = std::move(cb); }
  void set_on_resize(ResizeCallback cb) { on_resize_ = std::move(cb); }

 private:
  static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

  // WM_NCHITTEST custom hit-testing (05-DESIGN.md §3: header is the drag
  // region except over icon buttons; edges/corners resize).
  LRESULT HitTest(POINT screen_pt) const;

  HWND hwnd_ = nullptr;
  bool click_through_ = false;
  static constexpr int kResizeBorderPx = 6;
  static constexpr int kHeaderHeightPx = 40;  // matches 05-DESIGN.md header row

  BoundsChangedCallback on_bounds_changed_;
  DpiChangedCallback on_dpi_changed_;
  PaintCallback on_paint_;
  ClickCallback on_click_;
  ResizeCallback on_resize_;
};

}  // namespace ai_overlay
