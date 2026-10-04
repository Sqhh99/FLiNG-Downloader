# PR: Rewrite FLiNG Downloader in Rust + GPUI

- **Date:** 2026-10-04 / **Branch:** `rewrite/rust-gpui` → `main` / **Base commit:** `6701ab6` / **Related records:** [work log](../work-logs/2026-10-03-rust-gpui-rewrite.md)

## 关联
The maintainer asked for a full rewrite in Rust and GPUI, with a clear structure, decoupled modules, and the frontend separated from the backend. The Qt build concentrated orchestration and UI glue in one 1.6k-line `Backend.cpp`. It also relied on global singletons, and its test seams were bolted onto `NetworkManager`.

## 改了什么
**For users**
- The same app on a native GPU-rendered UI (GPUI Kit):
  - search with CN/JA suggestions, results table and detail drawer (cover, versions, options);
  - downloads with pause, resume and retry;
  - downloaded library;
  - settings with 9 themes, 3 languages, download folder, update source, and app/database updates.
- Language changes apply instantly, and status messages are now localized; the Qt build showed backend messages in English even in Chinese mode.
- Icons are the Lucide set.
- **Animations:**
  - settings fade and rise in and out;
  - the detail drawer slides from the right;
  - the download list drops down;
  - tabs and settings panes cross-fade;
  - result and library rows rise in when a list appears or changes;
  - lists scroll smoothly with the mouse wheel.

  All of it is off when Windows' "Animation effects" setting is off.
- **Upgrades keep users' data.** The Rust build reads and writes the same paths and formats as v1.1.x: `settings.ini` (unknown keys preserved), `downloaded_modifiers.json`, the recent-list cache, the cover cache and the translation-database override.
- The installer keeps the same AppId and asset names, so v1.1.x's updater finds and installs it. On upgrade it removes the old `app\` folder.
- The package is one exe (ONNX Runtime is linked in) plus `models/`, `resources/` and the MSVC runtime. The exe is 41.7 MB with a size-optimized profile, about 9.6 MB once compressed.

**For developers**
- A Cargo workspace of 10 crates, with dependencies pointing one way: `fling-ui → fling-app → {site, download, update, cover, mapping, config} → {net, core}`.
- `fling-app` is the only frontend/backend contract (`Command` / `Event`). It runs on its own thread and tokio runtime, and no singletons remain.
- The parsers and business rules are ports of the C++ code, quirks included; the deliberate deviations are listed in the work log.
- `cargo xtask dist` builds the release folder, and `cargo xtask notices` regenerates the license table.
- CI runs fmt, clippy, test and dist. The release workflow is unchanged in its outputs.
- The Qt/QML/CMake/vcpkg tree, the C++ tests and the vendored YOLOs-CPP are removed, and the docs are rewritten for the Rust workflow.

## 怎么验证
- [x] `cargo test --workspace`
- [x] 本地跑过相关界面 / 下载 / 搜索路径

Also run: `cargo fmt --all --check` and `cargo clippy --workspace --all-targets -- -D warnings`, both clean, with 131 tests passing. The tests include:
- the real reqwest client against a local socket server;
- the parsers on pages saved from flingtrainer.com;
- the real cover model on 7 sample screenshots;
- the whole backend driven through `Command`/`Event` with a fake network.

Live headless runs against flingtrainer.com covered the recent list, English search, CN/JA title search, detail pages and cover extraction. `cargo xtask dist` was built, and the packaged exe started and loaded its model.

The second box is ticked on the maintainer's word. After several review rounds on search, suggestions, the detail drawer and settings, the maintainer reported on 2026-10-04 that they had completed testing of the app. The installer is compiled and smoke-tested only by the release workflow in CI, because Inno Setup is not installed locally. The interface animations were added after that testing round. Their final states were checked by screenshot, but the motion itself and smooth wheel scrolling still need the maintainer's eye.

## 检查项
- [x] 已阅读 CONTRIBUTING.md
- [x] 界面改动附了截图（不适用可删）
- [x] 若改动了翻译库、i18n、模型或打包资源，已在上文写明
- [x] AI 使用披露：否 / 是（说明用在哪一部分）

**On the screenshot:** the README's `resources/interface.png` is the new UI.

**i18n and packaging changes:**
- UI strings moved from Qt `.ts` files to `crates/fling-ui/locales/app.yml`. The Japanese strings the Qt build never translated, and Chinese texts for former English-only status messages, were filled in during the rewrite and should be reviewed.
- The ONNX model and the translation database are unchanged.
- The package layout lost `app\` and the launcher.

**AI disclosure:** yes. The rewrite (code, tests, CI, docs) was written with Claude Code. It was verified by the automated tests, live headless runs and screenshot checks listed above, and by the maintainer's manual UI review of search, drawer and settings.
