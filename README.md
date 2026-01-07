<div align="center">
  <img src="https://github.com/user-attachments/assets/e8aceb6b-2534-4aaf-a757-020b654aa285" alt="Logo" width="120">
</div>

# DownloadIntegrator

基于 Qt 的游戏修改器下载管理器，支持中文游戏名智能映射搜索。

[简体中文](./README.md) | [English](./docs/README.en.md) | [日本語](./docs/README.ja.md)

---

## 功能

- 现代化界面与多主题（浅色、Windows 11、经典、彩色）
- 中文游戏名搜索，自动映射为英文进行检索
- 一键下载与分类管理修改器文件
- 内置多语言：中文、英文、日文
- 实时检测并提示可用更新

## 界面截图

![Interface](./resources/interface.png)

## 系统要求

- Windows 10 及以上
- 不需要额外依赖，已静态链接必需库

## 快速开始

- 从 [Releases](../../releases) 下载最新版本，解压运行 `DownloadIntegrator.exe`

## 开发与构建（Windows）

### 依赖

- Visual Studio 2022（含“使用 C++ 的桌面开发”）
- CMake ≥ 3.25
- Qt 6.6.3+（MSVC x64），例如 `C:\Qt\6.10.0\msvc2022_64`
- vcpkg（设置环境变量 `VCPKG_ROOT`）

### 环境变量示例（PowerShell）

```powershell
$env:VCPKG_ROOT = "C:\vcpkg"
$env:CMAKE_PREFIX_PATH = "C:\Qt\6.10.0\msvc2022_64"
```

### 配置与构建

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 \
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET="x64-windows-static"

cmake --build build --config Release
```

构建完成后，可执行文件位于：`build\Release\DownloadIntegrator.exe`。

（可选）如果你使用 CMakePresets，可运行：

```powershell
cmake --list-presets
cmake --preset <configure-preset>
cmake --build --preset <build-preset>
```

## 安全与隐私

- 可能被 Windows Defender 误报（如 `Win32/Wacapew.C!ml`），属误报，程序安全。
- 不收集个人信息；网络请求仅用于搜索与下载；配置仅本地存储。

## 许可与反馈

- 许可：MIT，详见 [LICENSE](LICENSE)
- 反馈：请在 GitHub 提交 [Issues](../../issues)
