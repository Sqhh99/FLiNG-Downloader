<div align="center">
  <img src="https://github.com/user-attachments/assets/e8aceb6b-2534-4aaf-a757-020b654aa285" alt="Logo" width="120">
</div>

# FLiNG Downloader

[简体中文](./README.md) | [English](./docs/README.en.md) | [日本語](./docs/README.ja.md)

---

## 功能

- 原生 GPU 渲染界面（Rust + GPUI），9 套主题（浅色、深色、海洋、日落、森林、薰衣草、玫瑰、午夜、摩卡）
- 基于本地 SQLite 翻译库的中/英/日游戏名搜索与建议
- 中文、日文游戏名可自动映射为官网标准英文标题进行检索
- 修改器名称可显示为中文或日文（如「艾尔登法环 (Elden Ring)」），新下载的文件也按该名称命名
- 一键下载与分类管理修改器文件
- 下载任务实时进度、暂停/继续与下载列表管理
- 内置多语言：中文、英文、日文（切换即时生效）
- 基于本地 ONNX 模型自动从修改器截图中裁剪游戏封面
- 软件更新检测与安装包下载
- 翻译数据库独立更新，支持内置数据库与 AppData 覆盖层版本择优

## 界面截图

![Interface](./resources/interface.png)

## 系统要求

- Windows 10（1903）及以上，64 位
- 无需安装任何运行库：便携包与安装包已附带所需文件（ONNX Runtime 已内置于程序中）

## 与 FLiNG 的关系

本项目是独立的开源下载工具，**不隶属于**风灵月影 / FLiNG Trainer 官方。软件不托管、不二次分发修改器本体，只按你的操作从公开来源检索并下载。游戏修改器可能违反部分游戏的用户协议，请自行判断使用风险。

## 快速开始

- 从 [Releases](../../releases) 下载最新版本
- 便携包：解压后运行 `FLiNG Downloader.exe`
- 安装包：运行 `setup.exe`
- 校验文件：对照同版本的 `SHA256SUMS.txt`（新版本起提供）
- Windows Defender 误报见 [杀毒 FAQ](./docs/ANTIVIRUS_FAQ.md)

## 开发与构建（Windows）

需要 Rust stable（`x86_64-pc-windows-msvc`）和 Visual Studio 2022 及以上的 C++ 生成工具。不需要 Qt、CMake 或 vcpkg；ONNX Runtime 会在首次构建时自动下载并静态链接。

```bat
cargo run -p fling-ui                 :: 构建并运行（Debug）
cargo test --workspace                :: 全部测试
cargo clippy --workspace --all-targets -- -D warnings
cargo fmt --all
cargo bench -p fling-cover            :: 封面识别基准测试
cargo xtask dist [--version 1.2.0]    :: 生成发布目录 dist\FLiNG Downloader\
cargo xtask notices                   :: 重新生成 THIRD_PARTY_NOTICES.md 中的依赖列表
```

### 项目结构

Cargo workspace，前后端分离：界面只通过命令 / 事件与后端通信。

| Crate | 职责 |
|---|---|
| `fling-core` | 领域类型与纯函数（版本比较、标题归一化、文件类型识别） |
| `fling-net` | HTTP 抽象与 reqwest 实现（续传、Referer、超时），测试用的假实现 |
| `fling-config` | AppData 路径与兼容 QSettings 的 `settings.ini` |
| `fling-mapping` | 翻译数据库、中日→英文标题映射、搜索建议 |
| `fling-site` | flingtrainer.com 页面解析、搜索、最近更新列表 |
| `fling-download` | 下载队列（暂停 / 继续 / 并发 3）与已下载列表 |
| `fling-update` | GitHub / Gitee 软件与数据库更新 |
| `fling-cover` | 封面缓存与 ONNX 封面识别 |
| `fling-app` | 后端门面：持有全部服务，对外暴露 `Command` / `Event` |
| `fling-ui` | GPUI 界面（`FLiNG Downloader.exe`） |
| `xtask` | 打包与仓库任务 |

### 测试

- 每个 crate 自带单元测试；`fling-app/tests`、`fling-site/tests`、`fling-mapping/tests` 为集成测试
- 测试不访问外网、不读写真实 AppData（使用假 HTTP 客户端与临时目录）
- `fling-site/tests/fixtures/` 保存了官网页面快照，`fling-cover/tests/screenshots/` 为封面识别样本

## 翻译数据库

- 程序运行时使用外置 SQLite 数据库：`fling_translations.db`
- 发布包自带一份数据库，路径在程序目录下的 `resources/`
- 数据库更新后会写入 `AppData` 覆盖层，程序会自动比较内置库与覆盖库版本，优先使用较新的有效版本
- 数据库文件必须包含：
  - `metadata.release_tag`
  - `metadata.schema_version`（可选；存在时必须为 `1`）
  - `games.english`
  - `games.normalized_english`
  - `games.chinese_simplified`
  - `games.japanese`

## CI / 发布

- `build.yml` 会执行格式检查、clippy、测试与打包
- `make-release.yml` 会生成安装包与便携包，并携带 `fling_translations.db` 与 `SHA256SUMS.txt`
- 便携包与安装包包含主程序、封面模型 `models/`、内置数据库 `resources/` 与 MSVC 运行库

### 版本发布

- 推送以 `v` 开头的 tag 会自动触发 `make-release.yml` 并创建 GitHub Release
- 正式版 tag 示例：

```bash
git tag v1.2.0
git push origin v1.2.0
```

- 预发布版 tag 示例：

```bash
git tag v1.2.0-beta.1
git push origin v1.2.0-beta.1
```

- 规则固定为：
  - 不含 `-` 的 tag：正式版
  - 含 `-` 的 tag：GitHub Pre-release

## 安全与隐私

- 可能被 Windows Defender 误报（如 `Win32/Wacapew.C!ml`），属误报，程序安全。
- 不收集个人信息；网络请求仅用于搜索、下载、更新检查；配置与数据库覆盖文件仅本地存储。

## 许可与反馈

- 许可：GNU AGPL v3，详见 [LICENSE](LICENSE)
- 第三方组件许可：[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
- 缺陷与已讨论过的功能：[Issues](../../issues)
- 使用疑问与想法：[Discussions](../../discussions)
- 参与开发：[CONTRIBUTING.md](CONTRIBUTING.md)
- 安全漏洞：[SECURITY.md](SECURITY.md)
