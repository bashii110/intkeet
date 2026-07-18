#ifdef ENABLE_WEBVIEW2_OVERLAY

#include "renderer_webview2.h"

#include <sstream>

#include "../logging/native_log.h"
#include "clipboard_util.h"

namespace ai_overlay {

RendererWebView2::~RendererWebView2() { Shutdown(); }

bool RendererWebView2::Initialize(HWND hwnd) {
  hwnd_ = hwnd;

  // CreateCoreWebView2EnvironmentWithOptions is async by design (it may
  // need to provision the WebView2 Runtime); the callback completes
  // environment/controller setup. Documented at:
  // https://learn.microsoft.com/microsoft-edge/webview2/reference/win32/webview2-idl#createcorewebview2environmentwithoptions
  const HRESULT hr = ::CreateCoreWebView2EnvironmentWithOptions(
      nullptr, nullptr, nullptr,
      Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
          [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
            if (FAILED(result) || env == nullptr) {
              NativeLog::Warn("RendererWebView2",
                               "CreateCoreWebView2EnvironmentWithOptions failed");
              return result;
            }
            OnEnvironmentCreated(env);
            return S_OK;
          })
          .Get());

  if (FAILED(hr)) {
    NativeLog::Warn("RendererWebView2", "CreateCoreWebView2EnvironmentWithOptions call failed");
    return false;
  }
  // Initialization completes asynchronously; OverlayManager should treat
  // this renderer as "not yet ready" until OnControllerCreated fires.
  // (Wiring that readiness signal into OverlayManager's command queue is
  // a follow-up once WebView2 mode is exercised on a real machine with
  // the SDK installed — this sandbox cannot compile or run this file at
  // all, per the header comment.)
  return true;
}

void RendererWebView2::OnEnvironmentCreated(ICoreWebView2Environment* env) {
  environment_ = env;
  env->CreateCoreWebView2Controller(
      hwnd_,
      Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
          [this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
            if (FAILED(result) || controller == nullptr) {
              NativeLog::Warn("RendererWebView2", "CreateCoreWebView2Controller failed");
              return result;
            }
            OnControllerCreated(controller);
            return S_OK;
          })
          .Get());
}

void RendererWebView2::OnControllerCreated(ICoreWebView2Controller* controller) {
  controller_ = controller;
  controller_->get_CoreWebView2(&webview_);

  RECT bounds{};
  ::GetClientRect(hwnd_, &bounds);
  controller_->put_Bounds(bounds);

  // Bundled asset shipped alongside the app (e.g.
  // %LOCALAPPDATA%\AiOverlayAssistant\web\overlay_content.html) defines
  // window.renderBlocks(json) and posts back {"type":"copyClicked",
  // "blockIndex": N} via window.chrome.webview.postMessage on click —
  // documented API:
  // https://learn.microsoft.com/microsoft-edge/webview2/reference/win32/icorewebview2#navigate
  webview_->Navigate(L"file:///AiOverlayAssistant/web/overlay_content.html");

  webview_->add_WebMessageReceived(
      Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
          [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
            LPWSTR message = nullptr;
            if (SUCCEEDED(args->TryGetWebMessageAsString(&message)) && message != nullptr) {
              OnWebMessageReceived(message);
              ::CoTaskMemFree(message);
            }
            return S_OK;
          })
          .Get(),
      nullptr);
}

void RendererWebView2::OnWebMessageReceived(const std::wstring& json_message) {
  // Minimal parse: this project's message shape is fixed and small
  // ({"type":"copyClicked","blockIndex":N}), so a tiny hand-rolled
  // extraction is used rather than pulling in a JSON library for one
  // field — consistent with 03-RULES.md's "documented APIs only" spirit
  // applied to dependency footprint, not just Win32 calls.
  const std::wstring key = L"\"blockIndex\":";
  const size_t pos = json_message.find(key);
  if (pos == std::wstring::npos) return;
  last_copy_clicked_block_index_ = std::stoi(json_message.substr(pos + key.size()));

  // IMPORTANT: unlike RendererD2D, this click never passes through
  // OverlayWindow::WM_LBUTTONUP / OverlayManager::OnWindowClick — the
  // WebView2 control is its own child HWND that owns its input, so the
  // parent overlay window's WndProc never sees it. The copy action must
  // therefore be completed right here rather than relying on the
  // hit-test-then-copy flow OverlayManager uses for native mode.
  const std::wstring code_text = GetCodeBlockText(last_copy_clicked_block_index_);
  if (!code_text.empty()) {
    WriteTextToClipboard(hwnd_, code_text);
  }
}

void RendererWebView2::Shutdown() {
  webview_.Reset();
  controller_.Reset();
  environment_.Reset();
  hwnd_ = nullptr;
}

std::wstring RendererWebView2::SerializeBlocksToJson() const {
  // Hand-rolled minimal JSON, matching the fixed shape
  // overlay_content.html's renderBlocks() expects. Escaping here only
  // covers quote/backslash/newline, sufficient for AI-generated
  // markdown text; revisit with a real JSON writer if that assumption
  // breaks in practice.
  auto escape = [](const std::wstring& s) {
    std::wstring out;
    for (wchar_t c : s) {
      if (c == L'"' || c == L'\\') out += L'\\';
      if (c == L'\n') { out += L"\\n"; continue; }
      out += c;
    }
    return out;
  };

  std::wstringstream json;
  json << L"[";
  for (size_t i = 0; i < last_blocks_.size(); ++i) {
    const auto& block = last_blocks_[i];
    if (i > 0) json << L",";
    json << L"{\"kind\":" << static_cast<int>(block.kind) << L",\"language\":\""
         << escape(block.code_language) << L"\",\"lines\":[";
    for (size_t j = 0; j < block.code_lines.size(); ++j) {
      if (j > 0) json << L",";
      json << L"\"" << escape(block.code_lines[j]) << L"\"";
    }
    json << L"],\"text\":\"";
    for (const auto& run : block.runs) json << escape(run.text);
    json << L"\"}";
  }
  json << L"]";
  return json.str();
}

void RendererWebView2::OnContentChanged(ContentModel& model) {
  // Unlike RendererD2D, this doesn't consume TakeDirtyBlockIndices() —
  // WebView2's DOM diffing is the browser engine's job, not ours; we
  // just push the full current block list each time and let
  // renderBlocks() (JS side) do a keyed re-render, which is standard
  // practice for this rendering approach.
  last_blocks_ = model.blocks();
  PushContentToPage();
}

void RendererWebView2::PushContentToPage() {
  if (!webview_) return;
  const std::wstring script = L"window.renderBlocks(" + SerializeBlocksToJson() + L");";
  webview_->ExecuteScript(script.c_str(), nullptr);
}

void RendererWebView2::OnResize(int width, int height) {
  if (!controller_) return;
  RECT bounds{0, 0, width, height};
  controller_->put_Bounds(bounds);
}

ContentHitResult RendererWebView2::HitTestContent(POINT) const {
  // See header comment: WebView2 mode reports clicks via
  // OnWebMessageReceived, not native hit-testing.
  return {};
}

std::wstring RendererWebView2::GetCodeBlockText(int block_index) const {
  if (block_index < 0 || static_cast<size_t>(block_index) >= last_blocks_.size()) return L"";
  const Block& block = last_blocks_[static_cast<size_t>(block_index)];
  if (block.kind != BlockKind::kCodeBlock) return L"";
  std::wstring joined;
  for (size_t i = 0; i < block.code_lines.size(); ++i) {
    joined += block.code_lines[i];
    if (i + 1 < block.code_lines.size()) joined += L"\r\n";
  }
  return joined;
}

}  // namespace ai_overlay

#endif  // ENABLE_WEBVIEW2_OVERLAY
