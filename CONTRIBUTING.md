# Contributing

**English** · [简体中文](docs/CONTRIBUTING.zh-CN.md)

Thank you for reporting problems or sending patches to FLiNG Downloader. This is a Windows desktop project maintained by one person, so please agree on the scope before you spend time writing code.

## Discuss first, then code

- **Bugs:** first check that you are on the latest [release](https://github.com/Sqhh99/FLiNG-Downloader/releases/latest), and rule out an [antivirus false positive](docs/ANTIVIRUS_FAQ.md). Then open an issue with the Bug template.
- **Features:** start in [Discussions](https://github.com/Sqhh99/FLiNG-Downloader/discussions). Open an issue or pull request once there is agreement.
- **Security vulnerabilities:** report them only through a [Security Advisory](https://github.com/Sqhh99/FLiNG-Downloader/security/advisories/new), never in a public issue.
- **Pull requests you can open directly:** typo fixes, documentation, and small, reproducible fixes. Describe how you verified the change in the pull request.

## Development environment

Windows 10 or later, with stable Rust (`x86_64-pc-windows-msvc`) and the Visual Studio 2022 (or later) C++ build tools.

```bat
cargo run -p fling-ui
cargo test --workspace
cargo clippy --workspace --all-targets -- -D warnings
cargo fmt --all
```

More commands are in the root [README.md](README.md#building-from-source). The repository map for AI agents is in [AGENTS.md](AGENTS.md).

## Commits and pull requests

- Write commit messages in the [Conventional Commits](https://www.conventionalcommits.org/) style: `feat:`, `fix:`, `docs:`, `chore:` and so on.
- One pull request does one thing.
- Use the repository's pull request template: link the issue, describe what users will notice, and list the commands you ran to verify it.
- Include before-and-after screenshots for UI changes.
- Call out changes to `fling_translations.db`, UI strings (`crates/fling-ui/locales/app.yml`), the ONNX model or the packaging layout explicitly in the pull request.

## AI assistance

You may use AI to help read code and draft patches, but you must be able to explain every change and how it behaves at the edges. Fill in the AI disclosure in the pull request template honestly. Do not use a model to write issues or comments for you.

## License

By contributing to this repository, you agree to license your contribution under the [GNU AGPL v3](LICENSE).
