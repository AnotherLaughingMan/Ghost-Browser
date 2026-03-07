# Ghost Browser

> A fast, lightweight, privacy-first desktop web browser built with Qt 6 and Qt WebEngine.

Ghost is designed around one idea: your browser should work *for you*, not against you.
It starts fast, uses minimal RAM, blocks tracking by default, and never phones home.

---

## Features

- **Tabbed browsing** — multi-tab interface with a custom frameless title bar
- **Browsing history** — persistent local history with search, time-range filtering, and per-entry deletion
- **Cookie viewer** — inspect, search, and delete cookies grouped by domain
- **Privacy defaults** — Do Not Track, third-party cookie blocking, and HTTPS-only mode out of the box
- **Clear browsing data** — wipe history, cookies, and HTTP cache on demand or on exit
- **GPU-accelerated rendering** — hardware rasterisation and zero-copy compositing via Chromium flags
- **Settings page** — full in-browser settings UI at `ghost://settings` with no external dependencies
- **Custom request interceptor** — foundation for future ad/tracker blocking
- **Assembly hot paths** — SSE4.2 CRC32C URL hashing and PREFETCHT0 cache warming for history load

### Planned
- Built-in ad and tracker blocking (AdBlock filter engine)
- Bookmark manager
- Extension / userscript support
- Linux and macOS builds

---

## Requirements

| Dependency | Version |
|------------|---------|
| C++ compiler | MSVC 2022 (Windows), GCC 13+ / Clang 17+ (Linux/macOS) |
| CMake | 3.24+ |
| Qt | 6.9+ (`Qt6Widgets`, `Qt6WebEngineWidgets`, `Qt6WebChannel`) |
| MASM (ml64) | Windows x64 ASM builds only |
| Node.js / tsc | TypeScript 5+ (for rebuilding `assets/js/settings-bridge.js`) |

---

## Building

### 1. Install Qt 6.9+

Use [aqtinstall](https://github.com/miurahr/aqtinstall) or the official Qt Online Installer.

```bash
pip install aqtinstall
aqt install-qt windows desktop 6.9.3 win64_msvc2022_64 \
    -m qtwebengine qtwebchannel qtpositioning qtserialport
```

### 2. Configure

```bash
cmake -S . -B build -A x64 \
    -DCMAKE_PREFIX_PATH="C:/Qt/6.9.3/msvc2022_64"
```

If you built Qt with proprietary codec support, add that prefix first:

```bash
cmake -S . -B build -A x64 \
    -DCMAKE_PREFIX_PATH="C:/Qt/6.9.3-custom-codecs;C:/Qt/6.9.3/msvc2022_64"
```

### 3. Build

```bash
# Debug
cmake --build build --config Debug

# Release
cmake --build build --config Release
```

The binary is output as **`Ghost Browser.exe`** (Windows) inside `build/Debug/` or `build/Release/`.

---

## Project Structure

```
Ghost/
├── asm/              # x64 assembly hot paths (CRC32C, prefetch)
├── assets/
│   ├── icons/        # Feather Icons (MIT) — dark and light variants
│   ├── js/           # Compiled TypeScript bridge (settings/history/cookie UI)
│   └── pages/        # Internal browser pages (settings.html, newtab.html, …)
├── config/           # JSON defaults and schema
├── src/
│   ├── app/          # Entry point (main.cpp)
│   ├── browser/      # Request interceptor and engine abstractions
│   ├── core/         # HistoryManager, CookieManager, SettingsManager
│   ├── network/      # Future: network filtering hooks
│   └── ui/           # MainWindow — tabs, nav bar, title bar
└── web/
    └── src/          # TypeScript source for settings-bridge.ts
```

---

## Data & Privacy

Ghost stores all user data **locally** on your machine — no accounts, no telemetry, no sync:

| Data | Location |
|------|----------|
| Browsing history | `%APPDATA%\Ghost\Ghost Browser\history.json` |
| Settings | `%LOCALAPPDATA%\Ghost\Ghost Browser\settings.json` |
| Cookies, cache, IndexedDB | `%APPDATA%\Ghost\Ghost Browser\` |

Nothing is sent to any server. The only outbound network traffic is the web pages you visit.

---

## Third-Party Licenses

| Component | License |
|-----------|---------|
| [Qt 6](https://www.qt.io/) | LGPL v3 |
| [Chromium](https://chromium.googlesource.com/chromium/src/) (via QtWebEngine) | BSD 3-Clause |
| [Feather Icons](https://feathericons.com/) | MIT |

---

## Contributing

Contributions are welcome. Please read [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) before opening issues or pull requests.

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-feature`)
3. Make your changes, ensuring both Debug **and** Release builds pass
4. Open a pull request with a clear description of what changed and why

---

## License

Ghost Browser is released under the **GNU General Public License v3.0**.
See [LICENSE](LICENSE) for full terms.

&copy; 2026 Ghost Browser contributors
