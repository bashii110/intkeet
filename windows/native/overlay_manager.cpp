#include "overlay_manager.h"

#include <chrono>
#include <future>

#include "../logging/native_log.h"

namespace ai_overlay {

OverlayManager::~OverlayManager() {
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

  if (!window_->Create(WindowBounds{}, L"AI Overlay Assistant")) {
    NativeLog::ErrorWithLastError("OverlayManager", "OverlayWindow::Create failed");
    running_.store(false);
    return;
  }
  animator_ = std::make_unique<Animator>(window_->hwnd());

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
  animator_.reset();
  window_.reset();
  hotkey_manager_.reset();
}

}  // namespace ai_overlay
