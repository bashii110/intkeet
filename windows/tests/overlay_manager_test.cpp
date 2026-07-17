#include "../native/overlay_manager.h"

#include <gtest/gtest.h>

// Phase 0: only thread lifecycle is real, so these tests only assert
// Start()/Stop() behave correctly (no leaked thread, idempotent Stop(),
// double-Start() rejected). Message-loop/window-creation behavior gets
// its own tests once Phase 1 implements Run() for real.

TEST(OverlayManagerTest, StartsAndStopsCleanly) {
  ai_overlay::OverlayManager manager;
  EXPECT_TRUE(manager.Start());
  EXPECT_TRUE(manager.is_running());
  manager.Stop();
  EXPECT_FALSE(manager.is_running());
}

TEST(OverlayManagerTest, DoubleStartFails) {
  ai_overlay::OverlayManager manager;
  ASSERT_TRUE(manager.Start());
  EXPECT_FALSE(manager.Start()) << "Start() must reject re-entry while running";
  manager.Stop();
}

TEST(OverlayManagerTest, DestructorStopsRunningManager) {
  {
    ai_overlay::OverlayManager manager;
    ASSERT_TRUE(manager.Start());
    // No explicit Stop() — destructor must not leak the thread/hang.
  }
  SUCCEED();
}

TEST(OverlayManagerTest, PostCommandBeforeStartDoesNotCrash) {
  ai_overlay::OverlayManager manager;
  bool ran = false;
  manager.PostCommand([&ran]() { ran = true; });
  // Phase 0: queue isn't drained yet, so `ran` staying false is expected
  // and asserted explicitly so this test starts failing (as it should)
  // the moment Phase 1 wires up draining, prompting an update here.
  EXPECT_FALSE(ran);
}
