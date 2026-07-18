#include "overlay_manager.h"

#include <chrono>
#include <future>

#include "../logging/native_log.h"
#include "clipboard_util.h"
#include "renderer_d2d.h"
#ifdef ENABLE_WEBVIEW2_OVERLAY
#include "renderer_webview2.h"
#endif

namespace ai_overlay {

namespace {
OverlayManager* g_active_instance = nullptr;
}  // namespace

OverlayManager::OverlayManager(RendererMode renderer_mode) : renderer_mode_(renderer_mode) {
  if (g_active_instance != nullptr) {
    NativeLog::Warn("OverlayManager",
                     "A second OverlayManager was constructed; only one is "
                     "expected per process. ActiveInstance() will now point "
                     "at the newest one.");
  }
  g_active_instance = this;
}

OverlayManager* OverlayManager::ActiveInstance() { return g_active_instance; }

OverlayManager::~OverlayManager() {
  if (g_active_instance == this) g_active_instance = nullptr;
  if (running_.load()) {
    Stop();
  }
}

bool OverlayManager::Start() {
  if (running_.load()) {
    NativeLog::Warn("OverlayManager", "Start() called while already running.");
    return false;
  }

  running_.store(true);
  overlay_thread_ = std::thread(&OverlayManager::Run, this);
  NativeLog::Info("OverlayManager", "OverlayThread started.");
  return true;
}

void OverlayManager::Stop() {
  if (!running_.load()) return;

  running_.store(false);
  if (overlay_thread_id_ != 0) {
    // Wake GetMessage() so the loop notices running_ == false promptly
    // instead of waiting for the next natural message.
    ::PostThreadMessageW(overlay_thread_id_, WM_QUIT, 0, 0);
  }
  if (overlay_thread_.joinable()) {
    overlay_thread_.join();
  }
  NativeLog::Info("OverlayManager", "OverlayThread stopped.");
}

void OverlayManager::PostCommand(OverlayCommand command) {
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    command_queue_.push(std::move(command));
  }
  if (overlay_thread_id_ != 0) {
    ::PostThreadMessageW(overlay_thread_id_, kWakeMessage, 0, 0);
  }
}

void OverlayManager::DrainCommandQueue() {
  std::queue<OverlayCommand> local;
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    std::swap(local, command_queue_);
  }
  while (!local.empty()) {
    local.front()();
    local.pop();
  }
}

void OverlayManager::Show(WindowBounds bounds) {
  PostCommand([this, bounds]() {
    if (!window_ || !window_->is_created()) return;
    window_->SetBounds(bounds);
    ::ShowWindow(window_->hwnd(), SW_SHOWNOACTIVATE);
    if (animator_) {
      animator_->Start(
          Animator::OverlayShowSpec(),
          [this](double progress) {
            const BYTE alpha = static_cast<BYTE>(progress * 255.0);
            if (window_ && window_->is_created()) {
              ::SetLayeredWindowAttributes(window_->hwnd(), 0, alpha, LWA_ALPHA);
            }
          },
          nullptr);
    }
  });
}

void OverlayManager::Hide() {
  PostCommand([this]() {
    if (!window_ || !window_->is_created()) return;
    if (animator_) {
      animator_->Start(
          Animator::OverlayHideSpec(),
          [this](double progress) {
            const BYTE alpha = static_cast<BYTE>((1.0 - progress) * 255.0);
            if (window_ && window_->is_created()) {
              ::SetLayeredWindowAttributes(window_->hwnd(), 0, alpha, LWA_ALPHA);
            }
          },
          [this]() {
            if (window_ && window_->is_created()) {
              ::ShowWindow(window_->hwnd(), SW_HIDE);
            }
          });
    } else {
      ::ShowWindow(window_->hwnd(), SW_HIDE);
    }
  });
}

void OverlayManager::SetBounds(WindowBounds bounds) {
  PostCommand([this, bounds]() {
    if (window_) window_->SetBounds(bounds);
  });
}

void OverlayManager::SetClickThrough(bool enabled) {
  PostCommand([this, enabled]() {
    if (window_) window_->SetClickThrough(enabled);
  });
}

CaptureAffinityState OverlayManager::SetCaptureExcludedBlocking(bool enabled) {
  auto result_promise = std::make_shared<std::promise<CaptureAffinityState>>();
  std::future<CaptureAffinityState> result_future = result_promise->get_future();

  PostCommand([this, enabled, result_promise]() {
    const HWND hwnd = window_ ? window_->hwnd() : nullptr;
    result_promise->set_value(display_affinity_.Apply(hwnd, enabled));
  });

  // Bounded wait so a stalled overlay thread can't hang the Dart call
  // forever (02-ARCHITECTURE.md §10: watchdog territory) — on timeout we
  // report kNotSupported rather than block, honoring "Dart never assumes
  // success."
  if (result_future.wait_for(std::chrono::seconds(2)) != std::future_status::ready) {
    NativeLog::Warn("OverlayManager",
                     "SetCaptureExcludedBlocking timed out waiting on OverlayThread.");
    return CaptureAffinityState::kNotSupported;
  }
  return result_future.get();
}

void OverlayManager::RegisterHotkey(const HotkeyRegistration& registration) {
  PostCommand([this, registration]() {
    if (hotkey_manager_) {
      hotkey_manager_->Register(registration);
    }
  });
}

void OverlayManager::AppendContentToken(std::wstring token) {
  PostCommand([this, token = std::move(token)]() {
    content_model_.AppendToken(token);
    if (renderer_) {
      renderer_->OnContentChanged(content_model_);
      renderer_->RepaintNow(content_model_);
    }
  });
}

void OverlayManager::ResetContent() {
  PostCommand([this]() {
    content_model_.Reset();
    if (renderer_ && window_ && window_->is_created()) {
      ::InvalidateRect(window_->hwnd(), nullptr, TRUE);
    }
  });
}

void OverlayManager::Run() {
  overlay_thread_id_ = ::GetCurrentThreadId();

  // Force message-queue creation on this thread before any
  // PostThreadMessage from another thread can target it — documented
  // requirement of PostThreadMessage:
  // https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-postthreadmessagew
  MSG dummy;
  ::PeekMessage(&dummy, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

  hotkey_manager_ = std::make_unique<HotkeyManager>(
      [this](const std::string& action_id) {
        if (event_sink_.on_hotkey_pressed) {
          event_sink_.on_hotkey_pressed(action_id);
        }
      });

  window_ = std::make_unique<OverlayWindow>();
  window_->set_on_bounds_changed([this](const WindowBounds& bounds) {
    if (event_sink_.on_moved) event_sink_.on_moved(bounds);
  });
  window_->set_on_dpi_changed([this](double scale) {
    if (event_sink_.on_dpi_changed) event_sink_.on_dpi_changed(scale);
  });
  window_->set_on_paint([this]() { OnWindowPaint(); });
  window_->set_on_click([this](POINT pt) { OnWindowClick(pt); });
  window_->set_on_resize([this](int w, int h) { OnWindowResize(w, h); });

  if (!window_->Create(WindowBounds{}, L"AI Overlay Assistant")) {
    NativeLog::ErrorWithLastError("OverlayManager", "OverlayWindow::Create failed");
    running_.store(false);
    return;
  }
  animator_ = std::make_unique<Animator>(window_->hwnd());

#ifdef ENABLE_WEBVIEW2_OVERLAY
  if (renderer_mode_ == RendererMode::kWebView2) {
    renderer_ = std::make_unique<RendererWebView2>();
  } else {
    renderer_ = std::make_unique<RendererD2D>();
  }
#else
  if (renderer_mode_ == RendererMode::kWebView2) {
    NativeLog::Warn("OverlayManager",
                     "WebView2 mode requested but ENABLE_WEBVIEW2_OVERLAY wasn't "
                     "compiled in; falling back to native Direct2D rendering.");
  }
  renderer_ = std::make_unique<RendererD2D>();
#endif
  if (!renderer_->Initialize(window_->hwnd())) {
    NativeLog::Warn("OverlayManager", "Content renderer Initialize() failed.");
  }

  MSG msg;
  while (running_.load()) {
    const BOOL got = ::GetMessageW(&msg, nullptr, 0, 0);
    if (got <= 0) break;  // WM_QUIT or error.

    if (msg.message == kWakeMessage) {
      DrainCommandQueue();
      continue;
    }

    if (msg.hwnd == nullptr && msg.message == WM_HOTKEY) {
      hotkey_manager_->HandleHotkeyMessage(msg.wParam);
      continue;
    }

    ::TranslateMessage(&msg);
    ::DispatchMessageW(&msg);
  }

  // Teardown happens on this thread — matches creation thread
  // (03-RULES.md §3 thread-affinity discipline).
  if (renderer_) renderer_->Shutdown();
  renderer_.reset();
  animator_.reset();
  window_.reset();
  hotkey_manager_.reset();
}

void OverlayManager::OnWindowPaint() {
  if (renderer_) {
    renderer_->RepaintNow(content_model_);
  }
}

void OverlayManager::OnWindowClick(POINT client_pt) {
  if (!renderer_) return;
  const ContentHitResult hit = renderer_->HitTestContent(client_pt);
  if (!hit.hit_copy_button) return;

  const std::wstring code_text = renderer_->GetCodeBlockText(hit.block_index);
  if (code_text.empty()) return;

  const HWND hwnd = window_ ? window_->hwnd() : nullptr;
  if (hwnd != nullptr) {
    // 05-DESIGN.md §4.2: copy button transitions to a checkmark for 1.5s
    // — that transient UI state is the renderer's job (a future pass on
    // RendererD2D/RendererWebView2), triggered by this successful copy;
    // this call only owns the actual clipboard write.
    WriteTextToClipboard(hwnd, code_text);
  }
}

void OverlayManager::OnWindowResize(int width, int height) {
  if (renderer_) {
    renderer_->OnResize(width, height);
  }
}

}  // namespace ai_overlay
