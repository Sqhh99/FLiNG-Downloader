# PR: English GitHub templates, plus new issue and discussion forms

- **Date:** 2026-10-02
- **Branch:** `docs/github-templates-english` → `docs/records-in-english` (PR #45), then `main`
- **Base commit:** `50f26cf`
- **Related records:** [Work log](../work-logs/2026-10-02-github-templates-english.md)

## Related

No Issue; the maintainer asked for this. Stacked on #45, which switched work logs and PR
records to English. Please merge #45 first.

## What changed

**Translated to English and improved**
- **PR template**
  - Asks for "Related to #123" and explicitly not `Fixes`/`Closes`, because issues are closed
    by hand once a fix ships. The old comment told contributors to write `Fixes #`.
  - New "Type of change" checklist.
  - The testing item asks which flow was tried.
  - New item for `qsTr()` / `tr()` plus `build.cmd i18n`.
  - Screenshots must be before-and-after.
  - The AI disclosure asks whether the AI-written parts were verified.
- **Bug report**
  - New "Where it happens" and "Interface language" dropdowns.
  - Says where to find the app version.
  - Asks for the game name when only one game is affected.
  - Sends title-lookup problems to the new form.
  - Welcomes reports in English, Chinese, or Japanese.
- **Feature request:** new "Related Discussion" and "Workarounds or alternatives" fields.
- **Issue chooser links:** Q&A and Ideas now open the matching Discussion categories directly.

**New templates**
- **Game title not found** (issue form): for Chinese/Japanese titles that find no trainer or
  the wrong one. It collects:
  - what was typed and its language;
  - what went wrong;
  - the English title on flingtrainer.com;
  - the title-database and app versions.

  It also asks the reporter to update the title database first.
- **Discussion forms** for the existing Q&A and Ideas categories.

**Records:** the project-records skill and `CLAUDE.md` now point at the English section
names. The "keep the Chinese headings" exception from #45 is removed.

**Not done**
- The new title form has no label, because none of the existing ones fit. A `translation`
  label could be created for it.
- Title-mapping fixes belong in `game-mappings-updater`, so some of these issues may need
  transferring there.
- `SECURITY.md`, `CONTRIBUTING.md`, and the generated release notes are still in Chinese.

## Type of change

- [ ] Bug fix
- [ ] New feature
- [ ] Performance or refactoring (no behavior change intended)
- [x] Docs, CI, or build only

## How it was tested

- [ ] `build.cmd tests` passes
- [ ] Tried the affected flow in the app (search / download / covers / library / settings — say which)

No code changed, so nothing was built. The YAML forms were **not** machine-parsed: there was
no YAML parser in WSL, and `cmd.exe` interop failed this time. I checked them by hand and
with a grep for unquoted `: `, stray `#`, and tabs. The English UI labels quoted in the forms
were checked against `resources/translations/flingdownloader_en_US.ts`.

**Before or right after merge:** open "New issue" and "New discussion" on GitHub and confirm
each form renders without an error banner. GitHub only renders forms from the default branch.

## Checklist

- [x] I have read [CONTRIBUTING.md](https://github.com/Sqhh99/FLiNG-Downloader/blob/main/CONTRIBUTING.md)
- [ ] UI / QML changes include before-and-after screenshots (no UI / QML changes)
- [ ] New user-facing strings use `qsTr()` / `tr()`, and `build.cmd i18n` was run (no app strings changed)
- [x] Changes to the translation database, i18n files, ONNX model, or packaging layout are called out above (none)
- [x] AI disclosure: Yes. All edits and both records were written by Claude Code at the
  maintainer's request. The forms were checked by reading them, not by rendering them.
