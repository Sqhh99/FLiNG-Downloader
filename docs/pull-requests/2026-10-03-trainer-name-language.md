# PR: Show trainer names in Chinese or Japanese

- **Date:** 2026-10-03 / **Branch:** `feat/trainer-name-language` → `main` / **Base commit:** `372491d` / **Related records:** [work log](../work-logs/2026-10-03-trainer-name-language.md)

## 关联
There is no issue for this. The maintainer asked for it directly: every trainer name is English, in search results, the detail drawer, the library and the names of downloaded files. That is awkward for Chinese and Japanese users. The bundled translation database already has a Chinese and a Japanese title for every game it covers, but until now it was used only to translate search input.

The maintainer also asked for two additions to this PR: the database-updater repo as a submodule, and the new database release.

## 改了什么
**For users**
- **New setting.** Settings › Language has a **Trainer names** option: Follow interface language (default), English, 简体中文, 日本語.
- **Two-line names.** In search results, the library and the detail drawer header, a game the database knows shows its translated title, with the English title on a smaller second line. For example, "Elden Ring Trainer" becomes 艾尔登法环 over "Elden Ring". Both lines fit the existing 40 px table rows.
- **File names.** New downloads are named after the translated title, e.g. `艾尔登法环 (Elden Ring)_v1.0.zip`. Existing files are not renamed.
- **When a name stays as it was:** when the game isn't in the database, or the database has no distinct title in that language. Many Japanese rows just repeat the English title.
- **English mode** shows exactly what it showed before.
- **Instant updates.** Switching the setting, switching the interface language while on "follow", or installing a database update re-labels the visible lists right away.
- **Upgrades.** The default is "follow", so after upgrading, users with a Chinese interface see Chinese names immediately.
- **Bundled translation database updated** from v0.0.3 (1,157 games) to [v0.0.5](https://github.com/Sqhh99/game-mappings-updater/releases/tag/v0.0.5) (1,185 games). The new rows include recent trainers such as *Ace Combat 8*, *The Witcher 3: Wild Hunt – Remastered*, *Onimusha 2* and *Trails in the Sky 2nd Chapter*.

**For developers**
- `fling-mapping`: `GameMappings::localized_title` looks up an English title and returns the Chinese or Japanese one. Matching is exact after `normalize_lookup_text`, with no fuzzy fallback, so a remaster never borrows the base game's title.
- `fling-app`: `names::label` produces `TrainerLabel { title, subtitle }`. It decodes site entities, drops a trailing "Trainer", then looks the title up. `one_line()` gives the file name.
  - The backend fills `display_name` and `display_subtitle` on results, the selection and the library.
  - It re-labels when the setting, the UI language or the database changes.
  - It passes `one_line()` to the queue as the download file stem.
- `ModifierInfo::name` and `DownloadedModifier::name` stay the site's English title, because cover ids, the cover cache, `downloaded_modifiers.json` (Qt compatibility) and search relevance key on them. The display fields on `DownloadedModifier` are `#[serde(skip)]`, so the JSON format is unchanged.
- **New command and setting:** `Command::SetTrainerNameLanguage`, with `SettingsSnapshot::trainer_name_language`. The new `settings.ini` key is `trainerNameLanguage` (`auto` / `en` / `zh` / `ja`); a Qt-era file without it reads as `auto`.
- `DownloadQueue::enqueue` takes a `file_stem`.
- **Name sort** orders by the displayed title.
- **New debug hook:** `FLING_DEBUG_OPEN=settings-language`.
- **New submodule:** [`game-mappings-updater`](https://github.com/Sqhh99/game-mappings-updater), the Python/uv tool that builds and releases the database.
  - It lives at `tools/game-mappings-updater`, pinned to `fb94eeb`, the v0.0.5 release merge. Fetch it with `git submodule update --init`.
  - Its old `.gitignore` entry is removed.
  - Nothing in the Rust build or CI uses it, so the workflows still check out without submodules.

**Affected crates:** `fling-core`, `fling-mapping`, `fling-config`, `fling-download`, `fling-app`, `fling-ui`.

## 怎么验证
- [x] `cargo test --workspace`
- [ ] 本地跑过相关界面 / 下载 / 搜索路径

All of these ran on the Windows toolchain and exited 0:
- `cargo test --workspace`. The new tests cover:
  - the reverse lookup, including case, punctuation, curly apostrophes, and empty or repeated titles;
  - label formatting, including entity decoding and suffix stripping;
  - settings persistence;
  - a CJK download file name with an English library name;
  - an end-to-end backend test: default Chinese names, switching to English, back to "follow" plus a Japanese UI, a localized file name, and a library JSON free of display names.
- `cargo clippy --workspace --all-targets -- -D warnings` and `cargo fmt --all --check`.

**Against the real bundled database:** with v0.0.3, 7 of the 15 trainers on the saved homepage fixture got a Chinese title, and the 8 misses were not in the database. With v0.0.5 all 15 do.

**The new database file** is byte-identical (SHA-256 `220d43ff…cfa3aa4`) to the v0.0.5 release asset. It has `schema_version` 1, no rows missing a Chinese or Japanese title, and `PRAGMA integrity_check` returns `ok`. `cargo test --workspace` passes with it.

**The second box is unticked.** The UI was run and checked by screenshot, below. To show translated names without changing the maintainer's real `settings.ini`, a temporary, uncommitted patch made the name setting read an env var; it was reverted and rebuilt afterwards. Three checks remain for the maintainer before merge:
- switching the setting interactively;
- downloading a trainer and checking the file name;
- looking at existing library entries.

## 检查项
- [ ] 已阅读 [CONTRIBUTING.md](https://github.com/Sqhh99/FLiNG-Downloader/blob/main/CONTRIBUTING.md)
- [x] 界面改动附了截图（不适用可删）
- [x] 若改动了翻译库、i18n、模型或打包资源，已在上文写明
- [x] AI 使用披露：否 / 是（说明用在哪一部分）

**Screenshots**

The new setting (English UI):

![Trainer names setting](https://github.com/Sqhh99/FLiNG-Downloader/raw/3ead2a1731ec5344aec987d2be571dda66676172/docs/pull-requests/assets/2026-10-03-trainer-name-language/settings.png)

Japanese trainer names in the results table and the drawer header. Untranslated rows stay single-line:

![Two-line Japanese names](https://github.com/Sqhh99/FLiNG-Downloader/raw/3ead2a1731ec5344aec987d2be571dda66676172/docs/pull-requests/assets/2026-10-03-trainer-name-language/two-line-names-ja.png)

**i18n:** three new keys in `crates/fling-ui/locales/app.yml` (`settings.trainer_names`, `settings.trainer_names_hint`, `trainer_names.follow_ui`), each with zh-CN, en and ja.

**Translation database:** the packaged `resources/fling_translations.db` is updated to v0.0.5. The schema is unchanged. The model and other packaged resources are unchanged.

**CONTRIBUTING.md:** left for the maintainer to tick.

**AI disclosure:** yes. The feature (code, tests, docs) was written with Claude Code. It is verified by the automated tests and the screenshot checks above. The manual interactive check is still pending.

**Open items**
- The download popover still shows the task's one-line file name. Splitting it into two lines would need display fields on `DownloadTask`.
- Coverage depends on the translation database (now v0.0.5). New releases stay English until the database adds them.
