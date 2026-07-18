# Software Requirements Document (SRD)
## Windows AI Overlay Assistant

**Version:** 0.1.0-draft
**Status:** Planning
**Last updated:** 2026-07-15

---

## 1. Purpose

This document defines the functional and non-functional requirements for a
Windows desktop AI assistant composed of:

1. A **Flutter application** (`app.exe`) that owns UI, state, auth, settings,
   conversation logic, and AI provider integration.
2. A **native Win32 overlay** (a separate top-level window, not a Flutter
   window) that provides always-on-top behavior, transparency/blur,
   click-through, and (where the OS/hardware support it) a request to be
   excluded from screen capture via the documented
   `SetWindowDisplayAffinity` API.

The overlay is explicitly scoped to use **only public, documented Win32
APIs**. No undocumented behavior, no kernel hooks, no attempts to defeat or
bypass OS security boundaries, anti-cheat systems, or capture-application
protections. `WDA_EXCLUDEFROMCAPTURE` is a legitimate, Microsoft-documented
affinity flag (used by e.g. password managers and credential prompts); this
project uses it as designed and documents its real limitations (see §9).

---

## 2. Scope

### 2.1 In scope
- Flutter Windows desktop app: auth, settings, provider config, hotkey
  config, conversation history, prompt templates, OCR *request* orchestration,
  clipboard integration (opt-in).
- Native Win32 overlay: window lifecycle, positioning, transparency,
  always-on-top, optional click-through, capture-exclusion request,
  animation, multi-monitor + per-monitor DPI awareness.
- FFI/MethodChannel bridge between Dart and native C++.
- AI provider adapters: OpenAI (GPT), Anthropic (Claude), Google (Gemini),
  local Ollama.
- Streaming responses, Markdown + syntax-highlighted code rendering.
- Global hotkeys, screen-region selection, OCR pipeline (via an OCR engine
  the user installs/consents to, e.g. Windows.Media.Ocr or Tesseract).
- SQLite-backed local config/history storage.

### 2.2 Out of scope (explicitly)
- Any technique intended to hide the app from legitimate security/monitoring
  software, exam-proctoring software, or DRM-protected capture pipelines.
- Reverse-engineering or bypassing third-party capture-exclusion detection.
- Cloud sync / multi-device sync (v1).
- macOS/Linux ports (v1).
- Telemetry beyond opt-in crash/usage reporting.

---

## 3. Definitions

| Term | Meaning |
|---|---|
| Overlay | The native Win32 top-level window rendering the floating assistant UI |
| Host app | The Flutter application process |
| Bridge | The FFI/MethodChannel layer connecting Dart ↔ C++ |
| Capture exclusion | `SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)` |
| Click-through | `WS_EX_TRANSPARENT` extended style, toggled at runtime |

---

## 4. Functional Requirements

### 4.1 Authentication & Settings (Flutter)
- FR-1: User can add/edit/remove API keys per provider (OpenAI, Anthropic,
  Google, Ollama endpoint URL). Keys stored via Windows Credential Locker
  (DPAPI-backed), never in plaintext SQLite.
- FR-2: User can select default provider/model per conversation.
- FR-3: Settings persist across restarts via local SQLite + secure storage.
- FR-4: User can configure global hotkeys (invoke overlay, capture region,
  toggle click-through, quick-ask).

### 4.2 Conversation & AI (Flutter)
- FR-5: User can start, rename, delete, and search conversations.
- FR-6: Streaming token-by-token responses rendered as Markdown with
  syntax-highlighted fenced code blocks and a per-block copy button.
- FR-7: Prompt templates: CRUD, variable substitution, quick-insert.
- FR-8: Provider adapters share a common interface
  (`AiProvider.streamChat(...)`) so new providers can be added without
  touching UI code.
- FR-9: Conversation history stored locally in SQLite; exportable as
  Markdown/JSON.

### 4.3 Overlay (Native + Flutter-driven)
- FR-10: Overlay is a borderless, layered, top-level window, independent of
  the Flutter window (Flutter may run hidden/tray-only).
- FR-11: Overlay supports drag-to-move and edge/corner resize.
- FR-12: Overlay supports always-on-top toggle
  (`HWND_TOPMOST` via `SetWindowPos`).
- FR-13: Overlay supports optional click-through toggle at runtime.
- FR-14: Overlay requests capture exclusion via `SetWindowDisplayAffinity`
  when the user enables "Hide overlay from screen shares/recordings" in
  settings; app surfaces the documented caveats (§9) in the UI copy itself.
- FR-15: Overlay is excluded from Alt+Tab via `WS_EX_TOOLWINDOW` (and not
  setting `WS_EX_APPWINDOW`).
- FR-16: Overlay auto-hides/fades after a configurable inactivity timeout
  and restores on hotkey or hover.
- FR-17: Overlay renders acrylic/blur background via
  `DwmSetWindowAttribute`/`SetWindowCompositionAttribute`-family documented
  APIs where available on the running Windows build, with a solid dark
  fallback when composition isn't available.
- FR-18: Overlay content rendered via Direct2D/DirectWrite, or via an
  embedded WebView2 surface when HTML/CSS UI is selected in settings.

### 4.4 Productivity Features
- FR-19: Global hotkey triggers screen-region selection (native rubber-band
  selector), captures the region, and sends it to the OCR pipeline.
- FR-20: OCR requires explicit first-run consent; can be disabled entirely.
- FR-21: Clipboard monitoring is opt-in, off by default, with a visible
  status indicator whenever active, and a one-click kill switch.
- FR-22: Full keyboard navigation of overlay UI (tab order, escape to
  dismiss, arrow-key list navigation).
- FR-23: Multi-monitor aware: overlay remembers per-monitor position and
  DPI scale; region selection works across monitor boundaries.

### 4.5 Native Windows Integration
- FR-24: App declares per-monitor-v2 DPI awareness in the manifest.
- FR-25: Overlay responds to `WM_DPICHANGED` by rescaling and repositioning.
- FR-26: Window snapping (Win+arrow) works normally on the overlay unless
  click-through is active.
- FR-27: All animations (show/hide/resize/fade) run off the UI-blocking
  path (timer or DirectComposition), targeting 60 FPS.

---

## 5. Non-Functional Requirements

| ID | Requirement | Target |
|---|---|---|
| NFR-1 | Cold start to visible UI | < 2 s |
| NFR-2 | Overlay input latency (hotkey → visible) | < 16 ms perceived |
| NFR-3 | Idle RAM (Flutter + native combined) | < 150 MB |
| NFR-4 | Idle CPU | < 1% average over 60 s idle |
| NFR-5 | Streaming render | no dropped frames > 100 ms during token stream |
| NFR-6 | Crash isolation | native overlay crash must not corrupt user data; Flutter host restarts overlay process/window gracefully |
| NFR-7 | Accessibility | overlay exposes UI Automation tree where feasible |
| NFR-8 | Localization-ready | all user-facing strings externalized |

---

## 6. Security & Privacy Requirements
- SEC-1: API keys never logged, never written to plaintext files.
- SEC-2: Clipboard/OCR data never leaves device except when explicitly sent
  to an AI provider as part of a user-initiated request.
- SEC-3: All network calls over TLS; certificate validation not disabled.
- SEC-4: No code obfuscation intended to hide app behavior from the OS or
  from legitimate monitoring tools; no process-hiding, no DKOM, no hooking
  of other processes.
- SEC-5: Local SQLite database encrypted at rest (SQLCipher or OS DPAPI
  wrapping) for conversation history containing sensitive content.
- SEC-6: Clear, persistent visual indicator any time clipboard monitoring or
  screen OCR capture is active.

---

## 7. Constraints
- Windows 10 20H2+ / Windows 11 (per-monitor DPI v2 requires 1703+).
- Acrylic/blur availability depends on DWM composition + user's transparency
  settings; must degrade gracefully.
- `WDA_EXCLUDEFROMCAPTURE` behavior is capture-API dependent; some capture
  paths (e.g. certain GPU-level or driver-level capture, some OS screenshot
  fallbacks) may still show the window. This must never be marketed as
  guaranteed invisibility.

---

## 8. Acceptance Criteria (v1 "done")
- All FR items above implemented and covered by at least one automated or
  manual test case in `phases.md` milestone gates.
- NFR targets met on a reference machine (documented in `phases.md`).
- No plaintext secrets on disk (verified via test).
- Overlay functions correctly across a 2-monitor, mixed-DPI test rig.

---

## 9. Known Limitations (must be disclosed in-app)
- Capture exclusion is best-effort and OS/driver/capture-tool dependent; it
  is not a security boundary and must not be used to defeat legitimate
  proctoring, compliance recording, or consent-based screen sharing
  requirements set by the user's organization.
- Local Ollama models depend on the user's local hardware/model choice for
  quality and speed; this app does not bundle model weights.
