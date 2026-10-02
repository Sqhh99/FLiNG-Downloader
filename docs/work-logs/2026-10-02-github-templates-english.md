# Work log: English GitHub templates, plus new issue and discussion forms

- **Date:** 2026-10-02
- **Branch:** `docs/github-templates-english` (from `docs/records-in-english` at `50f26cf`, which is PR #45)
- **Related:** [PR record](../pull-requests/2026-10-02-github-templates-english.md); follows [the records-in-English change](2026-10-02-records-in-english.md)

## 1. Request
> Further optimize the pull_request_template.md file, and ensure that this file is also in
> English, as is the ISSUE_TEMPLATE file. Then, see if there are any more templates that can
> be added. After making these changes, submit a pull request.

Translate and improve the PR template and the issue forms, look for missing templates, and
open a PR.

## 2. Plan
1. Branch from `docs/records-in-english` instead of `main`. PR #45 made the project-records
   skill quote the PR template's Chinese headings, so translating the template means editing
   the same lines. Stacking avoids a conflict.
2. Rewrite `.github/pull_request_template.md` in English and improve it.
3. Translate `bug.yml`, `feature.yml`, and `config.yml` and add the fields that help triage.
4. Check what the repo is missing. It has Discussions enabled (categories `q-a` and `ideas`
   exist), an existing `SECURITY.md`, and only the default labels. Add:
   - a "Game title not found" issue form;
   - Discussion forms for Q&A and Ideas.
5. Point the skill's PR-record skeleton and `CLAUDE.md` at the new English headings.
6. Check the YAML and open the PR.

## 3. Files changed
| File | Change |
|------|--------|
| `.github/pull_request_template.md` | English. "Related" now asks for "Related to #123" and says not to use `Fixes`/`Closes`: the maintainer closes issues by hand once a fix ships, and the old comment told contributors to write `Fixes #`. Added a "Type of change" checklist. The test item now asks which flow was tried. Added a `qsTr()` / `build.cmd i18n` item, because untranslated strings are a recurring rule in `CLAUDE.md`. Screenshot item made before-and-after (as `CONTRIBUTING.md` asks). AI disclosure now asks whether the AI-written parts were verified. |
| `.github/ISSUE_TEMPLATE/bug.yml` | English. Added "Where it happens" (search, download, covers, library, updates, settings, startup) and "Interface language" dropdowns. The version field says where to find it (Settings → About). Steps ask for the game name when the bug is specific to one. Points title-lookup problems to the new form. Says English, Chinese, and Japanese reports are all welcome. Did **not** add a log field: `Logger` writes only to the console, so release users have no log file to attach. |
| `.github/ISSUE_TEMPLATE/feature.yml` | English. Added "Related Discussion" and "Workarounds or alternatives" fields. |
| `.github/ISSUE_TEMPLATE/config.yml` | English. The Q&A and Ideas links now go straight to the matching Discussion categories instead of the Discussions front page. |
| `.github/ISSUE_TEMPLATE/game-title.yml` | New. For Chinese/Japanese titles that find no trainer or the wrong one: what was typed, its language, what went wrong, the English title on flingtrainer.com, and the title-database and app versions. Tells the reporter to update the title database first (Settings → Translation Database Updates → Check for Updates, the English UI labels). No label, since none of the existing ones fit. |
| `.github/DISCUSSION_TEMPLATE/q-a.yml`, `ideas.yml` | New. File names match the existing category slugs (`q-a`, `ideas`). Q&A sends reproducible problems to the bug form; Ideas points to the feature form once there is agreement. |
| `agents/skills/project-records/SKILL.md` | PR-record skeleton now mirrors the English template. Removed the "keep the Chinese headings verbatim" exception added in PR #45, which no longer applies. |
| `CLAUDE.md` | PR-record row lists the new English section names. |
| `docs/work-logs/2026-10-02-github-templates-english.md`, `docs/pull-requests/2026-10-02-github-templates-english.md` | This work log and the PR record. |

## 4. Verification
- No YAML parser was available in WSL (no PyYAML, no pip, no Ruby or Node), and
  `cmd.exe` interop failed this time (`Exec format error`), so the forms were **not**
  machine-parsed. They were checked by hand and with a grep for unquoted `: `, stray `#`, and
  tabs, which found nothing. GitHub validates issue forms when it renders them. After merge,
  open the "New issue" and "New discussion" pages to confirm every form shows up without an
  error banner.
- UI labels and version formats in the forms were checked against the source:
  - "Settings", "About", "Translation Database Updates", "Check for Updates" and "Current
    Version" in `resources/translations/flingdownloader_en_US.ts`;
  - the database tag format from the `game-mappings-updater` releases (`v0.0.4`);
  - the install options from `make-release.yml` (portable zip, Inno Setup installer).
- Nothing was built; no code changed.

## 5. Open items
- **Labels:** the new title form has no label. A `translation` or `title-mapping` label
  could be created and added to the form; I did not create repository labels without asking.
- **Fixes for title mappings** live in the `game-mappings-updater` repo. Issues filed here may
  need to be transferred there.
- **Still in Chinese and out of scope:** `SECURITY.md`, `CONTRIBUTING.md`, and the release
  notes generated by `make-release.yml`.
- **Stacked PR:** this PR is based on PR #45's branch. Merge #45 first; GitHub then retargets
  this PR to `main`.
