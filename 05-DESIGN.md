# Design Document
## Windows AI Overlay Assistant — Visual & Interaction Design

---

## 1. Design Principles

1. **Get out of the way.** The overlay is a tool that sits on top of other
   work; it should feel light, fast, and dismissible, never like a
   full application window.
2. **Native-feeling, not web-feeling.** Even where WebView2 is used, motion
   and depth should follow Fluent Design conventions so it doesn't read as
   a browser popup.
3. **Legible first.** Streaming text and code are the core content — type
   and contrast choices are optimized for long-session reading, not just
   first-impression polish.
4. **Keyboard-first.** Every interaction has a keyboard path; mouse is a
   convenience, not a requirement.

---

## 2. Design Tokens

### 2.1 Color (Dark theme — only theme for v1)

| Token | Hex | Usage |
|---|---|---|
| `bg.base` | `#0F1115` | Overlay panel base (fallback when no blur) |
| `bg.elevated` | `#171A21` | Cards, input box, code block background |
| `bg.acrylic.tint` | `#12141ACC` (80% alpha) | Acrylic tint over blur |
| `border.subtle` | `#2A2E38` | Panel borders, dividers |
| `border.focus` | `#7C9CFF` | Focus ring, active input border |
| `text.primary` | `#EDEFF5` | Primary reading text |
| `text.secondary` | `#9AA1B2` | Timestamps, meta, placeholders |
| `text.disabled` | `#5A5F6B` | Disabled controls |
| `accent.primary` | `#7C9CFF` | Primary actions, links, active states |
| `accent.primary.hover` | `#93AEFF` | Hover state |
| `accent.success` | `#5FD98A` | Success toasts, connected status |
| `accent.warning` | `#F2B84B` | Consent banners, capture-exclusion caveat |
| `accent.danger` | `#F26D6D` | Errors, destructive actions |
| `syntax.keyword` | `#C792EA` | Code highlighting |
| `syntax.string` | `#C3E88D` | Code highlighting |
| `syntax.function` | `#82AAFF` | Code highlighting |
| `syntax.comment` | `#6B7280` | Code highlighting |
| `syntax.number` | `#F78C6C` | Code highlighting |

All colors defined once in `ui/theme/tokens.dart` (Dart) and mirrored as
constants in the native renderer (`renderer_d2d.cpp`) so both surfaces stay
in sync — never hardcode a hex value outside these two source-of-truth
files.

### 2.2 Typography

| Token | Font | Size | Weight | Usage |
|---|---|---|---|---|
| `type.display` | Segoe UI Variable Display | 20 | Semibold | Overlay header |
| `type.body` | Segoe UI Variable Text | 14 | Regular | Chat body text |
| `type.body.strong` | Segoe UI Variable Text | 14 | Semibold | Emphasis |
| `type.caption` | Segoe UI Variable Small | 12 | Regular | Meta, timestamps |
| `type.code` | Cascadia Code / Consolas fallback | 13 | Regular | Code blocks |

Line height: 1.5x for body text, 1.4x for code. Fallback to system default
UI font if Segoe UI Variable isn't present (older Windows 10 builds).

### 2.3 Spacing & Sizing

- Base unit: `4px`. Spacing scale: 4 / 8 / 12 / 16 / 24 / 32.
- Overlay default size: `380×520` px (resizable, min `280×320`, max
  constrained to 90% of the current monitor's work area).
- Corner radius: `12px` outer panel, `8px` cards/buttons, `6px` code blocks.
- Elevation: overlay panel uses a single soft drop shadow
  (`0 8px 24px rgba(0,0,0,0.35)`) — no multi-layer shadow stacks, keeps
  compositing cheap.

### 2.4 Motion

| Interaction | Duration | Easing |
|---|---|---|
| Overlay show | 160ms | ease-out (decelerate) |
| Overlay hide/fade | 120ms | ease-in (accelerate) |
| Auto-hide fade | 400ms | linear opacity |
| Drag | 0ms (direct follow) | — |
| Resize | 0ms (direct follow) | — |
| Hover state | 80ms | ease |
| Streaming text append | no animation on text itself; only the cursor/typing indicator pulses (600ms loop) |

Respect OS "Show animations in Windows" setting: when off, all durations
collapse to 0 (states still update immediately, just no tween).

---

## 3. Overlay Layout

```
┌──────────────────────────────────────────┐
│  ●●●  Assistant            ⚙  🖈  ─  ✕    │  ← header: drag handle (title
├──────────────────────────────────────────┤     area), settings, pin
│                                            │     (always-on-top toggle),
│   [Conversation stream — scrollable]      │     minimize, close
│                                            │
│   ┌ user bubble ─────────────┐            │
│   │ ...                      │            │
│   └──────────────────────────┘            │
│            ┌ assistant bubble ─────────┐  │
│            │ ...markdown/code...   [📋]│  │
│            └────────────────────────────┘ │
│                                            │
├──────────────────────────────────────────┤
│  [prompt template ▾]  [provider: GPT ▾]   │  ← quick controls row
├──────────────────────────────────────────┤
│  ┌──────────────────────────────────┐ ➤  │  ← input box + send
│  └──────────────────────────────────┘     │
└──────────────────────────────────────────┘
      ↕ resize handles on all edges/corners
```

- Header doubles as the drag region (`WM_NCHITTEST` returns `HTCAPTION`
  over it, except over the icon buttons).
- Pin icon toggles always-on-top independent of overlay visibility.
- Click-through mode replaces the header with a thin, semi-transparent
  strip (still draggable via hotkey-triggered "move mode," since normal
  clicks pass through).

---

## 4. Component Notes

### 4.1 Message bubbles
- User messages: right-aligned, `bg.elevated`, subtle border.
- Assistant messages: left-aligned, transparent background (blends into
  panel), separated by a thin `border.subtle` rule instead of a bubble
  outline — keeps long streaming answers from feeling boxed-in.
- Streaming state shows a small pulsing dot at the end of the growing text,
  not a separate "typing..." row.

### 4.2 Code blocks
- Header strip: language label (left), copy button (right).
- Copy button: icon-only, transitions to a checkmark for 1.5s after copy,
  then reverts — no toast for this specific action (toast would be
  overkill for a frequent, low-stakes action).
- Horizontal scroll for long lines rather than wrapping, to preserve code
  formatting; wrap toggle available per-block.

### 4.3 Consent / status banners
- Clipboard monitoring active: persistent thin banner, `accent.warning`
  left-border accent, icon + "Clipboard monitoring is on" + inline
  "Turn off" link. Always visible while active — never auto-dismisses.
- Capture-exclusion caveat: shown once as a dismissible info banner the
  first time the user enables it, plus always available as static text
  under the setting toggle ("Best-effort; not guaranteed with every
  screen-recording or sharing tool").

### 4.4 Region selector (full-screen, separate native surface)
- Dimmed full-screen scrim (`rgba(0,0,0,0.35)`) with a bright rectangle
  cutout following the drag.
- Dimensions label follows the cursor (`320×180 px`).
- Esc cancels; Enter confirms once a region is drawn; click-drag is the
  primary path.

### 4.5 Settings
- Standard Fluent-style navigation: left rail (Providers, Hotkeys, Overlay,
  Privacy, About), content pane on the right.
- Privacy pane groups every consent-gated feature (Clipboard, OCR, Capture
  exclusion) together with its current state and a one-click disable-all.

---

## 5. Accessibility

- Minimum contrast: body text on `bg.base`/`bg.elevated` meets WCAG AA
  (4.5:1) — verified for the token pairs above.
- Focus rings always visible on keyboard navigation (`border.focus`, 2px,
  2px offset), suppressed only on pointer interaction (`:focus-visible`
  equivalent behavior in native renderer).
- All icons paired with accessible names for UI Automation / screen
  readers, even in the Direct2D render mode (exposed via a parallel
  UI Automation provider on the overlay HWND).
- Motion respects the OS reduced-motion setting (see §2.4).

---

## 6. Iconography & Imagery
- Line-style icons, 1.5px stroke, 20×20 base grid, consistent with Fluent
  System Icons where possible (or a matching custom set) — no photographic
  imagery in the core UI.
- No copyrighted third-party logos beyond provider name text labels (e.g.
  "GPT," "Claude," "Gemini," "Ollama") — no reproduction of provider logo
  artwork without express license.

---

## 7. Empty / Edge States
- No conversations yet: centered icon + "Ask anything — press `⌘/Ctrl` +
  your hotkey to start" (reflects the user's actual configured hotkey, not
  a hardcoded default).
- Provider not configured: input box disabled, inline prompt "Add an API
  key in Settings → Providers" with a direct link.
- Network error mid-stream: partial message kept, red inline note below it
  ("Connection lost — [Retry]"), not a full-screen error state.
