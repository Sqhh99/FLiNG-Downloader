# PR: Tint the Show All button and the settings update cards

- **Date:** 2026-10-03 / **Branch:** `fix/show-all-and-update-card-colors` → `main` / **Base commit:** `74c47ef` / **Related records:** [work log](../work-logs/2026-10-03-show-all-and-update-card-colors.md)

## 关联
There is no issue for this; the maintainer flagged it with screenshots. Two elements look out of place in the warm themes:
- **"Show All"** is a plain white button right next to the orange "Search".
- **The "Software Updates" and "Translation Database Updates" cards** are pure white on the cream settings dialog.

## 改了什么
- **"Show All"** is now a secondary button: the palette's hover tint with primary-colored text, the same as "Check for Updates", "Browse" and "Check Database Updates".
- **Both update cards** use the tint of the settings navigation column (`alternate_row`) instead of card white. They keep their border.

In Light, Dark, Sunset and Midnight the cards are now visibly tinted. In Ocean, Forest, Lavender, Rose and Mocha that palette's `alternate_row` equals the dialog background, so the cards blend in and only their border shows, as the sidebar already does there.

Two lines changed, in `crates/fling-ui/src/views/search_page.rs` and `crates/fling-ui/src/views/settings_panel.rs`.

## 怎么验证
- [x] `cargo test --workspace`
- [ ] 本地跑过相关界面 / 下载 / 搜索路径

`cargo fmt --all --check`, `cargo clippy --workspace --all-targets -- -D warnings` and `cargo test --workspace` exit 0 on the Windows toolchain.

The second box is unticked because the app was not used interactively. What was checked: the debug build was launched in the maintainer's Sunset theme, and the main screen and Settings › About were captured with `PrintWindow` (below). The other eight themes were not captured, because switching themes changes the real `settings.ini`; the per-theme note above is read from the palette values.

## 检查项
- [ ] 已阅读 [CONTRIBUTING.md](https://github.com/Sqhh99/FLiNG-Downloader/blob/main/CONTRIBUTING.md)
- [x] 界面改动附了截图（不适用可删）
- [x] 若改动了翻译库、i18n、模型或打包资源，已在上文写明
- [x] AI 使用披露：否 / 是（说明用在哪一部分）

**Screenshots (Sunset theme).** The "before" captures are the maintainer's.

| Before | After |
|--------|-------|
| ![Main screen before](https://github.com/Sqhh99/FLiNG-Downloader/raw/12d9e06ee2a38200cc4d748b8aac183f68f7d91f/docs/pull-requests/assets/2026-10-03-show-all-and-update-card-colors/before-main.png) | ![Main screen after](https://github.com/Sqhh99/FLiNG-Downloader/raw/12d9e06ee2a38200cc4d748b8aac183f68f7d91f/docs/pull-requests/assets/2026-10-03-show-all-and-update-card-colors/after-main.png) |
| ![About before](https://github.com/Sqhh99/FLiNG-Downloader/raw/12d9e06ee2a38200cc4d748b8aac183f68f7d91f/docs/pull-requests/assets/2026-10-03-show-all-and-update-card-colors/before-settings.png) | ![About after](https://github.com/Sqhh99/FLiNG-Downloader/raw/12d9e06ee2a38200cc4d748b8aac183f68f7d91f/docs/pull-requests/assets/2026-10-03-show-all-and-update-card-colors/after-settings.png) |

**Resources:** none changed. No translation database, i18n, model or packaging changes.

**CONTRIBUTING.md:** left for the maintainer to tick.

**AI disclosure:** yes. The change and the records were made with Claude Code, and verified by the checks and screenshots above.

**Template:** this record follows the Chinese template that `main` still has. PR #49 switches the template to English; if it merges first, the headings can be converted the same way as the PR #48 record.
