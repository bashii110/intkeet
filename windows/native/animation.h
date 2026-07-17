#pragma once
// Thread-affinity: must be driven from the OverlayThread (it uses
// SetTimer, which requires a message loop on the calling thread).

#include <windows.h>

#include <cstdint>
#include <functional>

namespace ai_overlay {

enum class Easing { kLinear, kEaseOut, kEaseIn };

struct AnimationSpec {
  int32_t duration_ms = 160;
  Easing easing = Easing::kEaseOut;
};

// Timer/DirectComposition-driven animator (02-ARCHITECTURE.md §4).
// Phase 1 drives window opacity only (via SetLayeredWindowAttributes)
// since there's no content renderer yet — 05-DESIGN.md's per-interaction
// durations table is reproduced as named factory methods so call sites
// don't hardcode magic numbers (03-RULES.md §5).
class Animator {
 public:
  using ProgressCallback = std::function<void(double eased_progress)>;
  using CompletionCallback = std::function<void()>;

  explicit Animator(HWND target);
  ~Animator();

  Animator(const Animator&) = delete;
  Animator& operator=(const Animator&) = delete;

  // 05-DESIGN.md §2.4 named presets:
  static AnimationSpec OverlayShowSpec() { return {160, Easing::kEaseOut}; }
  static AnimationSpec OverlayHideSpec() { return {120, Easing::kEaseIn}; }
  static AnimationSpec AutoHideFadeSpec() { return {400, Easing::kLinear}; }

  // Starts an animation. If the OS "Show animations in Windows" setting
  // is off (SPI_GETCLIENTAREAANIMATION), duration collapses to 0 and
  // `on_progress`/`on_complete` fire once, synchronously, with progress
  // = 1.0 — matching the reduced-motion contract in 05-DESIGN.md §2.4.
  void Start(AnimationSpec spec, ProgressCallback on_progress,
             CompletionCallback on_complete);

  void Cancel();

 private:
  static void CALLBACK TimerProcThunk(HWND hwnd, UINT msg, UINT_PTR id, DWORD time_ms);
  void OnTick();

  static bool OsAnimationsEnabled();
  static double ApplyEasing(Easing easing, double linear_progress);

  HWND target_;
  UINT_PTR timer_id_ = 0;
  ULONGLONG start_tick_ms_ = 0;
  AnimationSpec spec_;
  ProgressCallback on_progress_;
  CompletionCallback on_complete_;
  bool running_ = false;
};

}  // namespace ai_overlay
