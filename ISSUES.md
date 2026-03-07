# Ghost Browser — Issues Log

Track known issues, blockers, and investigation notes here.

---

## Open Issues

### ISSUE-001: Qt 6.9 installation
- **Status:** Closed
- **Priority:** Blocker
- **Opened:** 2026-03-06
- **Closed:** 2026-03-06
- **Description:** Qt 6.9+ must be installed and configured before the project can build.
- **Resolution:** Qt 6.9.3 installed via aqtinstall at `C:\Qt\6.9.3\msvc2022_64`. Both Debug and Release build successfully.

---

### ISSUE-002: QtWebEngine proprietary codecs
- **Status:** Closed
- **Priority:** Blocker
- **Opened:** 2026-03-06
- **Closed:** 2026-03-06
- **Description:** Pre-built Qt 6.9.3 from aqtinstall does not include proprietary codec support (H.264, AAC, etc.). QtWebEngine must be rebuilt from source with `-DFEATURE_webengine_proprietary_codecs=ON`.
- **Resolution:** Cloned qtwebengine v6.9.3 from `code.qt.io`, installed ATL build tools and html5lib dependency, built both RelWithDebInfo and Debug configurations (~43,764 Chromium targets). Installed custom WebEngine to `C:\Qt\6.9.3-custom-codecs`. Ghost now builds against both prefixes. Codec test page added at `ghost://codec-test`.

---

### ISSUE-003: windeployqt overwrites custom-codecs DLLs
- **Status:** Closed
- **Priority:** Blocker
- **Opened:** 2026-03-06
- **Closed:** 2026-03-06
- **Description:** `windeployqt` runs from the stock Qt prefix and copies stock WebEngine DLLs (199,584,992 bytes, no proprietary codecs) over the custom-codecs versions. All H.264/AAC/MP3 codec tests showed "No" at runtime despite building against the custom prefix.
- **Resolution:** Added a second POST_BUILD phase in CMakeLists.txt that runs after windeployqt and overwrites WebEngine DLLs + resources with the custom-codecs build (200,466,944 bytes). Verified deployed DLL matches custom build. All proprietary codecs now report supported.

---

### ISSUE-004: Frameless window could not be resized
- **Status:** Monitoring
- **Priority:** High
- **Opened:** 2026-03-06
- **Description:** The frameless Ghost window supported dragging but not reliable edge/corner resizing at runtime.
- **Mitigation:** Added native Windows `WM_NCHITTEST` handling in MainWindow so resize hit testing is delegated to the OS non-client path. Debug and Release builds pass. Runtime verification still required on the latest build.

---

### ISSUE-005: Video-heavy sites load slowly
- **Status:** Mitigated
- **Priority:** High
- **Opened:** 2026-03-06
- **Description:** YouTube and other video-heavy sites were slower than expected to initialize and begin rendering.
- **Mitigation:** Moved settings `QWebChannel` attachment to the internal settings page only, switched tabs to an explicit shared `QWebEngineProfile`, and configured persistent disk cache/storage paths. Also enabled content settings for autoplay and full-screen video. Debug and Release builds pass. Runtime verification required on the latest build.

---

## Closed Issues

### ISSUE-001
Resolved — see above.

### ISSUE-002
Resolved — see above.

### ISSUE-003
Resolved — see above.

### ISSUE-004
Monitoring — see above.

### ISSUE-005
Mitigated — see above.
