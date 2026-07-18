#include "overlay_window.h"

#include <windowsx.h>

#include "../logging/native_log.h"

// These DWM identifiers were added in newer Windows SDKs than this
// project may be built against (DWMWA_WINDOW_CORNER_PREFERENCE: SDK
// 10.0.22000+; DWMWA_SYSTEMBACKDROP_TYPE: SDK 10.0.22621+). Both calls
// already degrade gracefully at runtime on older OS builds (SUCCEEDED()
// check); these #ifndef guards only make sure the *build* doesn't fail
// on an older SDK header, mirroring the WDA_EXCLUDEFROMCAPTURE fallback
// in display_affinity.cpp.
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
typedef enum { DWMWCP_DEFAULT = 0, DWMWCP_DONOTROUND = 1, DWMWCP_ROUND = 2, DWMWCP_ROUNDSMALL = 3 } DWM_WINDOW_CORNER_PREFERENCE;
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
typedef enum { DWMSBT_AUTO = 0, DWMSBT_NONE = 1, DWMSBT_MAINWINDOW = 2, DWMSBT_TRANSIENTWINDOW = 3, DWMSBT_TABBEDWINDOW = 4 } DWM_SYSTEMBACKDROP_TYPE;
#endif

namespace ai_overlay {

namespace {
constexpr wchar_t kWindowClassName[] = L"AiOverlayAssistant.OverlayWindow";

// GWLP_USERDATA slot stores the `this` pointer so the static thunk can
// dispatch to the instance method — standard Win32 pattern, documented
// at https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-setwindowlongptrw
OverlayWindow* InstanceFromHwnd(HWND hwnd) {
  return reinterpret_cast<OverlayWindow*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}
}  // namespace

OverlayWindow::OverlayWindow() = default;

OverlayWindow::~OverlayWindow() {
  if (hwnd_ != nullptr) {
    Destroy();
  }
}

bool OverlayWindow::Create(const WindowBounds& initial_bounds,
                            const std::wstring& title) {
  const HINSTANCE instance = ::GetModuleHandleW(nullptr);

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(WNDCLASSEXW);
  // Only register once per process — a second RegisterClassExW call
  // with the same name fails harmlessly with ERROR_CLASS_ALREADY_EXISTS,
  // which we treat as success below rather than a hard failure.
  wc.lpfnWndProc = &OverlayWindow::WndProcThunk;
  wc.hInstance = instance;
  wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
  wc.lpszClassName = kWindowClassName;
  wc.hbrBackground = nullptr;  // We paint via the content renderer, not GDI fill.

  if (!::RegisterClassExW(&wc) && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    NativeLog::ErrorWithLastError("OverlayWindow", "RegisterClassExW failed");
    return false;
  }

  // WS_POPUP: borderless top-level window (no title bar/system menu —
  // we draw our own header per 05-DESIGN.md §3).
  // WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST: see header comment.
  const DWORD ex_style = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
  const DWORD style = WS_POPUP;

  hwnd_ = ::CreateWindowExW(
      ex_style, kWindowClassName, title.c_str(), style,
      initial_bounds.x, initial_bounds.y,
      initial_bounds.width, initial_bounds.height,
      /*hWndParent=*/nullptr, /*hMenu=*/nullptr, instance,
      /*lpParam=*/this);

  if (hwnd_ == nullptr) {
    NativeLog::ErrorWithLastError("OverlayWindow", "CreateWindowExW failed");
    return false;
  }

  // Store `this` for WndProcThunk. Done again here (not just relying on
  // WM_NCCREATE's lpCreateParams path below) as a defensive no-op-safe
  // assignment in case a future Windows version changes ordering.
  ::SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

  ApplyRoundedCorners();     // No-op-safe on pre-Win11.
  if (!ApplyAcrylicBackdrop()) {
    // Solid-dark fallback (05-DESIGN.md bg.base #0F1115) — the content
    // renderer (Phase 2) paints this; nothing further needed here beyond
    // leaving DWMWA_SYSTEMBACKDROP_TYPE unset.
    NativeLog::Info("OverlayWindow",
                     "Acrylic backdrop unsupported on this build; using solid fallback.");
  }

  // 100% opaque at the layered-window-attributes level; actual
  // panel-tint alpha (bg.acrylic.tint, 80%) is drawn by the content
  // renderer, not via SetLayeredWindowAttributes, so DWM composition
  // still gets real pixels to blur.
  ::SetLayeredWindowAttributes(hwnd_, 0, 255, LWA_ALPHA);

  ::ShowWindow(hwnd_, SW_HIDE);  // Caller (OverlayManager) decides initial visibility.
  return true;
}

void OverlayWindow::Destroy() {
  if (hwnd_ != nullptr) {
    ::DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
}

bool OverlayWindow::SetBounds(const WindowBounds& bounds) {
  if (hwnd_ == nullptr) return false;
  const BOOL ok = ::SetWindowPos(
      hwnd_, nullptr, bounds.x, bounds.y, bounds.width, bounds.height,
      SWP_NOZORDER | SWP_NOACTIVATE);
  if (!ok) {
    NativeLog::ErrorWithLastError("OverlayWindow", "SetWindowPos (bounds) failed");
    return false;
  }
  return true;
}

bool OverlayWindow::SetTopmost(bool topmost) {
  if (hwnd_ == nullptr) return false;
  const HWND insert_after = topmost ? HWND_TOPMOST : HWND_NOTOPMOST;
  const BOOL ok = ::SetWindowPos(
      hwnd_, insert_after, 0, 0, 0, 0,
      SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  if (!ok) {
    NativeLog::ErrorWithLastError("OverlayWindow", "SetWindowPos (topmost) failed");
    return false;
  }
  return true;
}

bool OverlayWindow::SetClickThrough(bool enabled) {
  if (hwnd_ == nullptr) return false;

  const LONG_PTR current = ::GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
  const LONG_PTR updated =
      enabled ? (current | WS_EX_TRANSPARENT) : (current & ~WS_EX_TRANSPARENT);

  if (::SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, updated) == 0 && ::GetLastError() != 0) {
    NativeLog::ErrorWithLastError("OverlayWindow", "SetWindowLongPtrW (click-through) failed");
    return false;
  }
  click_through_ = enabled;
  return true;
}

bool OverlayWindow::ApplyRoundedCorners() {
  if (hwnd_ == nullptr) return false;
  // DWMWA_WINDOW_CORNER_PREFERENCE was introduced in Windows 11; on
  // older builds DwmSetWindowAttribute simply returns a failure HRESULT,
  // which we treat as an expected no-op, not an error to log loudly.
  const DWM_WINDOW_CORNER_PREFERENCE pref = DWMWCP_ROUND;
  const HRESULT hr = ::DwmSetWindowAttribute(
      hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
  return SUCCEEDED(hr);
}

bool OverlayWindow::ApplyAcrylicBackdrop() {
  if (hwnd_ == nullptr) return false;
  // DWMWA_SYSTEMBACKDROP_TYPE requires Windows 11 22H2+ (build 22621+).
  // We deliberately use this documented DWM attribute instead of the
  // classic `SetWindowCompositionAttribute` acrylic call: that function
  // is NOT published on Microsoft Learn (it's reverse-engineered from
  // shell32/user32), which 03-RULES.md §1 rule 1 disallows outright.
  // DWMSBT_TRANSIENTWINDOW gives the closest visual match to the
  // acrylic look described in 05-DESIGN.md while staying on a
  // documented API surface:
  // https://learn.microsoft.com/windows/win32/api/dwmapi/ne-dwmapi-dwm_systembackdrop_type
  const DWM_SYSTEMBACKDROP_TYPE backdrop = DWMSBT_TRANSIENTWINDOW;
  const HRESULT hr = ::DwmSetWindowAttribute(
      hwnd_, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
  return SUCCEEDED(hr);
}

LRESULT OverlayWindow::HitTest(POINT screen_pt) const {
  RECT window_rect{};
  ::GetWindowRect(hwnd_, &window_rect);

  const int x = screen_pt.x - window_rect.left;
  const int y = screen_pt.y - window_rect.top;
  const int width = window_rect.right - window_rect.left;
  const int height = window_rect.bottom - window_rect.top;

  const bool on_left = x < kResizeBorderPx;
  const bool on_right = x >= width - kResizeBorderPx;
  const bool on_top = y < kResizeBorderPx;
  const bool on_bottom = y >= height - kResizeBorderPx;

  if (on_top && on_left) return HTTOPLEFT;
  if (on_top && on_right) return HTTOPRIGHT;
  if (on_bottom && on_left) return HTBOTTOMLEFT;
  if (on_bottom && on_right) return HTBOTTOMRIGHT;
  if (on_left) return HTLEFT;
  if (on_right) return HTRIGHT;
  if (on_top) return HTTOP;
  if (on_bottom) return HTBOTTOM;

  // Header strip (05-DESIGN.md §3): drag region, EXCEPT over the icon
  // buttons in its right-hand cluster. The actual icon hit-rects live in
  // the content renderer (Phase 2); until that's wired in, the whole
  // header is drag-only, which is safe (icons aren't rendered yet).
  if (y < kHeaderHeightPx) return HTCAPTION;

  return HTCLIENT;
}

LRESULT CALLBACK OverlayWindow::WndProcThunk(HWND hwnd, UINT msg, WPARAM wparam,
                                              LPARAM lparam) {
  if (msg == WM_NCCREATE) {
    const auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    ::SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                         reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
  }

  OverlayWindow* self = InstanceFromHwnd(hwnd);
  if (self != nullptr) {
    return self->WndProc(hwnd, msg, wparam, lparam);
  }
  return ::DefWindowProcW(hwnd, msg, wparam, lparam);
}

LRESULT OverlayWindow::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case WM_NCHITTEST: {
      if (click_through_) {
        // Click-through windows shouldn't intercept hit-testing either —
        // WS_EX_TRANSPARENT already forwards input, but returning
        // HTTRANSPARENT here is the documented complement so DWM doesn't
        // paint a resize cursor over a window the user can't interact with.
        return HTTRANSPARENT;
      }
      POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      return HitTest(pt);
    }

    case WM_PAINT: {
      PAINTSTRUCT ps;
      // D2D renders directly to its own HWND-bound render target
      // (not via this GDI BeginPaint/EndPaint pair) — the pair here only
      // exists to validate the update region so Windows stops resending
      // WM_PAINT. Documented pattern for D2D-in-a-Win32-window:
      // https://learn.microsoft.com/windows/win32/direct2d/direct2d-quickstart
      ::BeginPaint(hwnd, &ps);
      if (on_paint_) on_paint_();
      ::EndPaint(hwnd, &ps);
      return 0;
    }

    case WM_LBUTTONUP: {
      if (on_click_) {
        POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        on_click_(pt);
      }
      break;
    }

    case WM_SIZE: {
      if (on_resize_) {
        on_resize_(LOWORD(lparam), HIWORD(lparam));
      }
      break;
    }

    case WM_WINDOWPOSCHANGED: {
      if (on_bounds_changed_) {
        const auto* pos = reinterpret_cast<WINDOWPOS*>(lparam);
        if ((pos->flags & SWP_NOMOVE) == 0 || (pos->flags & SWP_NOSIZE) == 0) {
          on_bounds_changed_(WindowBounds{pos->x, pos->y, pos->cx, pos->cy});
        }
      }
      break;
    }

    case WM_DPICHANGED: {
      // FR-25: rescale/reposition on DPI change. LOWORD(wparam) is the
      // new DPI for the X axis; 96 is the DPI-unaware baseline.
      // https://learn.microsoft.com/windows/win32/hidpi/wm-dpichanged
      const double scale = LOWORD(wparam) / 96.0;
      const auto* suggested = reinterpret_cast<RECT*>(lparam);
      ::SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                      suggested->right - suggested->left,
                      suggested->bottom - suggested->top,
                      SWP_NOZORDER | SWP_NOACTIVATE);
      if (on_dpi_changed_) {
        on_dpi_changed_(scale);
      }
      return 0;
    }

    case WM_DESTROY:
      hwnd_ = nullptr;
      return 0;

    default:
      break;
  }
  return ::DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace ai_overlay
