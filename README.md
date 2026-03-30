<div align="center">
  <img src="https://github.com/user-attachments/assets/e8aceb6b-2534-4aaf-a757-020b654aa285" alt="Logo" width="120">
</div>

# FLiNG Downloader

[简体中文](./README.md) | [English](./docs/README.en.md) | [日本語](./docs/README.ja.md)

---

## 功能

- 现代化界面与多主题（浅色、Windows 11、经典、彩色）
- 基于本地 SQLite 翻译库的中/英/日游戏名搜索与建议
- 中文、日文游戏名可自动映射为官网标准英文标题进行检索
- 一键下载与分类管理修改器文件
- 下载任务实时进度、暂停/继续与下载列表管理
- 内置多语言：中文、英文、日文
- 软件更新检测与安装包下载
- 翻译数据库独立更新，支持内置数据库与 AppData 覆盖层版本择优

## 界面截图

![Interface](./resources/interface.png)

## 系统要求

- Windows 10 及以上
- 不需要额外依赖，已静态链接必需库

## 快速开始

- 从 [Releases](../../releases) 下载最新版本，解压运行 `FLiNG Downloader.exe`

## 开发与构建（Windows）

需要安装 Visual Studio 2022、CMake、Qt 6 和 vcpkg。配置好 `VCPKG_ROOT` 与 `CMAKE_PREFIX_PATH` 后，直接运行 `build.cmd`。

项目当前主要依赖：

- Qt 6（Core / Gui / Network / Qml / Quick / QuickControls2）
- SQLiteCpp（读取 `fling_translations.db`）
- OpenCV（封面提取）
- GoogleTest（单元测试 / 集成测试）
- Google Benchmark（性能基准测试）

### build.cmd 用法

- **`build.cmd` 或 `build.cmd release`**: 默认使用 Ninja 构建 Release 版本（输出到 `build\ninja-release`）
- **`build.cmd debug`**: 构建 Debug 版本（输出到 `build\ninja-debug`）
- **`build.cmd run` / `build.cmd run debug`**: 构建并立即运行程序
- **`build.cmd clean`**: 删除整个 `build` 构建目录
- **`build.cmd rebuild`**: 清除旧目录并重新配置、构建
- **`build.cmd i18n`**: 更新所有翻译源码文件（`.ts`）并生成翻译所需文件（`.qm`）
- **`build.cmd tests`**: 配置、构建并运行 GoogleTest 测试
- **`build.cmd benchmark`**: 配置、构建并运行 Google Benchmark
- **`build.cmd benchmark --filter CoverExtractor/all_images`**: 仅运行指定 benchmark 用例

构建完成后，可执行文件位于对应的 `build\ninja-release\` 或 `build\ninja-debug\` 目录下。

### 测试

- 单元测试与集成测试位于 `tests/`
- 默认命令：

```bat
build.cmd tests
```

- 当前优先覆盖：
  - 搜索建议与搜索词 canonical 映射
  - 下载链路核心状态与文件处理
  - 软件更新 / 数据库更新 / SQLite 数据校验

### Benchmark

- 性能基准测试位于 `tests/performance/`
- 当前内置 `CoverExtractor` 基准测试，测试样本位于 `tests/resources/fling_trainer_screenshot/`
- 默认命令：

```bat
build.cmd benchmark
```

- 运行指定用例：

```bat
build.cmd benchmark --filter CoverExtractor/.*
```

## 翻译数据库

- 程序运行时使用外置 SQLite 数据库：`fling_translations.db`
- 发布包自带一份数据库，路径在程序目录下的 `resources/`
- 数据库更新后会写入 `AppData` 覆盖层，程序会自动比较内置库与覆盖库版本，优先使用较新的有效版本
- 数据库文件必须包含：
  - `metadata.release_tag`
  - `metadata.schema_version`
  - `games.english`
  - `games.normalized_english`
  - `games.chinese_simplified`
  - `games.japanese`

## CI / 发布

- `build.yml` 会执行构建与测试
- `make-release.yml` 会生成安装包与便携包，并携带 `fling_translations.db`
- 便携包与安装包都包含启动器、主程序以及外置资源目录

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
- 反馈：请在 GitHub 提交 [Issues](../../issues)
