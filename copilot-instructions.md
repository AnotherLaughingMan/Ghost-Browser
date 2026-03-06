# Copilot Instructions — Ghost Browser

## Project Vision

Ghost is a **tiny, lightweight desktop web browser** with:

- 64-bit memory support
- A tabbed browsing interface
- Privacy and security as primary product priorities
- Fast startup and low RAM usage
- Future built-in ad-blocking (planned, not required in MVP)

## Core Principles

1. Keep the codebase small, readable, and modular.
2. Prioritize privacy and security by default in all user-facing behavior.
3. Prioritize performance and memory efficiency over feature sprawl.
4. Make minimal, targeted changes; avoid unrelated refactors.
5. Preserve backward-compatible behavior unless explicitly requested.
6. Build with clear separation between UI, browser engine integration, and core services.

## Privacy & Security Requirements

- Default to privacy-protective settings where feasible.
- Minimize data retention and avoid collecting telemetry unless explicitly enabled.
- Treat third-party script/network activity as untrusted input.
- Add secure defaults for network, storage, and permissions surfaces.
- Prefer local processing when possible to reduce data exposure.

## Technology Direction

- Primary language: **C++ (64-bit builds only)**
- Performance-critical hot paths may use **x64 Assembly** when profiling justifies it
- Lightweight tooling/injection helpers may use **TypeScript/JavaScript**
- Runtime/app configuration and schema validation use **JSON**
- UI: **Qt 6 + QtWebEngine** cross-platform desktop stack with tab support
- Browser engine integration should be abstracted behind interfaces
- Keep third-party dependencies minimal and documented

## Cross-Platform & OSS Requirements

- Ghost must remain cross-platform by design (Windows, Linux, macOS targets).
- Use open-source browser foundations only (QtWebEngine/Chromium path selected).
- Do not use OS-vendor-locked browser controls or proprietary engine wrappers.
- Do not add platform-specific UI libraries or APIs in core app flows.
- Any unavoidable platform-specific code must be isolated behind abstractions and kept out of app/core logic.

## QtWebEngine Policy

- Use Qt widgets and QtWebEngine APIs for browser UI and rendering integration.
- Keep UI logic framework-native and cross-platform; avoid platform-conditional behavior in normal flows.
- Do not introduce alternative UI frameworks unless explicitly requested.

## Qt Installation Record (Local Dev Environment)

- Installed on: **2026-03-01**
- Installed by: GitHub Copilot (automated terminal setup)
- Installer used: `winget` + `aqtinstall`
- Qt version: **6.8.2**
- Qt target: `windows desktop win64_msvc2022_64`
- Install location: `C:\Qt\6.8.2\msvc2022_64`

### Required Qt Components

- `Qt6Widgets`
- `Qt6WebEngineWidgets`
- `qtwebengine` (module)
- `qtwebchannel` (module dependency for WebEngine)
- `qtpositioning` (module dependency for WebEngine)
- `qtserialport` (module dependency used by Qt positioning plugin deployment)

### Additional Runtime/SDK Dependencies Pulled by Installer

- `qtbase`
- `qtdeclarative`
- `qtsvg`
- `qttools`
- `qttranslations`
- `d3dcompiler_47`
- `opengl32sw`

### Configure/Build Expectations

- CMake must be pointed at the Path A custom Qt install prefix (built with `-webengine-proprietary-codecs`):
  - `-DCMAKE_PREFIX_PATH="C:/Qt/6.8.2-custom-codecs"`
- Canonical configure/build commands:
  - `cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.8.2-custom-codecs"`
  - `cmake --build build --config Debug`

### Maintenance Notes

- If `find_package(Qt6 ...)` fails, verify all required Qt modules above are installed for the same Qt version and toolchain.
- Keep Qt version/toolchain consistent across local machines and CI to avoid ABI and package-resolution issues.

## Architecture Guidelines

- Organize by responsibility:
  - `src/app/` application entry and lifecycle
  - `src/ui/` windowing, tabs, navigation controls
  - `src/core/` shared utilities, config, logging
  - `src/browser/` engine wrapper and page/session abstractions
  - `src/network/` request/response policies and future filtering hooks
  - `asm/` optional architecture-specific optimized routines (x64)
  - `web/` TS/JS helper code for browser-side integration and tooling
  - `config/` JSON defaults and JSON schema files
- Prefer explicit interfaces for components likely to change (engine, filtering, storage).
- Avoid global mutable state.
- Use RAII, smart pointers, and deterministic resource cleanup.

## Platform Guardrails

- Keep `src/app/`, `src/ui/`, `src/browser/`, `src/network/`, and `src/core/` platform-neutral.
- If platform adapters are later required, place them in dedicated platform directories behind interfaces.
- Reject changes that directly couple product UX to a single operating system.

## Assembly Usage Policy

- Use Assembly only for isolated, benchmarked hot spots.
- Keep a portable C++ fallback for each Assembly routine.
- Document calling convention, register assumptions, and alignment requirements.
- Keep Assembly modules small and self-contained.

## TS/JS Usage Policy

- Use TS/JS only where browser/web-context behavior is required.
- Keep TS/JS modules independent from native core logic.
- Prefer TypeScript for maintainability; compile to minimal JS output.

## JSON Policy

- Keep app settings in human-readable JSON.
- Add/maintain JSON schema for any user-editable config.
- Validate config at load with safe defaults for missing/invalid fields.

## Performance & Memory Requirements

- Target a responsive cold start and low idle memory footprint.
- Avoid unnecessary allocations in hot paths.
- Reuse objects where practical.
- Use move semantics and pass-by-reference where appropriate.
- Be careful with thread creation; avoid background work without clear value.

## Tabs & Navigation (MVP Expectations)

- Support multiple tabs with create/close/switch flows.
- Maintain independent navigation state per tab.
- Keep tab management logic separate from rendering engine logic.
- Handle invalid URLs and load failures gracefully.
- Use a trailing `+` tab affordance for new tab creation (not toolbar new-tab buttons).
- Support tab closing through tab close controls and middle-mouse click on tab headers.

## UX Direction (Ghost Identity)

- Base interaction model on familiar Chromium patterns (discoverable tabs, predictable navigation flow).
- Integrate selected Firefox-like qualities where useful (clean information density, clear privacy/status cues).
- Maintain a unique Ghost identity through restrained styling, not novelty-for-novelty UX.
- Prefer subtle differentiation: layout rhythm, icon language, spacing, and motion restraint.
- Do not clone Chrome or Firefox UI assets, labels, or exact visual hierarchy.
- When ambiguous, choose familiarity first, then add one lightweight Ghost-specific refinement.
- Ghost UX theme is **dark mode only**.
- Base visual language on **VS Code Dark Modern** tokens and contrast behavior.

## Theming Rules (Dark Modern)

- Do not implement light theme, auto theme switching, or user theme toggles unless explicitly requested.
- Keep browser chrome (tabs, toolbar, address bar, menus, status UI) in Dark Modern styling.
- Use high-contrast readable text and restrained accent usage consistent with Dark Modern.
- Any new UI component must ship with Dark Modern styling by default.

## UX Constraints

- Keep MVP UI simple; avoid heavy customization systems.
- Minimize chrome around content to preserve a lightweight feel.
- New UX elements must justify cost in memory, startup, or complexity.
- Prioritize usability and speed over visual experimentation.
- Never trade away core privacy/security guarantees for cosmetic UX features.
- Use a custom draggable top chrome pattern where tabs occupy the title region and preserve minimize/maximize/close window controls.
- Provide a hamburger app menu in the navigation bar with core actions (tab actions, navigation actions, options/settings, diagnostics, exit).

## Ad-Blocking (Future Addition)

Do not implement full ad-blocking unless asked.
When adding related code now, only create **extension points**:

- request interception hooks
- domain/URL match pipeline interfaces
- pluggable rule provider abstraction

Keep these hooks lightweight and disabled by default.

## VPN Support (Future Addition)

Do not implement full VPN functionality unless asked.
When adding related code now, only create integration boundaries and interfaces:

- provider-agnostic VPN adapter interface
- WireGuard-based provider support path
- explicit compatibility target for Proton VPN, OpenVPN, and common WireGuard-compatible VPN providers
- connection state + error reporting interfaces for UI status

Security/privacy constraints for future VPN work:

- never log sensitive VPN connection data (keys, tokens, private endpoints)
- default to secure transport and safe failure behavior on tunnel errors
- keep provider-specific logic isolated from core browsing logic

## Coding Style

- Use modern C++ (C++20 where available).
- Prefer clear names over short names.
- Keep functions focused and small.
- Do not add comments that restate obvious code.
- Add comments only for non-obvious decisions, invariants, or constraints.
- Follow existing style in the file being edited.

## Safety & Reliability

- Validate all external inputs (URLs, file paths, settings values).
- Fail gracefully with useful error messages.
- Avoid crashes from null pointers, race conditions, or invalid state transitions.
- Write defensive checks around engine integration boundaries.
- Prefer secure failure modes (deny-by-default on invalid or unknown permission states).

## Build & Dependency Rules

- Keep build configuration simple and reproducible.
- Prefer standard library features before adding third-party libraries.
- Any new dependency must include a brief justification in docs/PR notes.

## Changelog Policy

- Maintain a professional versioned changelog in `CHANGELOG.md`.
- Every implemented change must be recorded under `Unreleased` in the same work session.
- On release, move `Unreleased` items into a new version section with date (`YYYY-MM-DD`).
- Use Keep a Changelog categories where applicable: `Added`, `Changed`, `Fixed`, `Removed`, `Security`, `Performance`, `Deprecated`.
- Keep entries concise, user-facing, and auditable.

## Testing & Validation

- Add or update tests when behavior changes.
- Prioritize tests for:
  - tab lifecycle
  - tab close semantics (including middle-click behavior where testable at unit/integration level)
  - navigation state
  - URL parsing/validation
  - webpage load speed benchmarking against maintained URL lists of known heavy sites
  - memory-sensitive logic
- If no test framework exists yet, keep code testable and add validation steps in notes.

### Performance Benchmarking Notes

- Keep a curated URL-list benchmark input under `config/` for repeatable load-speed testing.
- Include known heavy real-world websites (e.g., Gmail, YouTube, Spotify, Facebook, X) and update list over time.
- Prefer two-pass reporting (cold-ish and warm-cache) to identify caching and profile-level optimization opportunities.
- Include optional DNS warmup A/B runs (`--no-dns-warmup` vs `--dns-warmup`) to quantify first-load name-resolution effects.
- Keep DNS warmup lightweight and asynchronous in app startup paths to avoid blocking first paint.

## Non-Goals (unless explicitly requested)

- Sync/accounts/cloud features
- Extension marketplace
- Heavy customization/theming systems
- Complex background services

## How Copilot Should Respond in This Repo

- Be concise and implementation-focused.
- Propose the smallest viable change that satisfies the request.
- Call out tradeoffs when introducing performance or architecture impact.
- If requirements are ambiguous, choose the simplest interpretation aligned with this document.
