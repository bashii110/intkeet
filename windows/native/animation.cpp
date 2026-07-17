#include "animation.h"

#include <unordered_map>

#include "../logging/native_log.h"

namespace ai_overlay {

namespace {
// idEvent -> Animator*, so the free-function TimerProc callback can
// dispatch to the right instance. Only ever touched from OverlayThread
// (see header thread-affinity comment), so no lock is needed.
std::unordered_map<UINT_PTR, Animator*>& Registry() {
  static std::unordered_map<UINT_PTR, Animator*> registry;
  return registry;
}
}  // namespace

Animator::Animator(HWND target) : target_(target) {}

Animator::~Animator() { Cancel(); }

bool Animator::OsAnimationsEnabled() {
  // 05-DESIGN.md §2.4: "Respect OS 'Show animations in Windows' setting."
  // SPI_GETCLIENTAREAANIMATION reflects that toggle (Windows Vista+):
  // https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-systemparametersinfow
  BOOL enabled = TRUE;
  if (!::SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0)) {
    NativeLog::Warn("Animator", "SystemParametersInfoW query failed; assuming animations on.");
    return true;
  }
  return enabled != FALSE;
}

double Animator::ApplyEasing(Easing easing, double t) {
  switch (easing) {
    case Easing::kEaseOut:
      return 1.0 - (1.0 - t) * (1.0 - t);  // decelerate
    case Easing::kEaseIn:
      return t * t;  // accelerate
    case Easing::kLinear:
    default:
      return t;
  }
}

void Animator::Start(AnimationSpec spec, ProgressCallback on_progress,
                      CompletionCallback on_complete) {
  Cancel();  // Only one animation per Animator instance at a time.

  spec_ = spec;
  on_progress_ = std::move(on_progress);
  on_complete_ = std::move(on_complete);

  if (!OsAnimationsEnabled() || spec_.duration_ms <= 0) {
    // Reduced-motion path: fire once at progress = 1.0, no timer.
    if (on_progress_) on_progress_(1.0);
    if (on_complete_) on_complete_();
    return;
  }

  start_tick_ms_ = ::GetTickCount64();
  timer_id_ = reinterpret_cast<UINT_PTR>(this);
  Registry()[timer_id_] = this;
  running_ = true;

  // ~60 FPS tick (05-DESIGN.md/03-RULES.md §5 animation target). Uses
  // the classic SetTimer overload with an explicit TIMERPROC so this
  // doesn't rely on WM_TIMER being pumped by a specific WndProc:
  // https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-settimer
  ::SetTimer(target_, timer_id_, 16, &Animator::TimerProcThunk);
}

void Animator::Cancel() {
  if (!running_) return;
  ::KillTimer(target_, timer_id_);
  Registry().erase(timer_id_);
  running_ = false;
}

void CALLBACK Animator::TimerProcThunk(HWND hwnd, UINT msg, UINT_PTR id, DWORD time_ms) {
  (void)hwnd;
  (void)msg;
  (void)time_ms;
  const auto it = Registry().find(id);
  if (it == Registry().end()) return;
  it->second->OnTick();
}

void Animator::OnTick() {
  const ULONGLONG elapsed = ::GetTickCount64() - start_tick_ms_;
  const double linear_progress =
      static_cast<double>(elapsed) / static_cast<double>(spec_.duration_ms);

  if (linear_progress >= 1.0) {
    if (on_progress_) on_progress_(1.0);
    Cancel();
    if (on_complete_) on_complete_();
    return;
  }

  if (on_progress_) {
    on_progress_(ApplyEasing(spec_.easing, linear_progress));
  }
}

}  // namespace ai_overlay
