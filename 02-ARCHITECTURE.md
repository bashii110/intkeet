# Architecture Document
## Windows AI Overlay Assistant

---

## 1. High-Level System View

```
┌─────────────────────────────────────────────────────────────────┐
│                         User's Windows Desktop                   │
│                                                                    │
│  ┌───────────────────────────┐      ┌───────────────────────┐   │
│  │   Flutter Host Process     │      │  Native Overlay Window │   │
│  │   (app.exe, GPU-accel.     │◄────►│  (Win32 top-level      │   │
│  │    Flutter engine window,  │ FFI/ │   HWND, Direct2D or    │   │
│  │    normally hidden/tray)   │ IPC  │   WebView2 surface)    │   │
│  │                            │      │                        │   │
│  │  - Settings UI             │      │  - Window chrome       │   │
│  │  - Conversation UI         │      │  - Drag/resize         │   │
│  │  - Provider adapters       │      │  - Always-on-top       │   │
│  │  - SQLite / secure store   │      │  - Click-through       │   │
│  │  - Hotkey config (data)    │      │  - Blur/acrylic        │   │
│  │  - OCR orchestration       │      │  - Capture exclusion   │   │
│  └───────────────────────────┘      │  - Animations          │   │
│               │                      │  - DPI handling        │   │
│               │  WebSocket/HTTPS     │  - Global hotkey pump  │   │
│               ▼                      └───────────────────────┘   │
│   ┌────────────────────┐                                         │
│   │ AI Providers        │                                         │
│   │ OpenAI / Anthropic / │                                        │
│   │ Gemini / Ollama(local)│                                       │
│   └────────────────────┘                                         │
└─────────────────────────────────────────────────────────────────┘
```

**Key decision:** the overlay is a *separate native window*, not rendered by
Flutter's own window. Flutter's Windows embedder owns one HWND per
`FlutterWindow`; rather than fight that model for the transparent/topmost/
click-through/capture-excluded overlay behavior, we create and own a second,
independent Win32 window from C++ and drive it from Dart via a bridge. The
Flutter window can run hidden (tray-only) or as the "control panel" window
for settings/history, while the overlay is purely the floating assistant
surface.

---

## 2. Process & Threading Model

- **Single process**, two logical UI surfaces:
  - Flutter engine thread(s): UI (Dart), raster, platform threads (standard
    Flutter Windows embedder threading).
  - Native overlay: its own Win32 message loop, running on a **dedicated
    thread** created at startup (`OverlayThread`), separate from Flutter's
    platform thread, so overlay repaint/input handling is never blocked by
    Dart isolate work and vice versa.
  - Global hotkey listener runs on the overlay thread (low-level keyboard
    hook or `RegisterHotKey`, see §6).
- Cross-thread communication between the Dart platform thread and the
  overlay thread happens through a thread-safe command queue
  (`PostMessage`/custom `WM_APP+N` messages carrying opaque payloads) — no
  shared mutable state is touched from both threads without synchronization.

---

## 3. Layered Architecture (Dart side)

```
lib/
  app/            # App bootstrap, routing, DI/composition root
  models/         # Freezed/plain data classes: Conversation, Message,
                  # PromptTemplate, ProviderConfig, HotkeyBinding, OcrResult
  providers/      # Riverpod providers wiring services -> UI state
  services/
    ai/           # AiProvider interface + OpenAI/Anthropic/Gemini/Ollama impls
    storage/      # SQLite (drift or sqlite3), secure key storage wrapper
    clipboard/    # Opt-in clipboard watcher (polling via native bridge)
    ocr/          # OCR request orchestration, talks to native capture + OCR
    overlay/      # Dart-side facade over the native overlay bridge
    hotkeys/      # Hotkey config model -> native registration calls
    logging/      # Structured logger, log file rotation
  ui/
    settings/     # Settings screens
    conversation/ # Chat UI, markdown/code rendering, streaming
    overlay/      # Overlay *content* widgets, rendered either:
                  #   (a) off-screen and rasterized to the native surface, or
                  #   (b) directly if WebView2 HTML overlay mode is chosen
  overlay/        # Overlay window lifecycle Dart API (open/close/move/etc.)
  settings/       # Settings persistence glue
```

**Dependency rule:** `ui/` depends on `providers/`, `providers/` depend on
`services/`, `services/` depend on `models/`. Nothing in `services/` or
`models/` imports from `ui/`. Native calls are only ever made through
`services/overlay`, `services/hotkeys`, `services/ocr`, `services/clipboard`
— UI code never calls MethodChannel/FFI directly.

---

## 4. Layered Architecture (Native side)

```
windows/
  native/
    overlay_window.h/.cpp     # OverlayWindow: HWND creation, WndProc,
                               # style flags, DWM/composition setup
    overlay_manager.h/.cpp    # Owns OverlayThread + message loop,
                               # exposes thread-safe command API
    animation.h/.cpp          # Timer/DirectComposition-driven animator
    dpi.h/.cpp                # Per-monitor DPI helpers, WM_DPICHANGED
    renderer_d2d.h/.cpp       # Direct2D/DirectWrite content renderer
    renderer_webview2.h/.cpp  # Optional WebView2-hosted content renderer
  capture/
    display_affinity.h/.cpp   # Wraps SetWindowDisplayAffinity + capability
                               # detection (OS version gate)
    region_selector.h/.cpp    # Full-screen selection overlay -> bitmap
    screen_capture.h/.cpp     # Windows.Graphics.Capture wrapper for the
                               # user-consented region grab
  hotkeys/
    hotkey_manager.h/.cpp     # RegisterHotKey wrapper + conflict detection
  ffi/
    overlay_api.h/.cpp        # extern "C" flat API surface for Dart FFI
    method_channel_handlers.cpp # Alternative/parallel MethodChannel handlers
  runner/                      # Standard Flutter Windows runner, minimally
                               # modified to boot OverlayManager at startup
```

**Reusable classes, single responsibility:**
- `OverlayWindow` — only knows how to be *a* window (create/destroy/move/
  resize/style). No business logic.
- `OverlayManager` — owns the thread + message pump + command queue; the
  only class Dart's bridge talks to.
- `DisplayAffinityController` — isolated capability class; queries OS
  version, applies/reverts `WDA_EXCLUDEFROMCAPTURE`, reports actual applied
  state back (never assumes success silently).
- `HotkeyManager` — isolated; translates config → `RegisterHotKey`
  registrations; emits events through the same command queue.

---

## 5. Dart ↔ Native Bridge

Two viable transports, both supported by the abstraction in
`services/overlay`:

1. **MethodChannel** (`overlay_channel`) — simplest, sufficient for
   discrete calls (`show`, `hide`, `setBounds`, `setClickThrough`,
   `setCaptureExcluded`, `setOpacity`) and event callbacks
   (`onHotkeyPressed`, `onRegionSelected`, `onOverlayMoved`).
2. **Dart FFI** (`dart:ffi` against `overlay_api.dll`/statically linked into
   the exe) — used for the **high-frequency path**: streaming content
   updates into the Direct2D renderer without MethodChannel's JSON
   serialization overhead, when native (Direct2D) rendering mode is active.

Contract lives in `services/overlay/overlay_bridge.dart` behind an
interface (`OverlayBridge`), so the rest of the app is agnostic to which
transport is active. This also makes the bridge mockable for unit tests.

```dart
abstract class OverlayBridge {
  Future<void> show({Rect? bounds});
  Future<void> hide();
  Future<void> setBounds(Rect bounds);
  Future<void> setClickThrough(bool enabled);
  Future<void> setCaptureExcluded(bool enabled); // returns applied state via event
  Future<void> setOpacity(double opacity);
  Stream<OverlayEvent> get events; // hotkeys, region selection, move, dpi change
}
```

---

## 6. Global Hotkeys

- Default mechanism: `RegisterHotKey` (documented, per-thread, no admin
  rights, no global keyboard hook) registered on the overlay thread's
  message loop.
- Low-level keyboard hook (`WH_KEYBOARD_LL`) is **not** used by default
  because it requires more caution around performance/AV false positives;
  it is offered only as an opt-in "advanced hotkeys" mode for combos
  `RegisterHotKey` cannot express, clearly labeled in settings.
- Conflict detection: attempt registration, surface failure to Dart with
  the conflicting binding so the user can rebind.

---

## 7. Overlay Rendering Modes

| Mode | Tech | When to use |
|---|---|---|
| Native | Direct2D + DirectWrite | Default. Lowest latency, smallest memory, matches perf goals. |
| Web | WebView2 (Chromium) | Optional, for users who want rich HTML/CSS overlay content or custom themes/plugins. Higher memory cost (~+70-100MB), acceptable as an explicit opt-in, documented as exceeding the 150MB budget when active. |

Both modes sit behind the same `OverlayContentRenderer` interface so the
Dart layer doesn't need to know which is active beyond a settings toggle.

---

## 8. Data Flow: Streaming AI Response

```
User input (Flutter UI)
   → services/ai/AiProvider.streamChat()
   → WebSocket/HTTP SSE to provider
   → Stream<ChatToken> in Dart
   → Riverpod StateNotifier appends token, throttled to ~30-60 updates/sec
   → ConversationView rebuilds incrementally (markdown incremental parse)
   → if overlay is the active surface: tokens also pushed through
     OverlayBridge (FFI fast path) → Direct2D renderer repaints affected
     text region only (dirty-rect based), not full redraw
```

---

## 9. Data Storage

- **SQLite** (via `sqlite3`/`drift`): conversations, messages, prompt
  templates, provider configs (non-secret fields only), hotkey bindings,
  UI preferences.
- **Windows Credential Locker** (via `win32` credential APIs, DPAPI-backed):
  API keys and any OAuth tokens.
- **File system** (`%LOCALAPPDATA%\AiOverlayAssistant\`): logs (rotated,
  capped size), exported conversations, cached OCR temp images (deleted
  after use unless user opts to keep).

---

## 10. Error Handling & Resilience
- Native overlay failures (e.g., DWM composition unavailable) degrade to a
  flat dark background rather than crashing.
- AI provider errors surface as inline chat error bubbles with retry, never
  silently swallowed.
- `OverlayManager` watchdog: if the overlay thread stops pumping messages
  for > N seconds, Flutter host logs it and offers a "restart overlay"
  action (recreate thread + window) without restarting the whole app.
- All native boundary calls (FFI/MethodChannel) return explicit
  success/failure + error code; Dart never assumes success.

---

## 11. Testing Strategy (architecture-level)
- Dart: unit tests for services (`ai`, `storage`, `hotkeys` config, `ocr`
  orchestration) with `OverlayBridge`/`AiProvider` mocked.
- Native: GoogleTest for `DisplayAffinityController`, `HotkeyManager`
  parsing/registration logic, `dpi.cpp` scaling math — isolated from actual
  window creation where possible.
- Integration: scripted manual test matrix (see `phases.md`) for real
  multi-monitor/DPI/composition combinations that are impractical to fully
  automate.

---

## 12. Compliance Notes
- Only documented APIs used throughout (`SetWindowDisplayAffinity`,
  `SetWindowPos`, `DwmSetWindowAttribute`, `RegisterHotKey`,
  `Windows.Graphics.Capture`, `Windows.Media.Ocr` or bundled Tesseract).
- No process injection, no hooking of other applications, no modification
  of other processes' memory or windows.
