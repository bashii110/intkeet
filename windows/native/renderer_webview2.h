#pragma once
// Optional WebView2-hosted content renderer (02-ARCHITECTURE.md §7).
// Behind the ENABLE_WEBVIEW2_OVERLAY feature flag per 03-RULES.md §6
// ("Feature flags for anything experimental ... starts behind a flag
// until it meets the perf budget documented for it" — +70-100MB memory
// cost per 02-ARCHITECTURE.md §7, an explicit opt-in trade-off).
//
// Requires the WebView2 SDK (Microsoft.Web.WebView2 NuGet package or the
// standalone SDK) — this is NOT part of the in-box Windows SDK, unlike
// every other header in this project. windows/CMakeLists.txt only
// compiles this file when that SDK is actually present, mirroring how
// method_channel_handlers.cpp is guarded on flutter_wrapper_plugin.
#ifdef ENABLE_WEBVIEW2_OVERLAY

#include <WebView2.h>
#include <wrl/client.h>

#include <string>
#include <vector>

#include "overlay_content_renderer.h"

namespace ai_overlay {

// The Direct2D renderer computes pixel-precise copy-button hit-rects
// itself (renderer_d2d.cpp); WebView2 instead lets the DOM handle click
// targets and reports clicks back via a documented `postMessage` bridge
// (ICoreWebView2::add_WebMessageReceived) rather than native hit-testing
// — HitTestContent() on this class is therefore always a no-op stub, and
// interaction is driven by OnWebMessageReceived instead.
class RendererWebView2 : public OverlayContentRenderer {
 public:
  RendererWebView2() = default;
  ~RendererWebView2() override;

  bool Initialize(HWND hwnd) override;
  void Shutdown() override;
  void OnContentChanged(ContentModel& model) override;
  void OnResize(int width, int height) override;
  ContentHitResult HitTestContent(POINT client_pt) const override;
  std::wstring GetCodeBlockText(int block_index) const override;

 private:
  // Serializes ContentModel's blocks to a small JSON-ish payload and
  // pushes it into the page via ExecuteScript, calling a
  // `window.renderBlocks(...)` function the bundled HTML/JS defines.
  // Documented API:
  // https://learn.microsoft.com/microsoft-edge/webview2/reference/win32/icorewebview2#executescript
  void PushContentToPage();
  std::wstring SerializeBlocksToJson() const;

  void OnEnvironmentCreated(ICoreWebView2Environment* env);
  void OnControllerCreated(ICoreWebView2Controller* controller);
  void OnWebMessageReceived(const std::wstring& json_message);

  HWND hwnd_ = nullptr;
  Microsoft::WRL::ComPtr<ICoreWebView2Environment> environment_;
  Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
  Microsoft::WRL::ComPtr<ICoreWebView2> webview_;

  std::vector<Block> last_blocks_;
  int last_copy_clicked_block_index_ = -1;
};

}  // namespace ai_overlay

#endif  // ENABLE_WEBVIEW2_OVERLAY
