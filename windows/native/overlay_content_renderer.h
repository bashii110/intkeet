#pragma once
// Thread-affinity: all methods called from OverlayThread only.

#include <windows.h>

#include "../content/content_model.h"

namespace ai_overlay {

// What a click at a given point resolved to. Copy-button hit-testing is
// the only interactive content element in Phase 2 (05-DESIGN.md §4.2);
// more (wrap toggle, link clicks) can extend this struct later without
// changing the interface shape.
struct ContentHitResult {
  bool hit_copy_button = false;
  int block_index = -1;
};

// 02-ARCHITECTURE.md §7: "Both modes sit behind the same
// OverlayContentRenderer interface so the Dart layer doesn't need to
// know which is active beyond a settings toggle." Renderer selection
// (native vs. WebView2) happens once, at OverlayManager construction —
// this project does not hot-swap renderers on a live window.
class OverlayContentRenderer {
 public:
  virtual ~OverlayContentRenderer() = default;

  virtual bool Initialize(HWND hwnd) = 0;
  virtual void Shutdown() = 0;

  // Called whenever ContentModel's blocks changed. Implementations
  // decide their own repaint strategy — for Direct2D that means reading
  // ContentModel::TakeDirtyBlockIndices() and invalidating only those
  // blocks' screen rects; WebView2 patches its DOM via postMessage
  // instead, since "dirty rect" isn't a meaningful concept for an HTML
  // renderer.
  virtual void OnContentChanged(ContentModel& model) = 0;

  virtual void OnResize(int width, int height) = 0;

  // Called from WM_PAINT. Direct2D actually redraws here; WebView2's
  // no-op default is fine since the browser compositor repaints itself
  // continuously once content is pushed via OnContentChanged.
  virtual void RepaintNow(const ContentModel& model) { (void)model; }

  // `client_pt` is in client-area device pixels (i.e. already adjusted
  // for the window's DPI by the caller).
  virtual ContentHitResult HitTestContent(POINT client_pt) const = 0;

  // Returns the copyable text for `block_index` (empty if not a code
  // block or out of range) — caller (OverlayManager) writes it to the
  // clipboard via clipboard_util.h so that logic isn't duplicated in
  // every renderer implementation.
  virtual std::wstring GetCodeBlockText(int block_index) const = 0;
};

}  // namespace ai_overlay
