#pragma once
// Flutter Windows embedder plugin API (flutter/*.h below) — this is
// Flutter's own documented C++ plugin surface, not a raw Win32/COM call,
// so it sits outside the "MS Learn only" rule in 03-RULES.md §1; every
// Win32 call it eventually triggers happens inside OverlayManager, which
// is covered by that rule already.

#include <flutter/event_channel.h>
#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <memory>

#include "../native/overlay_manager.h"

namespace ai_overlay {

// Registered from the runner at startup (04-PHASES.md Phase 0: "runner/
// ... boot OverlayManager at startup"). Owns the single process-wide
// OverlayManager and translates MethodChannel calls into its thread-safe
// command API, and OverlayManager's event sink into EventChannel events.
class OverlayMethodChannelPlugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows* registrar);

  explicit OverlayMethodChannelPlugin(flutter::PluginRegistrarWindows* registrar);
  ~OverlayMethodChannelPlugin();

 private:
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue>& call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

  flutter::PluginRegistrarWindows* registrar_;
  std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> method_channel_;
  std::unique_ptr<flutter::EventChannel<flutter::EncodableValue>> event_channel_;
  std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> event_sink_;

  OverlayManager manager_;
};

}  // namespace ai_overlay
