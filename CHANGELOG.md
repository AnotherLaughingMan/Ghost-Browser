# Ghost Browser — Changelog

All notable changes to this project will be documented in this file.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

---

## [Unreleased]

### Added
- Protection diagnostics in `ghost://settings` now show recent blocked requests and HTTPS upgrades
- Ghost dashboard now starts the news and weather work with a live weather card and briefing links on `ghost://newtab`

### Changed
- Bumped Ghost Browser app version to `0.1.2`
- `protection.blockFingerprinting` now applies concrete mitigations by standardizing client hints, hardware and WebGL identifiers, and disabling the Battery API on newly loaded pages

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
