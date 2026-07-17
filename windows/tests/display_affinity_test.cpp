#include "../capture/display_affinity.h"

#include <gtest/gtest.h>

// 03-RULES.md §1 rule 2: this controller must never silently claim
// success. IsSupportedOnThisOs() is a real OS-build check (Win10 2004+ /
// build 19041+), so its expected value depends on the CI runner's OS —
// these tests instead pin down the "never trust the request, always
// re-query" contract, which must hold regardless of OS build.

TEST(DisplayAffinityControllerTest, NullHwndIsAlwaysReportedAsNotSupported) {
  ai_overlay::DisplayAffinityController controller;
  // A null HWND can never succeed — this must never be echoed back as
  // kEnabled just because the OS build supports the flag in principle.
  const auto state = controller.Apply(nullptr, /*exclude_from_capture=*/true);
  EXPECT_EQ(state, ai_overlay::CaptureAffinityState::kNotSupported);
  EXPECT_EQ(controller.current_state(), state);
}

TEST(DisplayAffinityControllerTest, IsSupportedOnThisOsDoesNotThrow) {
  ai_overlay::DisplayAffinityController controller;
  // Smoke test only — actual value is OS-build-dependent and asserted
  // against real applied state via the Win32 test-matrix run manually
  // per 04-PHASES.md Phase 1 gate, not here.
  (void)controller.IsSupportedOnThisOs();
  SUCCEED();
}
