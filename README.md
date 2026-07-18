# AI Overlay Assistant

Windows desktop AI assistant — Flutter host app + a separate native Win32
overlay window. See `/mnt/project docs` (`01-SRD.md` … `05-DESIGN.md`) for
the full spec this scaffold implements against.

## Status

**Phase 0 (scaffolding) — structurally complete, unverified.**
**Phase 1 (native overlay foundation) — core implementation in place, unverified.**
**Phase 2 (bridge & content rendering) — core implementation in place, unverified.**

This code was generated in a Linux sandbox with **no Flutter SDK, no
Windows SDK/MSVC, and no way to run `flutter create`, `flutter analyze`,
or compile the native C++**. Everything here is hand-written to the shape
those tools would produce and to the constraints in `03-RULES.md`, but
none of it has actually been built or run. Before trusting any gate in
`04-PHASES.md` as "passed," do this on a real Windows machine:

```powershell
flutter pub get
flutter analyze
flutter test
flutter run -d windows          # Phase 0 gate: blank window appears

cd windows
cmake -S . -B build -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

### What's real vs. stubbed right now

| Component | State |
|---|---|
| `lib/` folder structure, models, service interfaces | Real, matches `02-ARCHITECTURE.md` §3 |
| `AppLogger` / `NativeLog` | Real, both sides |
| `LocalDatabase`, `SecureKeyStorage` | Interface + in-memory fake only — real drift/Credential-Locker impl is Phase 3 |
| `OverlayWindow` (native) | Real: window creation, DWM rounded corners + backdrop, click-through, WM_NCHITTEST drag/resize, DPI change handling |
| `OverlayManager` (native) | Real: dedicated thread, message loop, thread-safe command queue, blocking capture-exclusion round trip |
| `DisplayAffinityController` | Real: OS-version gate + `SetWindowDisplayAffinity` + re-query of actual state |
| `HotkeyManager` | Structural only — `RegisterHotKey` wiring is Phase 3 per `04-PHASES.md` |
| `Animator` | Real: timer-driven show/hide fade, respects OS reduced-motion setting |
| MethodChannel bridge (`method_channel_handlers.cpp` / `windows_overlay_bridge.dart`) | Real, channel names/payload shapes match on both sides |
| FFI fast path | Real: `AiOverlay_AppendContentToken`/`AiOverlay_ResetContent` reach the live `OverlayManager` via `ActiveInstance()`, exercised from Dart in `overlay_content_channel.dart` |
| `MarkdownParser` / `SyntaxHighlighter` / `ContentModel` | Real: heading/bold/list/fenced-code parsing, heuristic per-line syntax tokens, dirty-block diffing — all native-unit-tested |
| `RendererD2D` | Real: Direct2D/DirectWrite paint of all block kinds, copy-button hit-testing, dirty-rect `InvalidateRect` (see "Dirty-rect, precisely" below for what that does and doesn't cover) |
| `RendererWebView2` | Real but **cannot be exercised in this sandbox at all** — requires the WebView2 SDK, off by default (`AI_OVERLAY_ENABLE_WEBVIEW2`), copy-click wired straight to the clipboard since WebView2 input never reaches `OverlayWindow`'s WndProc |
| Direct2D exact text metrics (per-glyph advance) | Approximated (fixed px-per-character), flagged in `renderer_d2d.cpp` — real `IDWriteTextLayout` measurement is a follow-up once visual QA on a real machine shows where it's off |

### Dirty-rect, precisely

The Phase 2 gate asks for "dirty-rect based repaint (no full-window
redraw per token)." What's actually implemented: `ContentModel` diffs
each re-parse and reports which *blocks* changed; `RendererD2D` uses
that to (a) only recompute layout from the first changed block onward,
not the whole document, and (b) call `InvalidateRect` with just the
affected screen region rather than the whole client area. What it does
**not** do is partial-target Direct2D presentation — each `WM_PAINT`
still runs one `BeginDraw`/`EndDraw` over all cached blocks. For a panel
this size that's standard practice and should comfortably hit the 60 FPS
target, but if profiling on real hardware says otherwise, the next step
is `ID2D1DeviceContext` + `PushAxisAlignedClip` per invalid rect, not a
rewrite of the diffing logic above it.

### Second design-doc deviation: WebView2's color duplication

`05-DESIGN.md` §2.1 says colors live in exactly two files (`tokens.dart`
+ `renderer_d2d.cpp`). `windows/web/overlay_content.html`'s `<style>`
block necessarily hardcodes the same palette a third time — CSS can't
import a Dart or C++ constant. Flagged in a comment at the top of that
file rather than silently resolved; worth a decision on whether to
generate that block from `tokens.dart` at build time, or just amend the
doc to name it as an explicit third, generated-from mirror.

`method_channel_handlers.cpp` fires `EventChannel` events from
`OverlayThread`, but Flutter's Windows embedder expects `EventSink`
calls on the platform thread. The correct marshaling call depends on the
exact Flutter engine version this is built against — flagged with a
`NOTE:` comment at the call site. Resolve this against the pinned
Flutter SDK version before Phase 1's gate can be considered passed.

### Design-doc deviation worth reviewing

`05-DESIGN.md`/`04-PHASES.md` describe the acrylic/blur background as
using "the documented composition attribute API." The classic call for
that (`SetWindowCompositionAttribute`) is **not** published on Microsoft
Learn, which conflicts with `03-RULES.md` §1 rule 1. This implementation
uses `DwmSetWindowAttribute(DWMWA_SYSTEMBACKDROP_TYPE)` instead — genuinely
documented, Windows 11 22H2+, solid-dark fallback below that. Worth a
one-line update to `05-DESIGN.md` to reflect this if it's agreed.

## Layout

Matches `02-ARCHITECTURE.md` §3/§4 exactly — see that doc for the
folder-by-folder rationale.
