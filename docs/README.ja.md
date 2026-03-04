<div align="center">
  <img src="https://github.com/user-attachments/assets/e8aceb6b-2534-4aaf-a757-020b654aa285" alt="Logo" width="120">
</div>

# FLiNG Downloader

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

- [Releases](../../releases) から最新版を取得し、`FLiNG Downloader.exe` を実行

## 開発・ビルド（Windows）

Visual Studio 2022、CMake、Qt 6、vcpkg が必要です。`VCPKG_ROOT` と `CMAKE_PREFIX_PATH` を設定した後、`build.cmd` を直接実行してください。

### `build.cmd` の使い方

- **`build.cmd` または `build.cmd release`**: デフォルトで Ninja を使用して Release 版をビルドします（出力先: `build\ninja-release`）
- **`build.cmd debug`**: Debug 版をビルドします（出力先: `build\ninja-debug`）
- **`build.cmd run` / `build.cmd run debug`**: ビルドしてすぐに実行します
- **`build.cmd clean`**: `build` ディレクトリ全体を削除します
- **`build.cmd rebuild`**: 古いディレクトリを削除し、再構成・ビルドします
- **`build.cmd i18n`**: 翻訳ソースファイル（`.ts`）を更新し、翻訳ファイル（`.qm`）を生成します

ビルド完了後、実行ファイルは対応する `build\ninja-release\` または `build\ninja-debug\` ディレクトリに生成されます。

## セキュリティとプライバシー

- Windows Defender により誤検知される可能性あり（例：`Win32/Wacapew.C!ml`）。安全です。
- 個人情報は収集しません。通信は検索とダウンロードのみ。設定はローカル保存。

## ライセンスとフィードバック

- ライセンス：GNU AGPL v3（[LICENSE](../LICENSE)）
- 問題報告：GitHub の [Issues](../../issues)
