#pragma once
// Flat extern "C" API surface for Dart FFI (02-ARCHITECTURE.md §5).
//
// Discrete calls (show/hide/setBounds/setClickThrough/setCaptureExcluded)
// go through the MethodChannel transport in method_channel_handlers.cpp.
// This file is Phase 2's high-frequency streaming path: appending a
// content token bypasses MethodChannel's JSON (de)serialization
// overhead, which matters at 50+ tokens/sec.
//
// Both transports ultimately reach the same OverlayManager instance via
// OverlayManager::ActiveInstance() — see that method's doc comment.

#include <cstdint>

extern "C" {

// Must equal kBridgeSchemaVersion in overlay_bridge.dart.
__declspec(dllexport) int32_t AiOverlay_GetBridgeSchemaVersion();

// Appends one streamed token to the active overlay's content. `utf16_token`
// must be a null-terminated UTF-16LE string (Dart's ffi package's
// `toNativeUtf16()` produces exactly this). Returns 0 if no OverlayManager
// is active yet (e.g. called before the native plugin finished
// registering) — Dart must not assume success (02-ARCHITECTURE.md §10).
__declspec(dllexport) int32_t AiOverlay_AppendContentToken(const wchar_t* utf16_token);

// Clears the current message's content (new conversation turn / overlay
// reopened). Same "no active instance yet" caveat as above, silently
// no-op'd rather than crashing.
__declspec(dllexport) void AiOverlay_ResetContent();

}  // extern "C"
