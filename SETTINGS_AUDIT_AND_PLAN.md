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
| General | `general.profileName` | Persists a local profile label and updates the dashboard profile chip on `ghost://newtab` live via the settings bridge. |
| General | `general.newTabModules.*` | Persists dashboard module visibility for weather, shortcuts, briefing, and focus, and applies those sections live on `ghost://newtab`. |
| Navigation | Bookmark management surface | Saved locally in `bookmarks.json`, managed from `ghost://settings > Bookmarks`, and mirrored into the native bookmarks toolbar. |
| Navigation | Bookmark import/export | Imports bookmarks from standard HTML exports or Ghost JSON, and exports the current bookmark store back to HTML or JSON from `ghost://settings > Bookmarks`. |
| Navigation | Current-page bookmark actions | The main browser menu can add, rename, and remove the bookmark for the page currently open in the active tab. |
| Appearance | `appearance.theme` | `dark`, `light`, and `system` are applied in native UI; `system` follows OS color scheme. |
| Appearance | `appearance.showBookmarksBar` | Toggles bookmarks bar visibility. |
| Appearance | `appearance.fontSize` | Applied to `QWebEngineSettings::DefaultFontSize` and mirrored into Ghost internal pages so `ghost://settings` and `ghost://newtab` visibly respond too. |
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
| Get started | `Ghost is your default browser` row | Opens Windows Default Apps via `ms-settings:defaultapps` so users can assign Ghost as the default browser from the OS settings surface, and reports current Windows HTTP/HTTPS ownership for the active profile. |
| Languages | `languages.spellCheck` | Enables/disables spell check on the profile. |
| Accessibility | `accessibility.highContrast` | Strengthens native Ghost chrome contrast and injects page-level contrast/focus overrides into loaded pages. |
| System | `system.hardwareAcceleration` | Applied to WebGL and accelerated canvas attributes. Process-level GPU flags applied at startup based on persisted setting; toggling at runtime notifies user a restart is required for full effect. |
| System | `system.proxyMode` | `system` uses OS proxy configuration and `none` forces a direct connection for app-level networking. |
| Site Settings | Notifications / Location / Camera / Microphone permissions | Default behavior, per-site exception lists, manual add, revoke flow, and current-site permission editor from the browser chrome (site-info button in nav bar). |
| Reset | `Reset settings` action | Restores JSON settings to defaults. |

### Partial

| Area | Setting | Gap |
| --- | --- | --- |
| System | `system.backgroundApps` | Persisted, but no background-process policy. |
| Get started | `Import bookmarks and settings` | Importing a Ghost settings JSON file is implemented. Bookmark import/export now exists too, but it intentionally lives under the dedicated `Bookmarks` tab instead of sharing this settings-specific row. |
| Get started | `Profile name and icon` | Profile name now works, but the avatar half is still only a default placeholder image until the real profile system lands. Future profile creation should start with `assets/placeholders/default_profile_placeholder.png` as the default avatar, allow end users to replace it, and constrain custom avatar inputs to square images from 256x256 up to 1024x1024. |

### UI Only / Not Implemented

| Area | Setting or Row | Gap |
| --- | --- | --- |
| Search engine | `Manage search engines` | No page or storage for custom engines. |
| Extensions | `Manage extensions` | No extension management subsystem. |
| Autofill | `Password Manager` | No password manager implementation. |
| Autofill | `Payment methods` | No storage or UI flow. |
| Autofill | `Addresses and more` | No storage or UI flow. |
| Languages | `Display language` | Static label only; no localization system. |
| Accessibility | `accessibility.caretBrowsing` | Persisted, but not applied to pages. |
| Privacy & Security | `Parental Controls` | No browser-level parental controls model yet. If added, prefer local device-side rules, allow/block lists, SafeSearch enforcement, schedule controls, and optional per-profile protections over invasive global age-verification style identity checks. The future profile system should also support locked child profiles so parents can create restricted profiles with local guardian-managed unlock. |
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
4. Define a privacy-preserving parental controls model at the browser/profile layer so Ghost can offer a credible alternative to global age-verification systems; prefer on-device rules, category controls, SafeSearch enforcement, schedules, clear guardian-managed overrides, and locked child profiles that hook directly into profile management.
5. Continue expanding the Ghost dashboard beyond the new first-pass weather card and briefing layout into a richer information surface, keeping a future real news feed backend or proxy in mind once the current lightweight approach has proven out.
6. Continue replacing static Get started rows with working OS/browser integration tasks; the default-browser row is now live, and the remaining rows should follow the same pattern.
7. Expand bookmark import/export beyond the current HTML/JSON flows if browser-specific formats or richer metadata migration become necessary.
8. Build the real profile system so new profiles start with the bundled default avatar placeholder, users can replace avatars later, and avatar validation enforces the 256x256 to 1024x1024 size window.

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

### Parental controls

1. Keep the design profile-local and privacy-preserving; do not require identity-style global age-verification flows to enable protections.
2. Start with enforceable browser-side controls such as site allow/block lists, SafeSearch/YouTube Restricted Mode enforcement, time windows, download restrictions, and private-browsing disablement for protected profiles.
3. Make parental controls hook into the future profile system so parents can create locked child profiles instead of relying only on global browser-wide restrictions.
4. Decide whether controls belong under `Privacy & Security`, a dedicated `Family` section, or per-profile settings before wiring UI.
5. Define how administrator/guardian unlock works locally so bypass requires deliberate consent without shipping sensitive personal data off-device.

### Profiles and avatars

1. New browser profiles should start with `assets/placeholders/default_profile_placeholder.png` as the default avatar.
2. End users should be able to replace the default avatar later from profile settings.
3. Accept custom avatar images only when they are square and between 256x256 and 1024x1024.
4. Keep the avatar model compatible with locked child profiles so parental-controls-protected profiles can still have a distinct default or guardian-approved avatar.

## Recommended Cleanup

1. Mark placeholder rows explicitly as `Coming Soon` until implemented, using a visible status tag rather than hiding them.
2. Keep quick utility links like `Settings > System > Codec test` available from the settings surface so diagnostic pages remain discoverable.
3. Keep the audit updated as each phase lands so the settings UI does not drift ahead of backend reality again.