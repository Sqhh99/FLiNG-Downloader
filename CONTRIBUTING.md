# 参与贡献

感谢愿意给 FLiNG Downloader 提问题或补丁。这是个人维护的 Windows 桌面项目，请先对齐范围，再投入编码时间。

## 先讨论，再写代码

- **缺陷**：先确认是最新 [Release](https://github.com/Sqhh99/FLiNG-Downloader/releases/latest)，并排除 [杀毒误报](docs/ANTIVIRUS_FAQ.md)。用 Bug 模板开 Issue。
- **功能**：先发 [Discussions](https://github.com/Sqhh99/FLiNG-Downloader/discussions)，有共识后再开 Issue 或 PR。
- **安全漏洞**：只走 [Security Advisory](https://github.com/Sqhh99/FLiNG-Downloader/security/advisories/new)，不要开公开 Issue。
- 可直接开 PR 的：错字、文档、小范围且可复现的修复，并在 PR 里写清验证步骤。

## 开发环境

Windows 10+，需要 Rust stable（`x86_64-pc-windows-msvc`）和 Visual Studio 2022 及以上的 C++ 生成工具。

```bat
cargo run -p fling-ui
cargo test --workspace
cargo clippy --workspace --all-targets -- -D warnings
cargo fmt --all
```

常用命令见根目录 [README.md](README.md)。给 agent 用的仓库地图在 [AGENTS.md](AGENTS.md)。

## 提交与 PR

- Commit 使用 [Conventional Commits](https://www.conventionalcommits.org/)：`feat:`、`fix:`、`docs:`、`chore:` 等。
- 一个 PR 只做一件事。
- 用仓库里的 PR 模板：关联 Issue、说明用户可见变化、列出验证命令。
- 界面改动请附前后截图。
- 改动了 `fling_translations.db`、界面文案（`crates/fling-ui/locales/app.yml`）、ONNX 模型或打包布局时，在 PR 里单独点名。

## AI 辅助

可以用 AI 辅助阅读和起草补丁，但提交者必须能解释每一处改动和边界行为。PR 模板里如实填写 AI 披露。不要用模型代写 Issue / 评论。

## 许可

向本仓库提交代码，即表示同意以 [GNU AGPL v3](LICENSE) 授权。
