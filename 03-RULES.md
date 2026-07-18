# Engineering Rules & Standards
## Windows AI Overlay Assistant

These rules are binding for all contributors (human or AI-assisted) working
on this codebase. PRs that violate §1–§3 should be rejected in review.

---

## 1. Non-Negotiable Boundaries

1. **Documented APIs only.** Every native Win32/COM call must map to a
   function documented on Microsoft Learn. If a capability requires an
   undocumented API, an OS hook, DKOM, process injection into another
   process, or reading another process's memory — it is out of scope,
   full stop. Add a comment linking the MS Learn page next to any
   non-obvious API call.
2. **No capture-bypass claims.** Capture exclusion code and its UI copy
   must always describe the behavior as "best-effort, may not work with
   all capture methods" — never "invisible" or "undetectable." Do not add
   features whose primary purpose is defeating a specific third-party
   capture/monitoring tool.
3. **No secrets in source, logs, or plaintext files.** API keys go through
   the secure storage service only. Grep for accidental `sk-`/`key=`
   literals is part of CI.
4. **Consent gates stay gates.** Clipboard monitoring and OCR capture may
   never be silently auto-enabled by a default, a migration, or a "helpful"
   settings reset. Toggling them on always requires an explicit, dated
   user action recorded in settings.

---

## 2. Dart / Flutter Rules

- **State management:** Riverpod (preferred) for new code. If a legacy
  Provider-based module exists, don't mix patterns within the same feature
  — migrate the whole feature or leave it, don't straddle.
- **Immutability:** models are immutable (`freezed` or hand-written
  `copyWith`); no mutable public fields on domain models.
- **No business logic in widgets.** Widgets read from providers/notifiers
  and dispatch intents; they do not call services, format dates for
  storage, or make network calls directly.
- **Null-safety strict:** no `!` without a preceding guard or a comment
  justifying why it's guaranteed non-null.
- **Async discipline:** every `Future` is awaited or explicitly fire-and-
  forget with a comment (`// intentionally unawaited: telemetry, best
  effort`). No dangling futures that swallow errors.
- **Error handling:** services throw typed exceptions
  (`AiProviderException`, `OverlayBridgeException`, `StorageException`);
  UI layer catches at the boundary and renders user-facing messages. No
  bare `catch (e) {}`.
- **Logging:** use the shared `AppLogger`; never `print()` in committed
  code. Log levels: `debug` (dev only), `info`, `warn`, `error`. Never log
  secrets, full clipboard contents, or full OCR text at `info`+ (hash or
  truncate if needed for debugging).
- **Formatting:** `dart format` clean, `flutter analyze` zero warnings
  before merge.
- **Testing:** every service class has a corresponding test file; every
  bug fix adds a regression test.

## 3. C++ / Native Rules

- **Standard:** C++17 minimum, RAII everywhere. No raw `new`/`delete` for
  ownership — use smart pointers or Win32 wrapper types (`wil::unique_hwnd`
  or hand-rolled equivalents) for HWNDs, GDI/D2D resources, handles.
- **One class, one responsibility** (see architecture doc §4). Do not grow
  `OverlayWindow` to also own hotkeys or capture logic — compose instead.
- **Thread safety:** anything crossing the Dart-thread ↔ overlay-thread
  boundary goes through the command queue. No shared mutable globals. Mark
  thread-affinity in a comment at the top of any class that must only be
  touched from one thread.
- **Error checking:** every Win32 call that can fail is checked; failures
  are logged with `GetLastError()` decoded, and propagated as a result
  code across the FFI boundary — never silently ignored.
- **No blocking calls on the message-pump thread** beyond what Win32 itself
  requires; expensive work (OCR, capture encode) goes to a worker thread
  and reports back via the command queue.
- **Resource cleanup:** every `Create*` has a matching `Destroy*`/RAII
  destructor; run under Application Verifier / ASan in CI for the native
  test target periodically.
- **Comments:** any use of `WS_EX_*`, `DWMWA_*`, or composition attributes
  gets a one-line comment stating *why* that flag, since these are easy to
  cargo-cult incorrectly.

## 4. Bridge (FFI/MethodChannel) Rules

- All bridge calls are versioned by a simple integer schema so native and
  Dart can detect mismatch during development (`kBridgeSchemaVersion`).
- Every bridge method has: a Dart-side interface method, a native handler,
  and a unit/integration test stub — added together in the same PR.
- Payloads crossing FFI use fixed, documented struct layouts (no raw
  pointer arithmetic without a struct definition committed alongside).
- MethodChannel calls always specify explicit types; no passing raw `Map`
  without a documented shape.

## 5. UI / Design Rules

- Follow `05-DESIGN.md` tokens (spacing, radius, color, type scale) — no
  ad-hoc magic numbers in widget code.
- Every interactive element must be reachable via keyboard alone (tab
  order, Enter/Space activation, Escape to dismiss overlays).
- Animations target 60 FPS; anything that can't hit that on the reference
  test machine gets a "reduced motion" fallback path, and reduced-motion
  must also respect the OS-level "Show animations" accessibility setting.
- Dark theme is the only theme for v1 (per spec); code should not hardcode
  colors that would break if a light theme is added later — use theme
  tokens even though only one theme ships.

## 6. Git / Process Rules

- Conventional commits (`feat:`, `fix:`, `refactor:`, `docs:`, `test:`,
  `chore:`).
- Every PR touching native overlay code includes before/after notes on
  measured memory/CPU on the reference machine when the change could
  plausibly affect NFR-1..NFR-5.
- No merging with `flutter analyze` or native compiler warnings above the
  baseline (`/W4` clean on native code, treat new warnings as errors).
- Feature flags for anything experimental (e.g., WebView2 overlay mode
  starts behind a flag until it meets the perf budget documented for it).

## 7. Definition of Done (per feature)
1. Implements the FR/NFR item(s) it targets (reference the ID from
   `01-SRD.md`).
2. Has unit tests (Dart) and/or native tests where logic is non-trivial.
3. Manual test matrix entry updated in `04-PHASES.md` if it touches
   overlay/multi-monitor/DPI behavior.
4. No new secrets, no new undocumented API usage, no new default-on
   consent-gated behavior.
5. Docs updated (`02-ARCHITECTURE.md` if structure changed,
   `05-DESIGN.md` if visuals changed).
