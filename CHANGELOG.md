# Ghost Browser — Changelog

All notable changes to this project will be documented in this file.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

---

## [Unreleased]

### Added
- Protection diagnostics in `ghost://settings` now show recent blocked requests and HTTPS upgrades
- Ghost dashboard now starts the news and weather work with a live weather card and briefing links on `ghost://newtab`
- Site info button in the navigation bar now opens a native per-site permissions popup for notifications, location, camera, and microphone
- Middle-clicking a tab now closes it, and middle-clicking the address bar now clears it and focuses the field for immediate input
- Ghost now opens Qt WebEngine Developer Tools for the active tab from `F12`, `Ctrl+Shift+I`, or the main browser menu, with docked left/right/bottom and detached-window modes; placement, open/closed state, detached window position and size, and dock widget height are all fully persisted across sessions in `devtools-state.json`
- `ghost://settings` can now import a Ghost settings JSON file from disk; bookmark import from external sources remains pending, but Ghost now has a real local bookmark storage and management foundation
- Get Started now wires a real local profile name for the dashboard and real `Customize new tab page` toggles for the weather, shortcuts, briefing, and focus modules
- Get Started now opens Windows Default Apps from the `Ghost is your default browser` row so users can hand off default-browser assignment to the OS settings surface
- Get Started now also reports whether Ghost currently owns the Windows HTTP and HTTPS associations for the active profile
- `ghost://settings > Bookmarks` now provides a real bookmark management surface backed by persistent local storage and mirrored into the bookmarks toolbar
- Bookmarks can now be imported from standard HTML/JSON bookmark exports and exported back out from `ghost://settings > Bookmarks`
- The main browser menu now supports bookmarking the current page, renaming the current page bookmark, removing it, and opening bookmark import/export without going through settings first

### Changed
- Bumped Ghost Browser app version to `0.1.2`
- `Get Started > Default Browser Check` row now shows a live coloured status badge (green "Default" / red "Not Default") and accurate subtext; the old static heading "Ghost is your default browser" has been replaced with the neutral label "Default Browser Check" so the wording is never wrong regardless of actual state
- `Get Started > Import bookmarks and settings` no longer carries an orange "Settings" status tag
- `protection.blockFingerprinting` now applies concrete mitigations by standardizing client hints, hardware and WebGL identifiers, and disabling the Battery API on newly loaded pages
- Hardware acceleration now applies Chromium GPU flags at process startup from persisted settings; changing it at runtime now prompts that a restart is required for full effect
- Hovered link reveals now use a Chromium-style bottom status bubble instead of the previous full-width status surface
- Status bubble visuals now use a more transparent surface and Chromium-like cursor avoidance that shifts upward before flipping corners
- Heavy external pages now show a neutral grey loading surface and fade-out loading curtain instead of a white blind flash while content initializes
- The bookmarks toolbar no longer ships hardcoded Home, Settings, and Codec Test links; it now reflects the persisted bookmark list managed from `ghost://settings`
- The dashboard profile chip now uses the bundled default profile placeholder PNG while user-defined profile avatars remain a future feature
- The Get Started bookmark import row now points users to the dedicated Bookmarks tab for bookmark import/export while keeping settings import separate

### Fixed
- Removed the broken configurable status overlay path and its related settings from the UI, defaults, schema, and runtime wiring
- Qt/VS Code C++ workspace configuration now resolves project and Qt headers correctly instead of reporting false IntelliSense include failures
- Maximize/Restore button now reliably restores the window to its exact pre-maximize size and position; previously `showNormal()` on a frameless window could leave the window at the maximized dimensions because the OS had no standard NC region to track the restore rect — fixed by explicitly capturing the geometry in `m_preMaximizeGeometry` before maximizing and restoring it explicitly on both button click and drag-unmaximize paths; `saveWindowPlacement` and startup restore are also updated to use this value
- Windows 11 Snap Layouts and Arrange now work with the custom frameless window by restoring the native maximize/minimize/sizing style bits on the HWND and returning `HTMAXBUTTON` when the cursor is over the custom maximize button
- `appearance.fontSize` now visibly affects Ghost internal pages such as `ghost://settings` instead of only touching Chromium defaults behind the scenes
- Window size and position now persist across sessions again, and startup no longer collapses back to the bad `640x480` fallback size caused by pre-show geometry saves during initialization
- Developer Tools placement, open state, and detached window geometry now persist through real settings storage, including dock moves and detached-window repositioning instead of only surviving within the current run
- Settings import and bookmark import/export dialogs now parent correctly to the active Ghost window instead of potentially opening behind the frameless shell, and the Windows Default Apps action now launches through the native shell API for more reliable `ms-settings:defaultapps` handoff
- `ghost://settings` now normalizes older or partially populated settings payloads before rendering permission sections, preventing the settings bridge from crashing on missing `content.siteSettings` fields and breaking later actions like Import and Open Defaults

---

## [0.1.1] - 2026-03-07

### Added
- Downloads settings phase: users can now choose the default download folder directly from `ghost://settings`
- Privacy/settings actions: `Clear browsing data` and `Reset settings` are now wired to the native backend from `ghost://settings`
- Content setting: `View YouTube Shorts as normal videos` rewrites `/shorts/<id>` URLs to standard YouTube watch pages
- Site Settings phase: `ghost://settings > Content > Site settings` now wires default controls for JavaScript, pop-ups, notifications, location, camera, and microphone
- Protection settings phase: tracker/ad blocking levels, navigation-focused HTTPS upgrade, and third-party script blocking now execute in the request interceptor
- Protection safe-browsing phase: requests to curated malicious/phishing test hosts and risky download types are now blocked when `Safe Browsing` is enabled
- Theme phase: `Same as system` now follows the OS color scheme in both the native shell and `ghost://settings`
- Privacy phase: `Block third-party cookies` is now enforced through the Qt WebEngine cookie filter hook
- System settings now include a direct link to the internal codec test page
- Chromium GPU flags (`--enable-gpu-rasterization`, `--enable-zero-copy`, `--ignore-gpu-blocklist`, `BackForwardCache`) for faster page compositing and instant back/forward navigation
- DNS prefetching enabled on all web views to reduce cross-origin lookup latency on media-heavy sites
- Smooth scroll animation enabled by default
- Persistent browsing history via `HistoryManager` — records URL, title, and timestamp for every page load, saved to `history.json` (capped at 10,000 entries)
- Full History view accessible from the top-bar — entries grouped by relative time ("Just now", "A few hours ago", "Earlier today", "Yesterday", "X days ago", "Last week", etc.)
- History search: real-time filtering by title or URL
- Clear history by time range: last hour, 24 hours, 7 days, 4 weeks, or all time
- Per-entry delete button (✕) on hover in history list
- `HistoryManager` exposed via QWebChannel (`ghostHistory`) with `Q_INVOKABLE` methods: `getHistoryJson()`, `clearAll()`, `clearByAge()`, `deleteEntry()`
- `LocalStorageEnabled` and `JavascriptEnabled` explicitly set on every web view

### Changed
- Browser tabs are now wider in the custom title bar for better readability
- Closing the final remaining tab now closes the browser window
- The hamburger menu button no longer shows the extra toolbutton popup arrow
- HTTP disk cache bumped from 256 MB to 512 MB for better media-site performance
- Request interceptor now caches DNT/HTTPS-only settings instead of re-reading per request
- "Clear browsing data" now also clears persistent browsing history
- Cookie loading now retries during async startup so existing cookies are less likely to appear empty until sites are revisited

---

## [0.1.0] - 2026-03-06

### Added
- Project restart with Qt 6.9+ target (2026-03-06)
- Initial CMake project scaffold with Debug + Release build parity
- copilot-instructions.md updated for Qt 6.9+ baseline
- CHANGELOG.md and ISSUES.md tracking files
- Source directory structure: `src/app/`, `src/ui/`, `src/core/`, `src/browser/`, `src/network/`
- Minimal MainWindow and application entry point
- Output executable: "Ghost Browser"
- Feather Icons (MIT, SVG) selected as UI icon set — dark (`#E0E0E0`) and light (`#424242`) variants
- Icon-only toolbar with Back, Forward, Reload, Home, URL bar, Menu
- Tooltips on all icon-only buttons (tooltip policy added to copilot-instructions)
- Qt resource file `ghost_icons.qrc` compiling both theme icon sets
- Dark + Light mode support (dark is default); theme policy updated in copilot-instructions
- `iconPath()` helper for theme-aware icon loading
- `navigateBack()`, `navigateForward()`, `reloadPage()` slots with WebEngine history integration
- Frameless custom title bar: tabs in the chrome, window controls (minimize/maximize/close), drag-to-move, double-click maximize
- Custom navigation bar below title bar: Back, Forward, Reload, Home, URL bar, Menu — all icon-only with tooltips
- QStackedWidget-based tab content with QTabBar in title chrome (replaces QTabWidget)
- Ghost New Tab Page (`assets/pages/newtab.html`): dark-themed with clock, search bar, quick-links grid, Ghost branding
- Dark page background (`#1E1E2E`) set via `QWebEnginePage::setBackgroundColor` — no white flash on load or about:blank
- Window control SVG icons: minimize, maximize, restore (dark + light variants)
- Full Qt stylesheet for dark chrome: tabs, URL bar, nav buttons, window controls, close-hover red
- QtWebEngine rebuilt from source with proprietary codecs enabled (H.264, AAC, H.265/HEVC, MP3, etc.)
- Custom Qt WebEngine install at `C:\Qt\6.9.3-custom-codecs` with `-DFEATURE_webengine_proprietary_codecs=ON`
- Codec test page (`assets/pages/codec-test.html`) — accessible via `ghost://codec-test`
- Internal URL routing: `ghost://` scheme maps to `qrc:` internal pages
- `resolveInternalUrl()` helper for ghost:// → qrc: page resolution
- URL bar displays `ghost://codec-test` for internal pages instead of raw qrc: paths
- Post-build custom-codecs DLL overlay: copies WebEngine DLLs/resources from custom-codecs prefix after windeployqt to ensure proprietary codec support ships
- Extended audio codec test coverage: Opus (MP4), FLAC (MP4), ALAC, MP3 (MP4), DTS, DTS-HD, Dolby TrueHD, AMR-NB, AMR-WB
- Settings page (`ghost://settings`) — Brave-like layout with left sidebar + content panels (Get started, Appearance, Content, Protection, Privacy & Security, Search engine, Extensions, Autofill, Languages, Downloads, Accessibility, System, Reset)
- Hamburger menu (☰) with Settings, New Tab, History, Bookmarks, Downloads, Codec Test, Exit — styled dark with hover effects
- x64 Assembly (MASM) build support: `enable_language(ASM_MASM)` on Windows, GAS on Linux/macOS, with `asm/` directory structure and platform sub-dirs
- C++ ASM fallbacks in `src/core/asm_fallbacks.h` — every ASM routine has a portable C++ default
- Placeholder ASM routine: `ghost_memset_u32_asm` (SSE2 vectorized memset) in `asm/win/memset_u32.asm`
- TypeScript build pipeline: `web/` directory with `tsconfig.json`, `tsc` compilation target in CMake, output to `assets/js/`
- Settings bridge TypeScript module (`web/src/settings-bridge.ts`) — interface + load/save stubs for QWebChannel integration
- JSON config system: `config/defaults.json` (default settings) and `config/settings-schema.json` (JSON Schema validation)
- JSON config files deployed alongside exe via POST_BUILD copy
- Runtime settings backend via `SettingsManager` + `QWebChannel` bridge to `ghost://settings`
- Live appearance settings: theme, bookmarks bar visibility, font size, and page zoom now apply to the native shell and WebEngine views
- Browser request interceptor for privacy settings (`Do Not Track` header and HTTPS-only upgrade for HTTP navigations)
- Download handling phase: `Ask where to save each file` now controls save prompts via `QWebEngineDownloadRequest`
- Clear-on-exit browsing data cleanup via `QWebEngineProfile` cache/cookie/history clearing
- Explicit disk-backed `QWebEngineProfile` cache/storage configuration for better warm-load performance on heavy sites
- Settings bridge is now attached only to `ghost://settings` instead of every page, reducing overhead on normal browsing/media sites
- Content settings phase: live `Autoplay` and `Allow full-screen video` controls now apply to WebEngine settings

### Changed
- New tabs now use an explicit shared `QWebEngineProfile` page instance instead of relying on implicit page construction

### Fixed
- windeployqt was overwriting custom-codecs WebEngine DLLs with stock (no-codec) versions — all H.264/AAC reported unsupported at runtime
- Frameless window resize path now uses native Windows non-client hit testing (`WM_NCHITTEST`) instead of relying only on client-area mouse handling
- Heavy media/video sites no longer pay unnecessary per-page `QWebChannel` setup cost on every tab load
