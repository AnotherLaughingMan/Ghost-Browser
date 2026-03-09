# Settings Audit And Implementation Plan

## Status Legend

- Functional: persisted and enforced in runtime behavior.
- Partial: persisted and/or reflected in UI, but behavior is incomplete or does not fully match the setting name.
- UI only: visible in settings, but not wired to backend behavior.

## Audit Summary

### Functional

| Area | Setting | Notes |
| --- | --- | --- |
| General | `general.homePage` | Used by Home and by `specificPages` startup path. |
| General | `general.startupBehavior` | `specificPages` uses the configured home page, and `lastSession` restores saved tabs plus active tab index from session state. |
| General | `general.searchEngine` | Used to build search URLs. |
| Appearance | `appearance.theme` | `dark`, `light`, and `system` are applied in native UI; `system` follows OS color scheme. |
| Appearance | `appearance.showBookmarksBar` | Toggles bookmarks bar visibility. |
| Appearance | `appearance.fontSize` | Applied to `QWebEngineSettings::DefaultFontSize`. |
| Appearance | `appearance.zoomLevel` | Applied to each web view zoom factor. |
| Content | `content.autoplay` | Toggles `PlaybackRequiresUserGesture`. |
| Content | `content.fullScreenVideo` | Toggles full-screen support. |
| Content | `content.youtubeShortsAsNormalVideos` | Rewrites YouTube Shorts URLs to `/watch?v=`. |
| Content / Site Settings | `content.siteSettings.javascript` | Toggles JavaScript enablement. |
| Content / Site Settings | `content.siteSettings.popups` | Controls `JavascriptCanOpenWindows`. |
| Content / Site Settings | `content.siteSettings.notifications` | Applied through `featurePermissionRequested`. |
| Content / Site Settings | `content.siteSettings.location` | Applied through `featurePermissionRequested`. |
| Content / Site Settings | `content.siteSettings.camera` | Applied through `featurePermissionRequested`. |
| Content / Site Settings | `content.siteSettings.microphone` | Applied through `featurePermissionRequested`. |
| Privacy | `privacy.doNotTrack` | Sends `DNT: 1` header in request interceptor. |
| Privacy | `privacy.blockThirdPartyCookies` | Enforced through the Qt WebEngine cookie filter hook using the request `thirdParty` flag. |
| Privacy | `privacy.clearDataOnExit` | Changes persistent cookie policy and clears data on close. |
| Privacy | `privacy.httpsOnly` | Redirects HTTP to HTTPS for non-local hosts. |
| Protection | `protection.trackingLevel` | Blocks requests to a curated tracker/ad host list with separate `standard` and `aggressive` behavior tiers. |
| Protection | `protection.httpsUpgrade` | Upgrades HTTP main-frame and sub-frame navigations to HTTPS while remaining separate from strict HTTPS-Only mode. |
| Protection | `protection.blockFingerprinting` | Standardizes client hints plus hardware and WebGL identifiers and disables the Battery API on newly loaded pages. |
| Protection | `protection.blockScripts` | Blocks third-party script requests in the request interceptor. |
| Protection | `protection.safeBrowsing` | Blocks requests to a curated malicious/phishing test-host denylist and blocks risky executable/script download types. |
| Protection | Diagnostics surface | `ghost://settings > Protection` now shows recent blocked requests and HTTPS upgrades for basic explainability. |
| Privacy | `Clear browsing data` action | Clears cookies, cache, visited links, and app history. |
| Downloads | `downloads.defaultPath` | Used as the default target directory. |
| Downloads | `downloads.askWhereToSave` | Prompts for a file path before accepting download. |
| Downloads | `Choose download path` action | Opens folder picker and persists result. |
| Languages | `languages.spellCheck` | Enables/disables spell check on the profile. |
| Accessibility | `accessibility.highContrast` | Strengthens native Ghost chrome contrast and injects page-level contrast/focus overrides into loaded pages. |
| System | `system.hardwareAcceleration` | Applied to WebGL and accelerated canvas attributes. |
| System | `system.proxyMode` | `system` uses OS proxy configuration and `none` forces a direct connection for app-level networking. |
| Reset | `Reset settings` action | Restores JSON settings to defaults. |

### Partial

| Area | Setting | Gap |
| --- | --- | --- |
| System | `system.hardwareAcceleration` | Applied at page settings level, but does not fully control all process-level Chromium/GPU flags after startup. |
| Site Settings | Notifications / Location / Camera / Microphone permissions | Default behavior, per-site exception lists, manual add, and revoke flow now work, but there is still no current-site editor from the browser chrome. |

### UI Only / Not Implemented

| Area | Setting or Row | Gap |
| --- | --- | --- |
| Get started | `Profile name and icon` | No handler or storage. |
| Get started | `Import bookmarks and settings` | No handler or import flow. |
| Get started | `Ghost is your default browser` | Informational row only; no OS default-browser integration. |
| Get started | `Customize new tab page` | No handler or customization page. |
| Search engine | `Manage search engines` | No page or storage for custom engines. |
| Extensions | `Manage extensions` | No extension management subsystem. |
| Autofill | `Password Manager` | No password manager implementation. |
| Autofill | `Payment methods` | No storage or UI flow. |
| Autofill | `Addresses and more` | No storage or UI flow. |
| Languages | `Display language` | Static label only; no localization system. |
| Accessibility | `accessibility.caretBrowsing` | Persisted, but not applied to pages. |
| System | `system.backgroundApps` | Persisted, but no background-process policy. |
| System | `Proxy settings` | Manual proxy host and port entry are still not implemented. |

## Recommended Implementation Order

### Phase 1: Finish Existing Real Settings

1. Tighten `lastSession` scope if needed by adding crash recovery and multi-window restore semantics.
2. Continue expanding the Protection backend beyond the current tracker/ad host list and navigation-only HTTPS upgrade.
3. Review whether the home-page selector needs more than `Ghost dashboard` and `Blank page` before adding more destinations.

### Phase 2: Wire Protection Settings

1. Implement `protection.blockFingerprinting` with a concrete set of mitigations, or remove/rename it until real behavior exists.
1. Decide whether `trackingLevel` should move from a curated host list to filter-list backed rules.
2. Expand `safeBrowsing` beyond the current scoped denylist/download filter if a real reputation source is introduced.
3. Keep expanding the new diagnostics surface after the current per-site and per-category filtering pass if the product wants export or longer-lived inspection.

### Phase 3: Wire Accessibility And System

1. Implement `accessibility.caretBrowsing` if Qt WebEngine supports it, otherwise remove the setting until it is supportable.
2. Implement `system.backgroundApps` or remove the UI until there is a background task model.
3. Expand proxy support from `system` / `none` into real manual host, port, and bypass-list configuration.

### Phase 4: Build Missing Product Surfaces

1. Add a real search-engine management model for predefined and custom engines.
2. Define whether Ghost will support extensions; if yes, build an extension management surface, if not, remove the placeholder.
3. Define autofill/password storage architecture before wiring those rows.
4. Continue expanding the Ghost dashboard beyond the new first-pass weather card and briefing layout into a richer information surface, keeping a future real news feed backend or proxy in mind once the current lightweight approach has proven out.
5. Replace static Get started rows with working OS/browser integration tasks.

## Suggested Technical Tasks

### Session restore

1. Extend the current tab-session restore to cover crash recovery if desired.
2. Decide whether multi-window restore is in scope.
3. Add session versioning if the stored format is expected to evolve.

### Third-party cookie blocking

1. Add tests covering first-party vs third-party cookie cases.
2. Decide how existing third-party cookies should be handled when the setting is enabled.
3. Add diagnostics if users need visibility into blocked third-party cookie attempts.

### Protection settings

1. Define exact behavior for `standard`, `aggressive`, and `disabled` before coding.
2. Keep names aligned with actual behavior; do not over-claim privacy/security features in UI copy.
3. Add telemetry/logging hooks only if the product wants explainability for blocked resources.

## Recommended Cleanup

1. Mark placeholder rows explicitly as `Coming Soon` until implemented, using a visible status tag rather than hiding them.
2. Keep quick utility links like `Settings > System > Codec test` available from the settings surface so diagnostic pages remain discoverable.
3. Keep the audit updated as each phase lands so the settings UI does not drift ahead of backend reality again.