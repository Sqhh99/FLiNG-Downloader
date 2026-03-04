<div align="center">
  <img src="https://github.com/user-attachments/assets/e8aceb6b-2534-4aaf-a757-020b654aa285" alt="Logo" width="120">
</div>

# FLiNG Downloader

A Qt-based trainer download manager with intelligent Chinese title mapping.

[简体中文](./README.md) | [English](./README.en.md) | [日本語](./README.ja.md)

---

## Features

- Modern UI with themes: Light, Windows 11, Classic, Colorful
- Search by Chinese game titles, auto-mapped to English
- One-click download and categorized management
- Built-in languages: Chinese, English, Japanese
- Real-time update detection

## Screenshot

![Interface](./resources/interface.png)

## Requirements

- Windows 10 or later
- No extra runtime dependencies; required libraries are statically linked

## Quick Start

- Download the latest release from [Releases](../../releases), unzip, run `FLiNG Downloader.exe`

## Development & Build (Windows)

Visual Studio 2022, CMake, Qt 6, and vcpkg are required. After configuring `VCPKG_ROOT` and `CMAKE_PREFIX_PATH`, simply run `build.cmd`.

### `build.cmd` Usage

- **`build.cmd` or `build.cmd release`**: Build Release version using Ninja by default (outputs to `build\ninja-release`)
- **`build.cmd debug`**: Build Debug version (outputs to `build\ninja-debug`)
- **`build.cmd run` / `build.cmd run debug`**: Build and run immediately
- **`build.cmd clean`**: Delete the entire `build` directory
- **`build.cmd rebuild`**: Clean directory, reconfigure, and build
- **`build.cmd i18n`**: Update translation source files (`.ts`) and generate translation files (`.qm`)

Artifacts will be located in the corresponding `build\ninja-release\` or `build\ninja-debug\` directories after building.

## Security & Privacy

- Windows Defender may flag a false positive (e.g. `Win32/Wacapew.C!ml`); the app is safe.
- No personal data collection; network requests only for search and downloads; config stored locally.

## License & Feedback

- License: GNU AGPL v3, see [LICENSE](../LICENSE)
- Feedback: open [Issues](../../issues) on GitHub
