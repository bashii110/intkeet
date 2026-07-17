#include "hotkey_manager.h"

#include "../logging/native_log.h"

namespace ai_overlay {

HotkeyManager::HotkeyManager(TriggerCallback on_triggered)
    : on_triggered_(std::move(on_triggered)) {}

HotkeyManager::~HotkeyManager() {
  for (const auto& [id, registration] : registrations_) {
    (void)registration;
    Unregister(id);
  }
}

bool HotkeyManager::Register(const HotkeyRegistration& registration) {
  NativeLog::Warn("HotkeyManager",
                   "Register() is a Phase 0 stub for action_id=" +
                       registration.action_id);
  return false;
}

bool HotkeyManager::Unregister(int32_t id) {
  const auto it = registrations_.find(id);
  if (it == registrations_.end()) return false;
  registrations_.erase(it);
  return false;  // Phase 0: no real UnregisterHotKey call yet.
}

void HotkeyManager::HandleHotkeyMessage(WPARAM wparam) {
  const auto it = registrations_.find(static_cast<int32_t>(wparam));
  if (it == registrations_.end()) return;
  if (on_triggered_) {
    on_triggered_(it->second.action_id);
  }
}

}  // namespace ai_overlay
