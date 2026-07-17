#include "method_channel_handlers.h"

#include "../logging/native_log.h"

namespace ai_overlay {

namespace {
constexpr char kMethodChannelName[] = "ai_overlay_assistant/overlay";
constexpr char kEventChannelName[] = "ai_overlay_assistant/overlay_events";

using flutter::EncodableMap;
using flutter::EncodableValue;

// Pulls a numeric field out of an EncodableMap `bounds` argument shaped
// like {"x": int, "y": int, "width": int, "height": int} — the
// documented shape both sides agree on (03-RULES.md §4: "MethodChannel
// calls always specify explicit types; no passing raw Map without a
// documented shape" — this is that documented shape, mirrored in
// overlay_bridge.dart's Rect (de)serialization).
WindowBounds ParseBounds(const EncodableMap& map) {
  WindowBounds bounds;
  auto get_int = [&map](const char* key, int32_t fallback) -> int32_t {
    const auto it = map.find(EncodableValue(std::string(key)));
    if (it == map.end()) return fallback;
    return std::get<int32_t>(it->second);
  };
  bounds.x = get_int("x", 0);
  bounds.y = get_int("y", 0);
  bounds.width = get_int("width", bounds.width);
  bounds.height = get_int("height", bounds.height);
  return bounds;
}
}  // namespace

void OverlayMethodChannelPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows* registrar) {
  auto plugin = std::make_unique<OverlayMethodChannelPlugin>(registrar);
  registrar->AddPlugin(std::move(plugin));
}

OverlayMethodChannelPlugin::OverlayMethodChannelPlugin(
    flutter::PluginRegistrarWindows* registrar)
    : registrar_(registrar) {
  method_channel_ = std::make_unique<flutter::MethodChannel<EncodableValue>>(
      registrar_->messenger(), kMethodChannelName,
      &flutter::StandardMethodCodec::GetInstance());

  method_channel_->SetMethodCallHandler(
      [this](const auto& call, auto result) {
        HandleMethodCall(call, std::move(result));
      });

  event_channel_ = std::make_unique<flutter::EventChannel<EncodableValue>>(
      registrar_->messenger(), kEventChannelName,
      &flutter::StandardMethodCodec::GetInstance());

  auto handler = std::make_unique<flutter::StreamHandlerFunctions<EncodableValue>>(
      [this](const EncodableValue*,
             std::unique_ptr<flutter::EventSink<EncodableValue>>&& sink)
          -> std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> {
        event_sink_ = std::move(sink);
        return nullptr;
      },
      [this](const EncodableValue*)
          -> std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> {
        event_sink_.reset();
        return nullptr;
      });
  event_channel_->SetStreamHandler(std::move(handler));

  OverlayEventSink sink;
  // NOTE: these callbacks fire on OverlayThread, but EventSink::Success
  // must be called on the platform thread in Flutter's Windows embedder.
  // The exact thread-marshaling call (task runner post vs. a documented
  // thread-safe messenger send) depends on the embedder version pinned
  // by the Flutter SDK this is built against — verify against that
  // version's flutter/plugin_registrar_windows.h on the dev machine and
  // wrap the three Success() calls below in the correct marshal before
  // this ships; flagged here rather than guessed at.
  sink.on_hotkey_pressed = [this](const std::string& action_id) {
    if (!event_sink_) return;
    EncodableMap event{{EncodableValue("type"), EncodableValue("hotkeyPressed")},
                        {EncodableValue("actionId"), EncodableValue(action_id)}};
    event_sink_->Success(EncodableValue(event));
  };
  sink.on_moved = [this](const WindowBounds& bounds) {
    if (!event_sink_) return;
    EncodableMap event{
        {EncodableValue("type"), EncodableValue("overlayMoved")},
        {EncodableValue("x"), EncodableValue(bounds.x)},
        {EncodableValue("y"), EncodableValue(bounds.y)},
        {EncodableValue("width"), EncodableValue(bounds.width)},
        {EncodableValue("height"), EncodableValue(bounds.height)},
    };
    event_sink_->Success(EncodableValue(event));
  };
  sink.on_dpi_changed = [this](double scale_factor) {
    if (!event_sink_) return;
    EncodableMap event{{EncodableValue("type"), EncodableValue("dpiChanged")},
                        {EncodableValue("scaleFactor"), EncodableValue(scale_factor)}};
    event_sink_->Success(EncodableValue(event));
  };
  manager_.set_event_sink(std::move(sink));

  if (!manager_.Start()) {
    NativeLog::ErrorWithLastError("OverlayMethodChannelPlugin",
                                   "OverlayManager::Start failed at plugin registration");
  }
}

OverlayMethodChannelPlugin::~OverlayMethodChannelPlugin() { manager_.Stop(); }

void OverlayMethodChannelPlugin::HandleMethodCall(
    const flutter::MethodCall<EncodableValue>& call,
    std::unique_ptr<flutter::MethodResult<EncodableValue>> result) {
  const std::string& method = call.method_name();
  const auto* args = std::get_if<EncodableMap>(call.arguments());

  if (method == "show") {
    WindowBounds bounds;
    if (args) bounds = ParseBounds(*args);
    manager_.Show(bounds);
    result->Success();
  } else if (method == "hide") {
    manager_.Hide();
    result->Success();
  } else if (method == "setBounds") {
    if (!args) {
      result->Error("bad_args", "setBounds requires a bounds map");
      return;
    }
    manager_.SetBounds(ParseBounds(*args));
    result->Success();
  } else if (method == "setClickThrough") {
    const auto it = args ? args->find(EncodableValue(std::string("enabled")))
                          : EncodableMap::const_iterator{};
    const bool enabled = (args && it != args->end()) ? std::get<bool>(it->second) : false;
    manager_.SetClickThrough(enabled);
    result->Success();
  } else if (method == "setCaptureExcluded") {
    const auto it = args ? args->find(EncodableValue(std::string("enabled")))
                          : EncodableMap::const_iterator{};
    const bool enabled = (args && it != args->end()) ? std::get<bool>(it->second) : false;
    const CaptureAffinityState state = manager_.SetCaptureExcludedBlocking(enabled);
    result->Success(EncodableValue(state == CaptureAffinityState::kEnabled));
  } else if (method == "setOpacity") {
    // Phase 1: opacity is currently only driven by the show/hide
    // animator; a direct setOpacity() passthrough on OverlayManager is
    // added when the content renderer (Phase 2) needs it independently
    // of animation state.
    result->Error("not_implemented", "setOpacity lands with the Phase 2 renderer.");
  } else {
    result->NotImplemented();
  }
}

}  // namespace ai_overlay
