# Work log: tint the Show All button and the settings update cards

- **Date:** 2026-10-03
- **Branch:** `fix/show-all-and-update-card-colors` (from `main` at `74c47ef`)
- **Related:** none yet

## 1. Request
> The most suitable combination of modifications is the local demand modification, the main screen "Show All" optional background color is improved, the setting surface is "Software Updates" and the "Translation Database Updates" background color is improved, the most preferable is the main screen and the main screen is further modified.

The maintainer sent two screenshots in the Sunset theme:
- **Main screen:** "Show All" is circled. It was a default button, drawn white next to the orange "Search".
- **Settings › About:** the "Software Updates" and "Translation Database Updates" cards were pure white on the cream dialog.

The request is to give both a better background color.

## 2. Plan
1. Make "Show All" a `secondary()` button. That uses the palette's hover tint with primary-colored text, the same as "Check for Updates", "Browse" and "Check Database Updates".
2. Change the update cards' background from `card` (white in every light palette) to `alternate_row`, the tint the settings navigation column already uses. Keep the border.

## 3. Files changed
| File | Change | Addresses |
|------|--------|-----------|
| `crates/fling-ui/src/views/search_page.rs` | "Show All" → `.secondary()` | Show All background |
| `crates/fling-ui/src/views/settings_panel.rs` | `update_card` background `c.card` → `c.alternate_row` | Update-card background |

## 4. Verification
- [x] `cargo fmt --all --check`, `cargo clippy --workspace --all-targets -- -D warnings` and `cargo test --workspace` all exit 0.
- [x] `cargo build -p fling-ui`, then `PrintWindow` captures of the app window in the maintainer's Sunset theme:
  - **Main screen:** Show All is peach (`#FFE0B2`) with orange text, matching the other secondary buttons.
  - **`FLING_DEBUG_OPEN=settings-about`:** both cards use the sidebar's `#FFF3E0` tint.
- [ ] **The other eight themes were not captured.** Switching themes would change the maintainer's real settings. Going by the palette values:
  - In Light, Dark, Sunset and Midnight, `alternate_row` differs from the dialog background, so the cards are tinted.
  - In Ocean, Forest, Lavender, Rose and Mocha, `alternate_row` equals the background, so the cards blend in and only their border shows. The settings sidebar already looks that way in those themes.

## 5. Open items
- **A distinct card tint for those five themes** would need a new palette token, e.g. a blend of `card` and `background`. Not done unless asked.
