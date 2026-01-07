<div align="center">
  <img src="https://github.com/user-attachments/assets/e8aceb6b-2534-4aaf-a757-020b654aa285" alt="Logo" width="120">
</div>

# DownloadIntegrator

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

- Download the latest release from [Releases](../../releases), unzip, run `DownloadIntegrator.exe`

## Development & Build (Windows)

### Prerequisites

- Visual Studio 2022 (Desktop development with C++)
- CMake ≥ 3.25
- Qt 6.6.3+ (MSVC x64), e.g. `C:\Qt\6.10.0\msvc2022_64`
- vcpkg (set `VCPKG_ROOT`)

### Environment (PowerShell)

```powershell
$env:VCPKG_ROOT = "C:\vcpkg"
$env:CMAKE_PREFIX_PATH = "C:\Qt\6.10.0\msvc2022_64"
```

### Configure & Build

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 \
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET="x64-windows-static"

cmake --build build --config Release
```

Artifact: `build\Release\DownloadIntegrator.exe`

Optional with CMakePresets:

```powershell
cmake --list-presets
cmake --preset <configure-preset>
cmake --build --preset <build-preset>
```

## Security & Privacy

- Windows Defender may flag a false positive (e.g. `Win32/Wacapew.C!ml`); the app is safe.
- No personal data collection; network requests only for search and downloads; config stored locally.

## License & Feedback

- License: MIT, see [LICENSE](LICENSE)
- Feedback: open [Issues](../../issues) on GitHub
