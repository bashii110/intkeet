#include "overlay_api.h"

extern "C" {

int32_t AiOverlay_GetBridgeSchemaVersion() {
  return 1;  // Keep in lockstep with kBridgeSchemaVersion (Dart side).
}

}  // extern "C"
