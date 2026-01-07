<div align="center">
  <img src="https://github.com/user-attachments/assets/e8aceb6b-2534-4aaf-a757-020b654aa285" alt="Logo" width="120">
</div>

# DownloadIntegrator

Qt 製のゲームトレーナーダウンロード管理ツール。中国語タイトルの英語マッピング検索に対応。

[简体中文](./README.md) | [English](./README.en.md) | [日本語](./README.ja.md)

---

## 機能

- モダン UI と複数テーマ（ライト／Windows 11／クラシック／カラー）
- 中国語ゲーム名で検索し、英語に自動マッピングして検索
- 1クリックでダウンロードと分類管理
- 内蔵言語：中文・英語・日本語
- 更新のリアルタイム検出

## スクリーンショット

![Interface](./resources/interface.png)

## 動作環境

- Windows 10 以降
- 必要なライブラリは静的リンク済み（追加の依存は不要）

## クイックスタート

- [Releases](../../releases) から最新版を取得し、`DownloadIntegrator.exe` を実行

## 開発・ビルド（Windows）

### 必要ツール

- Visual Studio 2022（C++ デスクトップ開発）
- CMake ≥ 3.25
- Qt 6.6.3+（MSVC x64）例：`C:\Qt\6.10.0\msvc2022_64`
- vcpkg（`VCPKG_ROOT` を設定）

### 環境変数例（PowerShell）

```powershell
$env:VCPKG_ROOT = "C:\vcpkg"
$env:CMAKE_PREFIX_PATH = "C:\Qt\6.10.0\msvc2022_64"
```

### 設定とビルド

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 \
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET="x64-windows-static"

cmake --build build --config Release
```

生成物：`build\Release\DownloadIntegrator.exe`

（任意）CMakePresets を使う場合：

```powershell
cmake --list-presets
cmake --preset <configure-preset>
cmake --build --preset <build-preset>
```

## セキュリティとプライバシー

- Windows Defender により誤検知される可能性あり（例：`Win32/Wacapew.C!ml`）。安全です。
- 個人情報は収集しません。通信は検索とダウンロードのみ。設定はローカル保存。

## ライセンスとフィードバック

- ライセンス：MIT（[LICENSE](LICENSE)）
- 問題報告：GitHub の [Issues](../../issues)
