#pragma once
// Flat extern "C" API surface for Dart FFI (02-ARCHITECTURE.md §5).
//
// Phase 1: OverlayManager's lifecycle is owned by
// OverlayMethodChannelPlugin (registered at runner startup), reached via
// the MethodChannel transport in method_channel_handlers.cpp — that's
// the "discrete calls" path per the architecture doc. This FFI surface
// is reserved for Phase 2's high-frequency streaming-content path, which
// needs FFI's lower call overhead; it deliberately does NOT duplicate
// show/hide/setBounds here to avoid two owners of window state.
//
// For now this only exposes the schema-version handshake so Dart can
// detect a native/Dart mismatch at startup (03-RULES.md §4).

#include <cstdint>

extern "C" {

// Must equal kBridgeSchemaVersion in overlay_bridge.dart.
__declspec(dllexport) int32_t AiOverlay_GetBridgeSchemaVersion();

}  // extern "C"
