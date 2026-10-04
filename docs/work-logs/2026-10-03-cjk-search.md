# Work log: Chinese and Japanese search returns nothing

- **Date:** 2026-10-03
- **Branch:** `fix/cjk-search` (from `main` at `62f2e2d`, the v1.2.0 release commit)
- **Related:** no PR record yet

## 1. Request
> There is also an issue with the software's search function: the current search bar seems to support only English queries. Searches using Chinese or Japanese yield no matches.

Make pressing Enter on a Chinese or Japanese query find trainers.

## 2. Plan
1. Reproduce with the headless example (`cargo run -p fling-app --example headless -- search <term>`). It runs the real backend against the live site in a throwaway data directory, not the user's settings.
2. Find out why the search finds nothing.
3. Fix it in `fling-mapping` and `fling-site`, with regression tests.
4. Run the live searches again.

### Root cause
`SiteClient::search` translated input with `GameMappings::translate_for_search`. That function accepts only **exact** or normalized-exact database titles. Users rarely type an exact title, for two reasons:
- **The database often has no row for the base game.** For example, there is no plain 艾尔登法环 row, only 艾尔登法环 黑夜君临 and 艾尔登法环 黄金树幽影.
- **Its titles carry suffixes.** For example 生化危机4 重制版, 生化危机：村庄.

On a miss, the raw Chinese or Japanese text went to flingtrainer.com, which indexes only English, so the search returned 0 results. The headless run confirmed it: 艾尔登法环, 艾尔登, 生化危机, エルデンリング and 黑神话 all returned 0, while "Elden Ring" returned 2.

The suggestion dropdown was not affected. It already did substring matching, and picking a suggestion searches its English title. Only typing a query and pressing Enter failed.

### Design
`GameMappings::search_terms(input) -> Vec<String>` returns the English searches for an input, best first. It works through these cases in order:
1. **Exact or normalized-exact match:** that title, as before.
2. **Latin input** (no Chinese characters or kana) without an exact match: nothing, so the input is searched as typed.
   - This keeps the deliberate rule that a broad Latin query such as "ace combat" stays a site search.
3. **Chinese or Japanese input** is matched as a substring of the normalized Chinese and Japanese titles. Titles that *start* with the query rank first.
   - **One game:** its English title. For example, 生化危机4 gives "Resident Evil 4".
   - **Several games:** the leading English words they all share. For example, 生化危机 gives "Resident Evil", and エルデンリング gives "Elden Ring". This is one request, and the site returns the whole series.
     - The shared words must include a word of 3+ characters that isn't generic ("the", "a", "an", "of", "and"). Otherwise "The Witcher 3" and "The Last of Us" would search for "The".
   - **Several games with no usable shared words:** up to `MAX_SEARCH_TITLES` (5) titles, each searched separately.
4. **No title contains the query:** the longest title the query *contains*. For example, 艾尔登法环 黑夜君临 修改器 gives "Elden Ring Nightreign".

`SiteClient::search` then works as follows:
- It requests every term concurrently.
- It parses each page and sorts it by relevance to its own term.
- It concatenates the lists in term order. With several terms, it drops rows whose URL an earlier term already returned.
- It enriches the merged list once.
- It fails only when every request fails. With a single term, behavior is exactly as before.

## 3. Files changed
| File | Change | Addresses |
|------|--------|-----------|
| `crates/fling-mapping/src/mappings.rs` | Added `search_terms`, `MAX_SEARCH_TITLES` and the `common_leading_words` helper. Added six tests: exact match wins; partial Chinese/Japanese gives the shared English words or a single title; ranking and the generic-prefix rule; a title inside a longer query; Latin input untouched; the five-title cap. | The CJK query has no exact match |
| `crates/fling-site/src/site.rs` | `search` uses `search_terms`, searches several terms concurrently, merges and de-duplicates them, and tolerates partial failure. Added three tests: a partial Chinese query searches "Resident Evil"; two titles are searched and merged without duplicates; one failed term keeps the other's results, and all failing is an error. | Sends English to the site |
| `CLAUDE.md` | The `fling-mapping` bullet described exact-only matching; it now describes `search_terms`. | Keeps the agent guide accurate |
| `docs/work-logs/2026-10-03-cjk-search.md` | This log. | Mandatory record |

## 4. Verification
On the Windows toolchain, from WSL through `cmd.exe`, one chained command exited 0:
- `cargo fmt --all --check`
- `cargo clippy --workspace --all-targets -- -D warnings`
- `cargo test --workspace`

**Live check.** The headless example (`cargo build -p fling-app --example headless`) used the real bundled database v0.0.5 and the live site.

| Query | Before | After |
|-------|--------|-------|
| 艾尔登法环 | 0 | 2 (Elden Ring Shadow of the Erdtree, Elden Ring Nightreign) |
| エルデンリング | 0 | 2 (same) |
| 生化危机 | 0 | 8 (Resident Evil Requiem, 4, Village, …) |
| バイオハザード | 0 | 8 (same) |
| 生化危机4 | — | 6 (Resident Evil 4 among them) |
| 黑神话 | 0 | 1 (Black Myth: Wukong) |
| 赛博朋克 | — | 1 (Cyberpunk 2077) |
| 巫师3 | — | 2 (The Witcher 3: Wild Hunt and its Remastered) |
| 空洞骑士 | — | 2 (Hollow Knight, Hollow Knight: Silksong) |

"—" means the query was not run before the fix.

**Not checked:** the GUI itself. `cargo build -p fling-ui` was not run, and the search bar was not exercised interactively. The UI sends the same `Command::Search` the headless example sends, so the backend path is the same. Pressing Enter in the real app is the maintainer's check.

## 5. Open items
- **Games missing from the database** still go to the site as typed and find nothing. The fix depends on database coverage (1,185 games in v0.0.5).
- **Ordering of a series search.** 生化危机4 searches "Resident Evil 4", but the site's fuzzy filter also returns other Resident Evil trainers. The app's current sort (here, last update) can place Resident Evil 4 second. Latin searches behave the same way, and this change does not alter it.
- **Not yet released.** This fix is not in v1.2.0 (tagged at `62f2e2d`) and needs a v1.2.1.
