# Work log: trainer-name language setting

- **Date:** 2026-10-03
- **Branch:** `feat/trainer-name-language` (from `main` at `372491d`)
- **Related:** [PR record](../pull-requests/2026-10-03-trainer-name-language.md)

## 1. Request
> An optimization feature for translation display needs to be added, with the option located in the translation section of the settings. Currently, trainer filenames—whether from search results or downloads—default to English, which is not ideal for Chinese and Japanese users. A name-mapping function could be added to the settings, allowing the displayed trainer names to be set to English, Japanese, or Chinese, thereby making it much more convenient for users to download and use the trainers.

The user wants a setting under Settings › Language that shows trainer names in English, Chinese or Japanese. It should apply wherever a name appears, including the names of downloaded files.

The user made four decisions during planning:
- **Scope.** The setting covers the display *and* the file names of new downloads. Existing files are not renamed.
- **Format.** The translated title plus the English title in brackets, everywhere, e.g. `艾尔登法环 (Elden Ring)`.
- **Default.** The options are Follow interface language / English / 简体中文 / 日本語, and the default is "Follow interface language".
- **No "Trainer" word.** No localized 修改器 / トレーナー suffix; only the game title is translated.

## 2. Plan
The approved plan is in `~/.claude/plans/please-take-a-look-abundant-catmull.md`. Steps, in order:

1. **`fling-core`.** Add the `TrainerNameLanguage` enum and a `display_name` field on `ModifierInfo` and `DownloadedModifier`. On `DownloadedModifier` it is `#[serde(skip)]`.
2. **`fling-mapping`.** Add `GameMappings::localized_title`, a reverse lookup from a normalized English title to the Chinese or Japanese one.
3. **`fling-app`.** Add `names::display_name`, which turns a site name into its display name. It also:
   - stores the setting;
   - adds the `SetTrainerNameLanguage` command;
   - fills display names on results, the selection and the library;
   - re-labels everything when the setting, the UI language (under "follow") or the database changes;
   - names new downloads after the display name.
4. **`fling-download`.** `enqueue` takes a file stem. The library entry keeps the site name.
5. **`fling-ui`.** Add the settings select and hint, render `display_name`, add the strings, and add a debug hook.
6. **Docs and tests.**

**Design notes**
- **`name` stays the site's English title.** Cover ids (`cover_game_id`), the cover cache, `downloaded_modifiers.json` (Qt compatibility) and search relevance all key on it. Only the new `display_name` changes.
- **Matching is exact after `normalize_lookup_text`.** Before the lookup, entities are decoded with `fling_site::parser::decode_html_entities` and a trailing standalone "Trainer" word is dropped.
  - The index keys are both the `english` and the `normalized_english` columns. So `Assassin&#8217;s Creed 3` decodes to `Assassin's Creed 3` and still matches the DB's `Assassin’s Creed 3`.
  - There is no contains or fuzzy fallback. "The Witcher 3: Wild Hunt – Remastered" must not borrow the base game's title.
- **A name stays exactly as the site gives it** when the game is unknown, or when the target title is empty or equal to the English title. Many Japanese rows just repeat the English title, e.g. `Total War: Warhammer III`.
- **English mode returns the raw site name**, with entities undecoded as before. The display is identical to before.
- **Name sort orders by `display_name`**, so the visible column is what is sorted.
- **A re-label re-sends `Results`.** The UI treats that like a re-sort: the row highlight and the scroll position reset, the same as changing the sort order today. The drawer stays open with the new name.
- **The delete-failed banner** uses the display name too.

## 3. Files changed
| File | Change | Addresses |
|------|--------|-----------|
| `crates/fling-core/src/model.rs` | `TrainerNameLanguage` (keys `auto`/`en`/`zh`/`ja`, `resolve`), `display_name` on `ModifierInfo` and `DownloadedModifier` (not serialized), tests | Setting type; UI-facing name without touching the canonical one |
| `crates/fling-mapping/src/mappings.rs` | `titles` index and `localized_title`, tests (case/punctuation, curly apostrophe, empty/repeated titles) | English → CN/JA lookup |
| `crates/fling-app/src/names.rs` (new) | `display_name`: decode, strip "Trainer", look up, format `译名 (English)`; tests | Name formatting in the backend, not the UI |
| `crates/fling-app/src/lib.rs` | `mod names` | — |
| `crates/fling-app/src/api.rs` | `Command::SetTrainerNameLanguage`, `SettingsSnapshot::trainer_name_language`, re-export | Frontend contract |
| `crates/fling-app/src/backend.rs` | `name_language`, `label`, `emit_library`, `relabel_all`; labels in `search_done`; re-label on setting/UI-language change and after a DB install; download uses `display_name` as file stem; name sort by `display_name` | Display + file names follow the setting |
| `crates/fling-config/src/settings.rs` | `trainerNameLanguage` key, getter/setter, tests | Persistence; Qt-era files read as "follow" |
| `crates/fling-download/src/queue.rs` | `enqueue(.., file_stem)`; task `file_name` starts as the stem; test for a CJK file name with an English library name | Localized file names |
| `crates/fling-download/src/library.rs` | Test literal gains `display_name` | Compile |
| `crates/fling-app/tests/backend.rs` | `trainer_names_follow_settings_and_name_new_downloads` | End-to-end regression |
| `crates/fling-ui/src/views/settings_panel.rs` | "Trainer names" select (280 px) + hint in the Language section; option names re-translated on UI-language change; `settings-language` debug hook | Setting UI |
| `crates/fling-ui/src/i18n.rs` | `trainer_name_language_name` | Option labels |
| `crates/fling-ui/locales/app.yml` | `settings.trainer_names`, `settings.trainer_names_hint`, `trainer_names.follow_ui` (zh-CN/en/ja); the language options reuse `lang.*` | Strings |
| `crates/fling-ui/src/views/widgets.rs` | `trainer_name(display, name)` with fallback | Shared by the three views |
| `crates/fling-ui/src/views/{search_page,detail_drawer,library_page}.rs` | Show `display_name` | Localized names on screen |
| `CLAUDE.md` | `localized_title`/`display_name` note; debug-hook list (`settings-language`, `downloads`) | Agent docs |
| `README.md`, `docs/README.en.md`, `docs/README.ja.md` | One feature line each | User docs |

## 4. Verification
Run from WSL through `cmd.exe` on the Windows toolchain:
- [x] `cargo fmt --all`, then `cargo fmt --all --check` (exit 0)
- [x] `cargo clippy --workspace --all-targets -- -D warnings` (exit 0)
- [x] `cargo test --workspace` (exit 0). The 12 new tests all pass:
  - `fling-core`: 2
  - `fling-mapping`: 3
  - `fling-app` `names`: 4
  - `fling-config`: 1
  - `fling-download`: 1
  - `fling-app` integration: 1
- [x] `cargo build -p fling-ui`.
- [x] `PrintWindow` screenshots of the app window only:
  - `FLING_DEBUG_OPEN=settings-language` shows the new select and hint. The first capture cut off "Follow Interface Language" in a 200 px select; it was widened to 280 px and re-captured.
  - `FLING_DEBUG_OPEN=drawer:0` runs with the user's own settings. The UI is in English, so under "follow" the names stay English, as expected.
- [x] **One-off check against the real bundled DB.** A temporary test, deleted afterwards, mapped the trainers on the saved homepage fixture (`crates/fling-site/tests/fixtures/homepage.html`).
  - 7 of 15 got a Chinese name, e.g. `全面战争：战锤3 (Total War: Warhammer III)`, `英灵神殿 (Valheim)`, `三国志14 (Romance Of The Three Kingdoms XIV)`.
  - The 8 misses are not in the DB (*Witcher 3 Remastered*, *Onimusha 2*, *Trails in the Sky 2nd Chapter*, …).
- [ ] **Chinese/Japanese names in the running UI** are not checked by screenshot. That would mean changing the user's real settings, and input injection is off-limits. The integration test covers the backend path; the on-screen check is the user's.
- [ ] **Interactive checks are the user's:** switching the setting, downloading a trainer and checking the file name, and existing library entries showing translated names.

## 5. Follow-up: two-line names

> Let's switch the display of the translated trainer name to a drawer-style layout; if it's shown in a single list row, it might overflow the available space and become hard to read.

`艾尔登法环 (Elden Ring)` on one line truncates in the 3-weight name column. Translated names are now stacked like a drawer header: the translated title on top, and the English title underneath in smaller, muted text. Download file names keep the one-line `艾尔登法环 (Elden Ring)` form.

**Plan.** The backend sends the two parts separately, so the UI never splits a formatted string:
- `display_name` is the title;
- the new `display_subtitle` is the English title, empty when untranslated.

The UI renders a second line only when there is a subtitle.

| File | Change | Addresses |
|------|--------|-----------|
| `crates/fling-core/src/model.rs` | `display_subtitle` on `ModifierInfo` and `DownloadedModifier` (`#[serde(skip)]`); persistence test covers it | Structured name parts |
| `crates/fling-app/src/names.rs` | `display_name()` → `label()` returning `TrainerLabel { title, subtitle }` with `one_line()` for file names/messages; tests updated | Same lookup, two parts |
| `crates/fling-app/src/backend.rs` | `trainer_label`/`label_modifier` fill both fields; the download stem and the delete-failed message use `one_line()` | — |
| `crates/fling-download/src/{queue,library}.rs` | Struct literals gain `display_subtitle` | Compile |
| `crates/fling-app/tests/backend.rs` | Asserts title and subtitle separately; JSON must not contain either | Regression |
| `crates/fling-ui/src/views/widgets.rs` | `trainer_name_cell`: a 17 px title line over a 15 px `text_xs` muted subtitle, both truncating, within the unchanged 40 px row; single line when untranslated | No overflow in lists |
| `crates/fling-ui/src/views/{search_page,library_page}.rs` | Use `trainer_name_cell` | — |
| `crates/fling-ui/src/views/detail_drawer.rs` | Header title stacked over a 13 px subtitle; header grows from fixed 48 px to `min_h` 48 px + `py_2` | Drawer header |
| `CLAUDE.md` | Name note updated | Agent docs |

**Verification**
- [x] `cargo fmt --all --check`, `cargo clippy --workspace --all-targets -- -D warnings` and `cargo test --workspace` all exit 0.
- [x] `cargo build -p fling-ui`.
- [x] **Screenshots of translated names (Chinese, then Japanese)**, taken with a temporary, uncommitted patch that made `Settings::trainer_name_language` read an env var. This avoided changing the user's real `settings.ini`.
  - The patch was reverted afterwards. The source was re-touched and rebuilt, and the exe no longer contains the env var name.
  - The search table and the library-style rows show two lines inside the 40 px rows. Untranslated rows stay single-line.
  - The first drawer capture had the title pressed against the top edge; `py_2` fixed it, confirmed in a re-capture.
- [ ] **The download list** still shows the task's file name on one line (`艾尔登法环 (Elden Ring)`). That is the real file name, and it truncates like before. Not changed; see open items.

## 6. Open items
- **The download popover** shows the one-line file name, not the two-line layout. Splitting it would need display fields on `DownloadTask`. Left as is unless asked.
- **Coverage depends on the translation DB** (1,157 games, release `v0.0.3`). New releases stay English until the `game-mappings-updater` DB adds them.
- **Existing downloaded files keep their names.** Only new downloads use the display name.
- **A pending download keeps the name it was queued with.** Its file name is fixed at enqueue, so the task row doesn't change if the setting changes mid-download.
- **Interactive checks are the user's.** The PR was opened on the user's request on 2026-10-03, before any manual test of the setting, the download file names or existing library entries.
