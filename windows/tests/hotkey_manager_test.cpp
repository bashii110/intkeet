#include "../hotkeys/hotkey_manager.h"

#include <gtest/gtest.h>

TEST(HotkeyManagerTest, RegisterStubReturnsFalseAndDoesNotThrow) {
  bool triggered = false;
  ai_overlay::HotkeyManager manager(
      [&triggered](const std::string&) { triggered = true; });

  ai_overlay::HotkeyRegistration reg;
  reg.id = 1;
  reg.action_id = "invokeOverlay";

  EXPECT_FALSE(manager.Register(reg));
  EXPECT_FALSE(triggered);
}

TEST(HotkeyManagerTest, UnregisterUnknownIdReturnsFalse) {
  ai_overlay::HotkeyManager manager([](const std::string&) {});
  EXPECT_FALSE(manager.Unregister(999));
}

TEST(HotkeyManagerTest, DestructorDoesNotCrashWithNoRegistrations) {
  { ai_overlay::HotkeyManager manager([](const std::string&) {}); }
  SUCCEED();
}
