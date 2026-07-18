#include "overlay_api.h"

#include <string>

#include "../native/overlay_manager.h"

extern "C" {

int32_t AiOverlay_GetBridgeSchemaVersion() {
  return 1;  // Keep in lockstep with kBridgeSchemaVersion (Dart side).
}

int32_t AiOverlay_AppendContentToken(const wchar_t* utf16_token) {
  ai_overlay::OverlayManager* manager = ai_overlay::OverlayManager::ActiveInstance();
  if (manager == nullptr || utf16_token == nullptr) return 0;
  manager->AppendContentToken(std::wstring(utf16_token));
  return 1;
}

void AiOverlay_ResetContent() {
  ai_overlay::OverlayManager* manager = ai_overlay::OverlayManager::ActiveInstance();
  if (manager != nullptr) {
    manager->ResetContent();
  }
}

}  // extern "C"
