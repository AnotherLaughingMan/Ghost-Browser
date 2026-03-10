# Ghost Browser — Development Bible

> This document is the canonical authority on what Ghost Browser is, why it exists, how it is built, and the principles that govern every decision made in this codebase. It supersedes any contradictory guidance in stray comments or outdated notes. It is a living document — update it when the product or architecture meaningfully changes.

---

## Table of Contents

1. [Why Ghost Exists](#1-why-ghost-exists)
2. [What Ghost Is](#2-what-ghost-is)
3. [What Ghost Is Not](#3-what-ghost-is-not)
4. [Core Principles](#4-core-principles)
5. [Architecture](#5-architecture)
6. [Technology Stack](#6-technology-stack)
7. [Source Map](#7-source-map)
8. [Build System](#8-build-system)
9. [Feature State Registry](#9-feature-state-registry)
10. [Settings System](#10-settings-system)
11. [Privacy & Security Model](#11-privacy--security-model)
12. [Assembly Hot-Path Policy](#12-assembly-hot-path-policy)
13. [TypeScript / JavaScript Policy](#13-typescript--javascript-policy)
14. [UI & Design Language](#14-ui--design-language)
15. [Platform Policy](#15-platform-policy)
16. [Decision Log](#16-decision-log)
17. [Roadmap Principles](#17-roadmap-principles)

---

## 1. Why Ghost Exists

Modern browsers have drifted away from the user. They collect telemetry. They bundle AI features with unclear data routing. They default to permissive stances on tracking, fingerprinting, and third-party cookies. They are slow to start, heavy at idle, and increasingly opaque about what they do on behalf of advertisers versus the person sitting at the keyboard.

Ghost exists because none of that is acceptable.

The premise is simple: **a browser should be a tool that the user controls, not a platform that controls the user.** Ghost is built to be fast, transparent, and private by default — with genuine enforcement, not checkbox privacy.

Ghost is also a direct response to the complexity trap most browsers fall into. Features accumulate. Settings sprawl. The product becomes something that requires a manual. Ghost should never need a manual. Every feature earns its place by being useful to the person using the browser, not to a monetization layer they cannot see.

---

## 2. What Ghost Is

Ghost Browser is a **standalone desktop web browser** built on Qt 6 and Qt WebEngine (Chromium). It is:

- **Lightweight.** Cold-start fast, low idle RAM. No background services that the user did not enable.
- **Private by default.** Do Not Track, third-party cookie blocking, HTTPS-only mode, fingerprint mitigation, and tracker blocking ship on by default — not as opt-in extras.
- **Frameless and native.** A custom chrome built on Qt Widgets with no OS-native title bar. The UI is a first-class product decision, not a placeholder for a later designer.
- **Cross-platform by design.** Windows-first in active development (MSVC 2022, Win64), but the codebase is structured to reach Linux and macOS without architectural surgery.
- **Honest.** If a setting is not implemented, it is marked "Coming Soon". If a feature is partial, the audit says so. The UI does not lie about what the browser does.
- **Open source.** Built in the open. All decisions logged. No telemetry phoned home.

**Current version:** `0.1.2`  
**Engine:** Qt WebEngine 6.9.3 (Chromium), rebuilt from source with proprietary codec support (H.264, AAC, H.265, MP3 and more)  
**Primary platform:** Windows x64 (MSVC 2022)

---

## 3. What Ghost Is Not

Understanding what Ghost refuses to be is as important as understanding what it is.

| Ghost is not… | Because… |
|---|---|
| A telemetry platform | No analytics, no usage beacons, no crash reporting to a remote server |
| An AI-first browser | AI-assist features are deferred until a privacy trust model is clearly defined |
| An attachment/file-transfer browser | File sharing features (e.g., cross-device transfers) are rejected due to exploitation and CSAM risk |
| A Chromium fork | Ghost wraps Chromium via Qt WebEngine — it does not patch Chromium internals |
| A feature accumulator | Features without clear purpose and without real backend wiring are not shipped |
| A single-platform product | Windows-specific code is behind `#ifdef` guards and isolated from core paths |
| A settings checkbox graveyard | Every setting in the UI must be wired to real behavior before it ships without a "Coming Soon" tag |

---

## 4. Core Principles

These principles govern every line of code written in this codebase. When a decision is unclear, return to these.

### P1 — Privacy first, always
Default settings protect the user. There is no "opt-in to privacy" model. Tracking is blocked, fingerprinting is mitigated, third-party cookies are denied, and HTTPS is enforced — out of the box, before the user touches a single setting.

### P2 — Honest UI
The settings page is a contract with the user. If a toggle does not do what it says, it must be either implemented or marked "Coming Soon." UI that lies — even by omission — erodes trust and will not be merged.

### P3 — Minimum complexity
Code is a liability. Every line of code is a line that can contain a bug, must be maintained, and must be understood by the next person working here. Add what is needed. Do not add what is not. Do not refactor working code to satisfy aesthetic preferences.

### P4 — Every feature earns its place
New features go through a simple test: does this give the person using the browser more control, more speed, more privacy, or more clarity? If the answer is no, the feature does not ship.

### P5 — Performance is not optional
Ghost must start fast and stay light. Avoid unnecessary allocations in hot paths. Reuse objects. Move. Pass by reference. Do not create threads without a clear use case. Disk I/O and network calls are never synchronous on the UI thread.

### P6 — No global mutable state
State is owned by a specific component. It is passed explicitly. There are no singletons beyond what Qt requires. There is no shared mutable global that a component modifies as a side effect.

### P7 — Platform isolation
Windows-specific code is behind `#ifdef Q_OS_WIN` or CMake guards. It lives in the narrowest possible scope. Core logic (`src/core/`, `src/browser/`, `src/network/`) must compile and behave correctly on all target platforms.

### P8 — ASM is the opt-in, C++ is the default
Every assembly routine has a portable C++ fallback that ships by default. The ASM path is enabled only when profiling justifies it on a specific platform. ASM files are leaf-level, allocation-free, and fully documented.

### P9 — Build both configurations, every time
Every code change is validated in both Debug and Release before it is considered done. Debug validates correctness. Release validates that optimizations do not break behavior. Skipping either is not acceptable.

### P10 — The changelog is code
`CHANGELOG.md` is updated when changes land, not as an afterthought before a release. The changelog is the public record of intent and should be readable by someone who has never seen the source.

---

## 5. Architecture

Ghost is organized into five layers that respect a strict dependency direction: higher layers may depend on lower layers, but lower layers never depend on higher layers.

```
┌─────────────────────────────────────────┐
│                  src/ui/                │  Window, tabs, nav bar, widgets
│             (MainWindow et al.)         │
└────────────────┬────────────────────────┘
                 │ depends on
┌────────────────▼────────────────────────┐
│               src/browser/              │  Engine integration, request interception,
│  (GhostRequestInterceptor,              │  fingerprint protection
│   FingerprintingProtection)             │
└────────────────┬────────────────────────┘
                 │ depends on
┌────────────────▼────────────────────────┐
│                src/core/                │  Settings, bookmarks, history, cookies,
│  (SettingsManager, BookmarkManager,     │  weather, protection diagnostics, ASM
│   HistoryManager, CookieManager,        │  fallbacks
│   WeatherService, ProtectionDiagnostics,│
│   asm_fallbacks)                        │
└────────────────┬────────────────────────┘
                 │ depends on
┌────────────────▼────────────────────────┐
│              src/network/               │  (Reserved for future network filter layer)
└────────────────┬────────────────────────┘
                 │ depends on
┌────────────────▼────────────────────────┐
│              src/app/                   │  main.cpp — process entry, lifecycle only
└─────────────────────────────────────────┘
```

**Key constraints:**
- `src/core/` has no knowledge of `src/ui/`. Core services do not call into the window.
- `src/app/` does nothing except construct `MainWindow` and start the event loop.
- `src/network/` is currently reserved. Do not put filtering logic directly in `browser/` — put it through `network/` once the filter layer is built.
- The Qt WebChannel bridge (`QWebChannel`) is the *only* path from JavaScript running in a browser page back to C++. There is no other cross-context call path.

### Component Responsibilities

| Component | Responsibility |
|---|---|
| `MainWindow` | Single application window; owns the tab bar, navigation bar, bookmarks bar, content stack, and all Qt dock widgets. Coordinates all other components. |
| `SettingsManager` | Owns persisted user settings (JSON). Exposes `Q_INVOKABLE` slots over QWebChannel. Emits `settingsChanged` when settings mutate. |
| `BookmarkManager` | Persists bookmarks to `bookmarks.json`. Provides import/export (HTML, Ghost JSON). Exposed over QWebChannel. |
| `HistoryManager` | Persists browsing history to `history.json` (capped at 10,000 entries). Exposed over QWebChannel. |
| `CookieManager` | Provides cookie inspection and deletion via `QNetworkCookieJar`. Exposed over QWebChannel. |
| `GhostRequestInterceptor` | Intercepts all network requests; enforces DNT, HTTPS-only, tracker blocking, script blocking, safe browsing, and HTTPS upgrade. |
| `FingerprintingProtection` | Injects JavaScript into loaded pages to standardize client hints, hardware identifiers, WebGL renderer strings, and disable the Battery API. |
| `ProtectionDiagnostics` | Accumulates blocked-request and HTTPS-upgrade events for display in the Protection settings panel. |
| `WeatherService` | Fetches weather data for the new tab dashboard. |
| `LoadingCurtainWidget` | Grey overlay that fades out after page load, eliminating the white flash on heavy sites. |
| `StatusBubbleWidget` | Chromium-style status bubble at the bottom of the window for hovered link previews. |
| `asm_fallbacks` | Portable C++ implementations of every assembly hot-path routine. These are the compile-time default. |

### State Persistence Files

| File | Owner | Contents |
|---|---|---|
| `settings.json` | `SettingsManager` | All user-facing settings |
| `bookmarks.json` | `BookmarkManager` | Bookmark entries |
| `history.json` | `HistoryManager` | Browsing history (capped at 10,000) |
| `window-state.json` | `MainWindow` | Window geometry and maximized state |
| `devtools-state.json` | `MainWindow` | DevTools placement, open state, detached geometry, dock size |
| `session.json` | `MainWindow` | Open tabs and active tab index |

All persistence files are written to `QStandardPaths::AppConfigLocation`. They are never stored in the source tree, the build directory, or any cloud-synced path.

---

## 6. Technology Stack

| Layer | Technology | Rationale |
|---|---|---|
| Language | C++20 (64-bit only) | Performance, RAII, Qt compatibility, long-term maintainability |
| UI framework | Qt 6.9+ Widgets | Native desktop, cross-platform, no Electron overhead |
| Browser engine | Qt WebEngine 6.9.3 (Chromium) | Open source, cross-platform, maintained, wide standards support |
| Codec support | Qt WebEngine rebuilt from source with `-DFEATURE_webengine_proprietary_codecs=ON` | H.264, AAC, H.265, MP3, Opus, FLAC etc. without licensing cost being paid per-install |
| Settings bridge | QWebChannel + TypeScript | Typed, auditable JS-to-C++ data path |
| TypeScript | TypeScript 5+, compiled by `tsc` via CMake | Type safety for settings bridge; compiles to `assets/js/` |
| Assembly | MASM (Windows), GAS (Linux/macOS) | Profiling-justified hot paths only; SSE4.2 CRC32C, vectorized memset |
| Config | JSON + JSON Schema | Human-readable, self-documented, validated at load |
| Build | CMake 3.21+ | Cross-platform, multi-config (Debug/Release), integrates ASM and TS compilation |
| Icons | Feather Icons (MIT) | Consistent, open-licensed, dark/light variants |

### Qt Installation (Current Dev Environment)

- **Base Qt:** `C:\Qt\6.9.3\msvc2022_64`
- **Custom WebEngine (proprietary codecs):** `C:\Qt\6.9.3-custom-codecs`
- **Configure command:**
  ```
  cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.9.3-custom-codecs;C:/Qt/6.9.3/msvc2022_64"
  ```
- Custom codecs prefix must come **first** so CMake's WebEngine package resolution finds the codec build.

---

## 7. Source Map

```
Ghost/
├── asm/
│   ├── README.md              # ASM policy and calling convention docs
│   ├── win/
│   │   ├── crc32_url.asm      # SSE4.2 CRC32C URL hashing (MASM, Microsoft x64 ABI)
│   │   └── memset_u32.asm     # SSE2 vectorized 32-bit memset (MASM, Microsoft x64 ABI)
│   ├── linux/                 # Reserved for GAS sources (System V AMD64 ABI)
│   └── macos/                 # Reserved for GAS sources (System V AMD64 ABI)
│
├── assets/
│   ├── ghost_icons.qrc        # Qt resource manifest — compiles icons + pages + JS into binary
│   ├── ghost.rc               # Windows application resource (version info, app icon)
│   ├── icons/
│   │   ├── dark/              # Feather Icons at #E0E0E0 — used in dark mode
│   │   └── light/             # Feather Icons at #424242 — used in light mode
│   ├── js/
│   │   └── settings-bridge.js # Compiled output from web/src/settings-bridge.ts — DO NOT EDIT DIRECTLY
│   ├── pages/
│   │   ├── settings.html      # ghost://settings full UI
│   │   ├── newtab.html        # ghost://newtab dashboard
│   │   └── codec-test.html    # ghost://codec-test diagnostic page
│   └── placeholders/
│       └── default_profile_placeholder.png  # Default user avatar
│
├── config/
│   ├── defaults.json          # Default values for every user-facing setting
│   └── settings-schema.json   # JSON Schema — validated at SettingsManager load
│
├── src/
│   ├── app/
│   │   └── main.cpp           # Process entry point — constructs MainWindow, starts event loop
│   ├── browser/
│   │   ├── FingerprintingProtection.cpp/.h   # JS injection for fingerprint mitigation
│   │   └── GhostRequestInterceptor.cpp/.h    # Network-level privacy enforcement
│   ├── core/
│   │   ├── asm_fallbacks.cpp/.h              # Portable C++ fallbacks for every ASM routine
│   │   ├── BookmarkManager.cpp/.h            # Bookmark persistence and import/export
│   │   ├── CookieManager.cpp/.h              # Cookie inspection and management
│   │   ├── HistoryManager.cpp/.h             # Browsing history persistence
│   │   ├── ProtectionDiagnostics.cpp/.h      # Blocked-request event accumulator
│   │   ├── SettingsManager.cpp/.h            # Settings persistence and QWebChannel bridge
│   │   └── WeatherService.cpp/.h             # New tab weather data fetch
│   ├── network/                              # Reserved — future network filter layer
│   └── ui/
│       ├── LoadingCurtainWidget.cpp/.h       # Page-load grey overlay with fade-out
│       ├── MainWindow.cpp/.h                 # Primary application window (tabs, chrome, dev tools)
│       └── StatusBubbleWidget.cpp/.h         # Hovered link status bubble
│
└── web/
    ├── tsconfig.json           # TypeScript compiler config — outputs to assets/js/
    └── src/
        └── settings-bridge.ts  # TypeScript source for settings/history/cookie/bookmark bridge
```

---

## 8. Build System

### Commands

```powershell
# Configure (run once, or after CMakeLists changes)
cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.9.3-custom-codecs;C:/Qt/6.9.3/msvc2022_64"

# Build Debug
cmake --build build --config Debug --target GhostBrowser

# Build Release
cmake --build build --config Release --target GhostBrowser
```

### Output

- `build/Debug/Ghost Browser.exe`
- `build/Release/Ghost Browser.exe`

### What the build does beyond compiling C++

1. **TypeScript compilation** — `tsc` compiles `web/src/settings-bridge.ts` → `assets/js/settings-bridge.js` as a CMake pre-build step. **Edit the `.ts` file, not the `.js` file.** The `.js` is overwritten on every build.
2. **Qt resource compilation** — `rcc` bundles all HTML, JS, CSS, and icons listed in `assets/ghost_icons.qrc` into the binary. Changing an HTML or JS file requires a rebuild; the running binary serves the compiled-in version.
3. **MOC** — Qt's meta-object compiler generates signal/slot glue for all `Q_OBJECT` classes automatically via `CMAKE_AUTOMOC`.
4. **windeployqt** — copies all required Qt DLLs alongside the exe.
5. **Custom-codecs DLL overlay** — a second post-build step overwrites the stock WebEngine DLLs that windeployqt placed with the proprietary-codec build. This must run after windeployqt to avoid being clobbered.
6. **Config file deployment** — `config/defaults.json` and `config/settings-schema.json` are copied alongside the exe.

### Two-build rule

**Both Debug and Release must be built and pass on every change.** Debug validates correctness with full assertions and no inlining. Release validates that compiler optimizations do not break behavior. Never ship or commit a change that only passes one configuration.

---

## 9. Feature State Registry

This is the authoritative list of features and their current implementation status.

### Fully Implemented

| Feature | Entry Point |
|---|---|
| Tabbed browsing with custom frameless title bar | `MainWindow` |
| Custom navigation bar (Back, Forward, Reload, Home, URL bar, Menu) | `MainWindow` |
| Frameless window with native Win32 hit-testing for resize and Snap Layouts | `MainWindow::nativeEvent` |
| Ghost New Tab dashboard (clock, search, weather, briefing, shortcuts) | `ghost://newtab` |
| Settings page | `ghost://settings` |
| Hamburger menu (Settings, New Tab, History, Bookmarks, Codec Test, Exit) | `MainWindow` |
| Browsing history with search, time-range filtering, per-entry delete | `HistoryManager` |
| Cookie inspector with search and delete by domain | `CookieManager` |
| Bookmark manager with toolbar mirroring | `BookmarkManager` |
| Bookmark import (HTML, Ghost JSON) and export | `BookmarkManager` |
| Current-page bookmark actions from browser menu | `MainWindow` |
| Do Not Track enforcement | `GhostRequestInterceptor` |
| Third-party cookie blocking | `GhostRequestInterceptor` / Qt cookie filter |
| HTTPS-only mode | `GhostRequestInterceptor` |
| HTTPS upgrade (separate from strict-only) | `GhostRequestInterceptor` |
| Tracker/ad blocking (standard + aggressive tiers) | `GhostRequestInterceptor` |
| Third-party script blocking | `GhostRequestInterceptor` |
| Safe browsing (host denylist + risky download type filter) | `GhostRequestInterceptor` |
| Fingerprint mitigation (client hints, WebGL, hardware IDs, Battery API) | `FingerprintingProtection` |
| Protection diagnostics (blocked request and HTTPS upgrade log) | `ProtectionDiagnostics` |
| Site-level permission management (notifications, location, camera, mic) | `MainWindow`, site-info popup |
| Hardware acceleration (GPU flags at startup) | `main.cpp` |
| Proprietary codec support (H.264, AAC, H.265, MP3, etc.) | Custom Qt WebEngine build |
| Settings persistence and live apply | `SettingsManager` |
| Settings import from file | `SettingsManager` |
| Theme (dark / light / system-follow) | `MainWindow`, `SettingsManager` |
| Font size and zoom level | `SettingsManager` → WebEngine |
| Window placement persistence | `MainWindow` (window-state.json) |
| Session restore (tabs + active index) | `MainWindow` (session.json) |
| DevTools (F12/Ctrl+Shift+I), docked and detached, fully persisted | `MainWindow` (devtools-state.json) |
| Startup behavior (new tab / last session / specific page) | `SettingsManager` |
| YouTube Shorts → normal video URL rewrite | `GhostRequestInterceptor` |
| Download folder selection + ask-where-to-save | `MainWindow`, `SettingsManager` |
| Clear browsing data on demand and on exit | `MainWindow`, `SettingsManager` |
| Default browser check with live status badge | `ghost://settings > Get Started` |
| Profile name persistence | `SettingsManager` |
| New tab modules (weather, shortcuts, briefing, focus) toggle | `SettingsManager` |
| Loading curtain (eliminates white flash on heavy sites) | `LoadingCurtainWidget` |
| Chromium-style status bubble (hovered link) | `StatusBubbleWidget` |
| Middle-click tab to close | `MainWindow` |
| Middle-click URL bar to clear | `MainWindow` |
| x64 ASM hot paths: CRC32C URL hashing, vectorized memset | `asm/win/` |
| C++ fallbacks for all ASM routines | `src/core/asm_fallbacks` |
| Spell check | `SettingsManager` |
| High contrast mode | `SettingsManager` |
| Proxy mode (system / direct) | `SettingsManager` |
| DNS prefetching | Applied per view |
| Smooth scrolling | Applied per view |

### Partially Implemented

| Feature | Gap |
|---|---|
| Profile avatar | Name works; avatar is a static placeholder until real profile system ships |
| Background apps setting | Persisted, but no background process policy enforced |
| Manual proxy configuration | `system` and `none` modes work; host/port entry not yet implemented |
| Caret browsing | Persisted, not applied to pages |

### Not Yet Implemented (UI Placeholder Exists)

| Feature | Notes |
|---|---|
| Search engine management | No custom engine storage |
| Extensions | No extension subsystem; decision whether to build one is pending |
| Password manager | Architecture not defined |
| Payment methods autofill | Architecture not defined |
| Addresses autofill | Architecture not defined |
| Display language / localization | No i18n system |
| Parental controls | Architecture pending; must be on-device, profile-local, privacy-preserving |
| Linux build | Compilation guards in place; not yet tested on Linux toolchain |
| macOS build | Compilation guards in place; not yet tested on macOS toolchain |

---

## 10. Settings System

### How it works

1. `SettingsManager` loads `settings.json` at startup, merging missing keys from `defaults.json`.
2. Schema validation runs against `settings-schema.json` at load. Invalid or unknown keys are ignored; defaults fill gaps.
3. `SettingsManager` exposes all mutation as `Q_INVOKABLE` methods over `QWebChannel`, accessible from `ghost://settings`.
4. When a setting changes, `SettingsManager` emits `settingsChanged(json)` with the full updated settings payload.
5. `MainWindow` listens to `settingsChanged` and applies changes immediately to open tabs and native UI.

### Key rules

- **Leaf paths only.** `updateSetting(path, value)` is designed for scalar leaf values like `"appearance.theme"`, not for nested object writes. Nested state (like DevTools placement) lives in its own direct-write JSON file.
- **No nested QVariantMap writes.** `QJsonValue::fromVariant(QVariantMap{...})` with sub-maps is fragile across Qt versions. Any state that requires nested structure gets its own file.
- **No SettingsManager for UI-internal state.** DevTools placement lives in `devtools-state.json`. Window geometry lives in `window-state.json`. Session state lives in `session.json`. These are direct `QFile` + `QJsonDocument` writes and never touch SettingsManager.
- **Every `updateSetting` call emits `settingsChanged`.** Avoid calling it in tight loops or during initialization.

### Adding a new setting

1. Add the key and default value to `config/defaults.json`.
2. Add the schema entry to `config/settings-schema.json`.
3. Add a `Q_INVOKABLE` getter/setter or use the generic `updateSetting(path, value)` path.
4. Wire real behavior in `MainWindow::applyXxxSettings()` or the relevant `apply*` slot.
5. Add the UI control to `assets/pages/settings.html`.
6. Add the TypeScript type annotation to `web/src/settings-bridge.ts`.
7. Update `SETTINGS_AUDIT_AND_PLAN.md` to move the row from "Not Implemented" to the correct status.
8. Update `CHANGELOG.md`.

---

## 11. Privacy & Security Model

Ghost's privacy model has three layers:

### Layer 1 — Network enforcement (GhostRequestInterceptor)

Runs on every request before it leaves the machine:
- **DNT header** — `DNT: 1` on every request when enabled
- **HTTPS-only mode** — HTTP navigations are blocked or redirected
- **HTTPS upgrade** — HTTP navigations are silently upgraded to HTTPS where possible
- **Tracker/ad blocking** — requests to a curated host list are blocked at two tiers (standard: ad networks; aggressive: ad networks + analytics + social pixels)
- **Third-party script blocking** — non-first-party script requests blocked when enabled
- **Safe browsing** — requests to a curated malicious/phishing denylist blocked; risky download file types rejected

### Layer 2 — Page-level mitigation (FingerprintingProtection)

Injected as JavaScript into every page after load:
- Standardizes `navigator.userAgentData` (client hints)
- Normalizes `navigator.hardwareConcurrency` and `navigator.deviceMemory`
- Stubs `navigator.getBattery()` to a permanently-charged, non-identifying value
- Normalizes WebGL renderer and vendor strings to a standard identifier

### Layer 3 — Engine configuration

Applied at profile and view construction time:
- Third-party cookie blocking via Qt's `QWebEngineCookieStore` filter hook
- Persistent named profile (`QWebEngineProfile("Ghost")`) for stable Chromium storage
- `LocalStorageEnabled` and `JavascriptEnabled` controlled per setting
- `JavascriptCanOpenWindows` controlled by the popups setting
- `PlaybackRequiresUserGesture` controlled by autoplay setting

### Security surface rules

- No cross-origin data is passed from browser JS to C++ except through the audited QWebChannel bridge.
- The settings bridge is attached **only to `ghost://settings`**, never to arbitrary web pages.
- `Q_INVOKABLE` methods on `SettingsManager` are the only JavaScript-callable C++ surface. This surface must be audited whenever new methods are added.
- All user-supplied strings that become file paths (download paths, import paths) must be validated before use. Never concatenate user input into `QFile` paths without sanitization.
- Registry reads (default browser detection) use `RegOpenKeyExW` with `KEY_READ` access only.

---

## 12. Assembly Hot-Path Policy

ASM hot paths exist for one reason: profiling showed a specific C++ implementation was a measurable bottleneck, and an architecture-specific implementation provides a meaningful gain.

**Current routines:**
- `ghost_crc32_url_asm` — SSE4.2 CRC32C hashing for URL lookup (Windows x64 / MASM)
- `ghost_memset_u32_asm` — SSE2 vectorized 32-bit memset (Windows x64 / MASM)

**Rules:**
1. Every ASM routine has an identical-behavior C++ fallback in `src/core/asm_fallbacks.h/.cpp`. The C++ path is the default. ASM is opt-in.
2. ASM files are leaf-only. No Qt calls. No framework calls. No heap allocation.
3. Pre-allocated buffers are always passed from C++.
4. Document calling convention (Microsoft x64 on Windows, System V AMD64 ABI on Linux/macOS), all modified registers, and alignment requirements in a comment block at the top of each file.
5. New ASM routines require a profiling result that justifies them. "This might be faster" is not a justification.
6. ASM routines are tested against their C++ fallback for output parity before merging.

---

## 13. TypeScript / JavaScript Policy

- TypeScript lives in `web/src/`. It is compiled by `tsc` to `assets/js/` as part of the CMake build.
- **Never edit `assets/js/settings-bridge.js` directly.** It is generated output. The build overwrites it. All changes belong in `web/src/settings-bridge.ts`.
- TypeScript is used only where browser/web-context behavior is required (i.e., code that runs inside the WebEngine page, not C++ business logic).
- The TypeScript interface mirrors the C++ `Q_INVOKABLE` surface of `SettingsManager`, `BookmarkManager`, `HistoryManager`, and `CookieManager`. When a new `Q_INVOKABLE` method is added to any of these, the TypeScript interface in `settings-bridge.ts` must be updated to match.
- Keep the TypeScript minimal. It is a bridge, not a framework. No large JS dependencies. No bundler. Direct `tsc` compilation only.

---

## 14. UI & Design Language

### Chrome philosophy
Ghost's UI is a **frameless native window** with a custom title bar that integrates the tab strip. There is no OS-native title bar. The window chrome is a first-class product decision — it must look intentional, not like a Qt widget default.

### Color palette (dark mode — default)

| Role | Value |
|---|---|
| App background | `#1E1E2E` |
| Panel / card background | `#252535` |
| Border / separator | `#2A2A3C` |
| Primary text | `#E0E0E0` |
| Secondary text / sublabel | `#888` |
| Accent (links, active tabs, badges) | `#7C7FE8` |
| Icon stroke (dark mode) | `#E0E0E0` |
| Icon stroke (light mode) | `#424242` |
| Danger / close hover | `#E06C75` |
| Success badge | `#3DBE6D` |
| Error badge | `#CF4D6F` |
| Warning / coming-soon tag | `#F4A261` |

### Status tags
Tags in the settings UI communicate feature readiness:
- **Orange** — "Coming Soon" (feature visible but not wired)
- **Green** (`tag-ok`) — positive live status (e.g., "Default")
- **Red** (`tag-err`) — negative live status (e.g., "Not Default")
- **Gray** (`tag-dim`) — neutral or unsupported status

Tags must never be used to decorate working features with category labels. A working row does not get a tag.

### Icon set
Feather Icons (MIT licensed). All icons are SVG, loaded from the Qt resource system. Dark and light variants live in `assets/icons/dark/` and `assets/icons/light/` respectively. The `iconPath(name)` helper in `MainWindow` selects the correct variant.

### Tooltips
Every icon-only button must have a tooltip. Text-labelled controls do not require tooltips, but may have them if the label alone is ambiguous.

### File size discipline
No source file in `src/` should exceed 600 lines. When a file approaches that limit it must be split into smaller, focused files. This is a maintainability constraint, not a style preference.

---

## 15. Platform Policy

Ghost targets three platforms: **Windows** (active), **Linux** (planned), **macOS** (planned).

### Rules

- `src/core/`, `src/browser/`, `src/network/`, and `src/app/` must compile and behave correctly on all three platforms.
- Windows-only code (Win32 calls, COM, registry access, `ShellExecuteW`) must be inside `#ifdef Q_OS_WIN` blocks.
- Any platform-specific path that reaches a core abstraction must go through a Qt abstraction or a `#ifdef`-guarded adapter.
- Do not use Windows SDK types (`HINSTANCE`, `HWND`, etc.) outside of files that are already `#ifdef Q_OS_WIN` guarded.
- `src/ui/MainWindow.cpp` contains `nativeEvent` for Windows Snap Layouts and resize hit-testing. That block is `#ifdef Q_OS_WIN` guarded. Future Linux/macOS native event handling follows the same pattern.
- ASM sources are CMake-conditional per platform. If an ASM source has no cross-platform equivalent yet, its slot in `GHOST_ASM_SOURCES` is left empty with a comment marking it reserved.

---

## 16. Decision Log

A record of non-obvious decisions made during development and the reasoning behind them.

| Date | Decision | Reason |
|---|---|---|
| 2026-03-06 | Custom frameless window instead of OS title bar | Design requirement; allows tab integration into the chrome at zero OS chrome overhead |
| 2026-03-06 | Qt WebEngine rebuilt from source for proprietary codecs | Pre-built Qt packages do not include H.264/AAC; rebuilding is the only way to ship codec support without per-install licensing |
| 2026-03-06 | Settings bridge attached only to `ghost://settings`, not all tabs | Attaching QWebChannel to every tab adds measurable overhead on media-heavy sites and is a security surface expansion |
| 2026-03-06 | Named persistent QWebEngineProfile ("Ghost") | Chromium state (cookies, DevTools theme, localStorage) must survive restarts; an off-the-record profile would lose it |
| 2026-03-07 | History capped at 10,000 entries | Unbounded growth would make history.json a performance liability; 10k entries covers realistic usage |
| 2026-03-07 | Third-party cookie blocking via cookie store filter, not engine setting | Qt's per-request `thirdParty` flag gives more accurate blocking than the engine's coarser cookie policy knob |
| 2026-03-08 | DevTools state in devtools-state.json, not SettingsManager | `updateSetting()` is designed for scalar leaf paths; nested `QVariantMap` passed to `QJsonValue::fromVariant` is fragile and also fires `settingsChanged` on every call, cascading a full settings re-apply |
| 2026-03-08 | Direct file I/O for window-state.json, devtools-state.json, session.json | These are operational state, not user preferences. Routing them through SettingsManager is wrong: it would fire settingsChanged on every geometry event, polluting the settings apply cycle |
| 2026-02-28 | No file/image transfer features | CSAM and exploitation risk; reputation risk for a browser product at this stage |
| 2026-02-28 | AI-assist features deferred | No privacy trust model defined; do not ship features with unclear data routing |

---

## 17. Roadmap Principles

Ghost's roadmap is governed by the same principles as its codebase: do less, do it right.

### Near-term priorities (ordered)

1. **Finish partially-implemented settings** — background apps, manual proxy, caret browsing.
2. **Profile system** — real profile creation, avatar upload with size validation (256×256 to 1024×1024, square), and locked child profiles as the foundation for parental controls.
3. **Parental controls** — on-device only. Site allow/block lists, SafeSearch enforcement, YouTube Restricted Mode, time windows, download restrictions, and guardian-managed unlock. No identity verification. No cloud. Hooks into the profile system.
4. **Search engine management** — predefined engines plus user-defined custom engines.
5. **Linux build validation** — get the existing source compiling cleanly on a GCC/Clang Linux target.

### Medium-term

6. **Ad/tracker filter list engine** — move from the curated host list to a proper filter-list engine (e.g., Adblock Plus filter syntax) so Ghost can consume publicly maintained lists.
7. **Extension decision** — decide whether Ghost builds an extension host. If yes, define the surface and security model before writing a line of code. If no, remove the placeholder from settings.
8. **Autofill / password manager** — define the storage architecture (local-only, encrypted) before wiring the UI.
9. **macOS build** — once Linux is clean, macOS follows the same path.

### What Ghost will not do

- **Telemetry.** No usage data, no crash reports to a remote server.
- **AI features without a defined privacy model.** The data routing must be explicit and user-controlled before any AI surface ships.
- **Attachment or file transfer between devices.** Out of scope permanently.
- **Cloud sync of any kind without explicit, informed user consent and local encryption.**
- **Features that exist only to increase engagement metrics.** Ghost has no engagement metrics.

---

*Last updated: 2026-03-10*  
*Maintainer: Ghost Browser project — update this document when architecture or product decisions change.*
