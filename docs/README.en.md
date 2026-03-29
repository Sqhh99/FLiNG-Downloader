<div align="center">
  <img src="https://github.com/user-attachments/assets/e8aceb6b-2534-4aaf-a757-020b654aa285" alt="Logo" width="120">
</div>

# FLiNG Downloader

[简体中文](../README.md) | [English](./README.en.md) | [日本語](./README.ja.md)

---

## Features

- Modern UI with multiple themes (Light, Windows 11, Classic, Colorful)
- Search and suggestions backed by a local SQLite translation database
- Chinese and Japanese game titles can be remapped to the canonical English title used by FLiNG
- One-click download and categorized trainer management
- Real-time download progress, pause/resume, and downloaded item management
- Built-in languages: Chinese, English, Japanese
- Application update detection and installer download
- Independent translation database updates, with automatic selection between the bundled copy and the newer AppData override

## Screenshot

![Interface](../resources/interface.png)

## Requirements

- Windows 10 or later
- No extra runtime dependencies are required; required libraries are bundled with the app

## Quick Start

- Download the latest version from [Releases](../../releases), extract it, and run `FLiNG Downloader.exe`

## Development & Build (Windows)

Visual Studio 2022, CMake, Qt 6, and vcpkg are required. After configuring `VCPKG_ROOT` and `CMAKE_PREFIX_PATH`, run `build.cmd`.

Current main dependencies:

- Qt 6 (Core / Gui / Network / Qml / Quick / QuickControls2)
- SQLiteCpp (for reading `fling_translations.db`)
- OpenCV (for cover extraction)
- GoogleTest (unit / integration tests)
- Google Benchmark (performance benchmarks)

### `build.cmd` Usage

- **`build.cmd` or `build.cmd release`**: Build the Release version with Ninja by default (output: `build\ninja-release`)
- **`build.cmd debug`**: Build the Debug version (output: `build\ninja-debug`)
- **`build.cmd run` / `build.cmd run debug`**: Build and launch immediately
- **`build.cmd clean`**: Delete the entire `build` directory
- **`build.cmd rebuild`**: Remove the old build directory, reconfigure, and rebuild
- **`build.cmd i18n`**: Update translation source files (`.ts`) and generate translation files (`.qm`)
- **`build.cmd tests`**: Configure, build, and run GoogleTest targets
- **`build.cmd benchmark`**: Configure, build, and run Google Benchmark targets
- **`build.cmd benchmark --filter CoverExtractor/all_images`**: Run a specific benchmark case only

After building, the executables are generated in `build\ninja-release\` or `build\ninja-debug\`.

### Testing

- Unit and integration tests are located in `tests/`
- Default command:

```bat
build.cmd tests
```

- Current focus areas:
  - Search suggestions and canonical query remapping
  - Core download state and file handling
  - Application update, database update, and SQLite validation paths

### Benchmark

- Performance benchmarks are located in `tests/performance/`
- The repository currently includes `CoverExtractor` benchmarks, using samples from `tests/resources/fling_trainer_screenshot/`
- Default command:

```bat
build.cmd benchmark
```

- Run specific cases:

```bat
build.cmd benchmark --filter CoverExtractor/.*
```

## Translation Database

- The application uses an external SQLite database: `fling_translations.db`
- Release packages include a bundled copy under the app's `resources/` directory
- After a database update is downloaded, it is written to the AppData override location; the app compares the bundled and override versions and uses the newer valid copy
- A valid database must include:
  - `metadata.release_tag`
  - `metadata.schema_version`
  - `games.english`
  - `games.normalized_english`
  - `games.chinese_simplified`
  - `games.japanese`

## CI / Release

- `build.yml` runs build and test jobs
- `make-release.yml` produces installer and portable packages and includes `fling_translations.db`
- Both installer and portable packages contain the launcher, main app, and external resource directory

## Security & Privacy

- Windows Defender may report a false positive (for example `Win32/Wacapew.C!ml`); this is a false alarm.
- No personal data is collected; network access is used only for search, download, and update checks; local config and database override files are stored locally only.

## License & Feedback

- License: GNU AGPL v3, see [LICENSE](../LICENSE)
- Feedback: open [Issues](../../issues) on GitHub
