# Development Phases & Roadmap
## Windows AI Overlay Assistant

Each phase ends with a gate: specific, checkable exit criteria. Do not start
phase N+1 core work before phase N's gate passes (spikes/prototyping ahead
of schedule are fine, but they don't count as "done").

---

## Phase 0 — Project Scaffolding (est. 3-5 days)
**Goal:** empty-but-real skeleton that builds, runs, and is wired for CI.

- Flutter Windows project created; folder structure per `02-ARCHITECTURE.md`.
- Native `windows/` subfolders created with placeholder classes
  (`OverlayWindow`, `OverlayManager`, `HotkeyManager`,
  `DisplayAffinityController`) compiling and unit-testable but not
  functional yet.
- CI pipeline: `flutter analyze`, `dart test`, native build via CMake/MSBuild
  wired into Flutter's plugin build, `/W4` warnings-as-errors baseline.
- SQLite + secure storage services stubbed with interfaces + mocks.
- Logging framework in place.

**Gate:** `flutter run -d windows` shows a blank window; CI green.

---

## Phase 1 — Native Overlay Foundation (est. 1.5-2 weeks)
**Goal:** the overlay exists as a real, independent, correctly-behaved
Win32 window, driven manually (no AI features yet).

- `OverlayWindow`: borderless, layered (`WS_EX_LAYERED`), topmost
  (`HWND_TOPMOST`), tool window (`WS_EX_TOOLWINDOW`, no `WS_EX_APPWINDOW`)
  so it's hidden from Alt+Tab.
- Rounded corners via `DwmSetWindowAttribute(DWMWA_WINDOW_CORNER_PREFERENCE)`
  (Win11) with graceful no-op on older builds.
- Acrylic/blur background via documented composition attribute API, with
  solid-dark fallback when composition is off.
- Drag-to-move, edge/corner resize (`WM_NCHITTEST` custom hit-testing).
- Click-through toggle (`WS_EX_TRANSPARENT` add/remove at runtime).
- Capture exclusion: `DisplayAffinityController` implemented, OS-version
  gated, reports real applied state back.
- Per-monitor DPI v2 manifest + `WM_DPICHANGED` handling; verified on a
  2-monitor mixed-DPI rig.
- `OverlayManager` thread + command queue; MethodChannel bridge exposing
  `show/hide/move/resize/setClickThrough/setCaptureExcluded`.
- Basic show/hide/fade animation (timer-driven, no content yet — just a
  colored panel).

**Gate (manual test matrix — must pass on Win10 and Win11, single + dual
monitor, 100%/150%/200% DPI):**
| Check | Pass criteria |
|---|---|
| Alt+Tab | Overlay never appears in the switcher |
| Always on top | Stays above all normal windows incl. fullscreen-windowed apps |
| Drag/resize | Smooth, no tearing, respects monitor work-area |
| Click-through | Toggling it makes clicks pass to the window underneath |
| Capture exclusion | Verified with at least 2 different capture tools (e.g. Windows Snipping Tool, OBS); documented which ones honor it |
| DPI change | Moving overlay between differently-scaled monitors keeps it crisp, correctly sized |
| Idle CPU/RAM | Native overlay alone: < 20MB RAM, ~0% idle CPU |

---

## Phase 2 — Bridge & Content Rendering (est. 1-1.5 weeks)
**Goal:** overlay can render real dynamic content driven from Dart.

- FFI fast-path implemented for high-frequency content updates.
- Direct2D/DirectWrite renderer: text, basic markdown (headings, bold,
  lists), monospace code blocks with a syntax highlighter.
- WebView2 renderer mode implemented behind a feature flag (alternative
  path), sharing the same `OverlayContentRenderer` interface.
- Dirty-rect based repaint (no full-window redraw per token).
- Copy-button hit-testing/interaction working in both render modes.

**Gate:** streaming 50 tokens/sec of mixed markdown+code into the overlay
sustains 60 FPS on the reference machine (documented spec) with no visible
tearing; native mode RAM stays under budget, WebView2 mode's extra memory
is measured and documented.

---

## Phase 3 — Flutter App Shell (est. 1-1.5 weeks)
**Goal:** the "real app" — settings, auth, storage, control panel window.

- Riverpod app skeleton, routing, DI/composition root.
- Settings UI: provider API keys (secure storage), model selection, hotkey
  configuration UI, overlay appearance toggles (click-through default,
  capture-exclusion default, auto-hide timeout).
- SQLite schema + migrations for conversations/messages/templates/configs.
- Conversation list + basic single-conversation view (non-streaming stub
  data first).
- Global hotkey registration wired end-to-end: config in Flutter → native
  `HotkeyManager` → event back to Dart → overlay show/hide.

**Gate:** user can install, add an API key, set a hotkey, press it, and see
the (still-stub) overlay appear/disappear reliably across 20 consecutive
toggles with no leak (RAM flat) and no missed hotkey press.

---

## Phase 4 — AI Provider Integration (est. 1.5-2 weeks)
**Goal:** real streaming answers from all four provider types.

- `AiProvider` interface + implementations: OpenAI, Anthropic, Gemini,
  Ollama (local HTTP).
- Streaming plumbed end-to-end: provider → Dart stream → conversation state
  → overlay renderer (both render modes).
- Error handling: auth failure, rate limit, network drop, model-not-found —
  each with a distinct user-facing message and retry affordance.
- Prompt templates: CRUD + variable substitution + quick-insert into the
  overlay's input box.
- Conversation history persistence including streamed messages (resumable
  if app restarts mid-stream is out of scope for v1 — clearly abort and
  save partial message instead).

**Gate:** a full round trip (hotkey → overlay → type prompt → stream
response → copy code block → close) works against all four providers on
the reference machine; error paths manually verified by simulating each
failure mode (bad key, airplane mode, invalid model).

---

## Phase 5 — Productivity Features (est. 1.5-2 weeks)
**Goal:** OCR, clipboard, region capture, keyboard-only flows.

- Region selector: full-screen native overlay for click-drag rectangle
  selection, multi-monitor aware, Esc to cancel.
- Screen capture of the selected region via `Windows.Graphics.Capture`.
- OCR pipeline (Windows.Media.Ocr or bundled engine) with first-run
  consent dialog; result flows into a new/active conversation as context.
- Clipboard monitor: opt-in, visible status indicator, kill switch, feeds
  clipboard text into the "quick ask" flow on hotkey.
- Full keyboard navigation audit of every screen (settings, conversation,
  overlay input, template picker) — tab order, focus rings, Escape
  semantics documented and consistent.

**Gate:** OCR and clipboard both function correctly with consent flow
verifiable via a fresh profile; toggling either off immediately stops all
associated background activity (verified via logs — zero clipboard reads
after disable).

---

## Phase 6 — Polish, Performance, Accessibility (est. 1-1.5 weeks)
**Goal:** meet all NFRs, ship-quality polish.

- Animation pass: all transitions smoothed, reduced-motion path verified
  against the OS accessibility setting.
- Startup profiling to hit < 2s; idle RAM/CPU profiling to hit budget in
  native mode (document WebView2-mode numbers separately as an accepted
  trade-off for that opt-in mode).
- UI Automation tree review for overlay content (screen reader smoke test).
- Crash/watchdog handling for the overlay thread (Phase-1 hook now
  exercised with induced-failure tests).
- Full manual regression pass across the Phase-1 test matrix plus all
  features added since.

**Gate:** all NFR-1..NFR-8 targets met and recorded with actual measured
numbers in a `PERF_RESULTS.md`; no P0/P1 bugs open.

---

## Phase 7 — Packaging & Release (est. 3-5 days)
- MSIX or signed installer packaging.
- Auto-update mechanism (or documented manual-update process for v1).
- Privacy/consent copy finalized, including the capture-exclusion caveat
  language from `01-SRD.md` §9 surfaced in-app (not just in docs).
- README, user-facing docs, changelog.

**Gate:** clean install → first-run consent flows → working hotkey →
working AI answer, all on a machine with no dev tools installed.

---

## Cross-Phase: Testing Cadence
- Unit tests written alongside each feature (per `03-RULES.md` DoD), not
  batched at the end.
- Manual multi-monitor/DPI matrix re-run at the end of Phases 1, 2, 5, 6.
- Security/privacy self-review checklist run at the end of Phases 3, 5, 7
  (secrets handling, consent gates, log content audit).
