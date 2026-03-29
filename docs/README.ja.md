<div align="center">
  <img src="https://github.com/user-attachments/assets/e8aceb6b-2534-4aaf-a757-020b654aa285" alt="Logo" width="120">
</div>

# FLiNG Downloader

[简体中文](../README.md) | [English](./README.en.md) | [日本語](./README.ja.md)

---

## 機能

- モダン UI と複数テーマ（ライト、Windows 11、クラシック、カラフル）
- ローカル SQLite 翻訳データベースによる中英日ゲーム名検索とサジェスト
- 中国語・日本語のゲーム名を FLiNG 公式サイトで使われる標準英語タイトルに自動変換して検索
- ワンクリックダウンロードとトレーナーの分類管理
- ダウンロード進捗のリアルタイム表示、一時停止 / 再開、ダウンロード済み一覧管理
- 内蔵言語：中文・英語・日本語
- アプリ本体の更新チェックとインストーラーのダウンロード
- 翻訳データベースの独立更新に対応し、同梱版と AppData 上書き版のうち新しい有効な方を自動選択

## スクリーンショット

![Interface](../resources/interface.png)

## 動作環境

- Windows 10 以降
- 追加のランタイム依存は不要。必要なライブラリはアプリに同梱済み

## クイックスタート

- [Releases](../../releases) から最新版をダウンロードし、展開して `FLiNG Downloader.exe` を実行

## 開発・ビルド（Windows）

Visual Studio 2022、CMake、Qt 6、vcpkg が必要です。`VCPKG_ROOT` と `CMAKE_PREFIX_PATH` を設定した後、`build.cmd` を実行してください。

現在の主な依存関係：

- Qt 6（Core / Gui / Network / Qml / Quick / QuickControls2）
- SQLiteCpp（`fling_translations.db` の読み込み）
- OpenCV（カバー画像抽出）
- GoogleTest（単体テスト / 結合テスト）
- Google Benchmark（性能ベンチマーク）

### `build.cmd` の使い方

- **`build.cmd` または `build.cmd release`**: デフォルトで Ninja を使って Release ビルド（出力先: `build\ninja-release`）
- **`build.cmd debug`**: Debug ビルド（出力先: `build\ninja-debug`）
- **`build.cmd run` / `build.cmd run debug`**: ビルド後すぐに起動
- **`build.cmd clean`**: `build` ディレクトリ全体を削除
- **`build.cmd rebuild`**: 旧ビルドを削除して再構成・再ビルド
- **`build.cmd i18n`**: 翻訳ソース（`.ts`）を更新し、翻訳ファイル（`.qm`）を生成
- **`build.cmd tests`**: GoogleTest ターゲットを構成・ビルド・実行
- **`build.cmd benchmark`**: Google Benchmark ターゲットを構成・ビルド・実行
- **`build.cmd benchmark --filter CoverExtractor/all_images`**: 特定の benchmark ケースのみ実行

ビルド後の実行ファイルは `build\ninja-release\` または `build\ninja-debug\` に生成されます。

### テスト

- 単体テストと結合テストは `tests/` 配下にあります
- 基本コマンド：

```bat
build.cmd tests
```

- 現在の主な対象：
  - 検索サジェストと canonical 検索語変換
  - ダウンロード状態とファイル処理
  - アプリ更新、DB 更新、SQLite 検証

### Benchmark

- 性能ベンチマークは `tests/performance/` にあります
- 現在は `CoverExtractor` の benchmark を用意しており、サンプル画像は `tests/resources/fling_trainer_screenshot/` にあります
- 基本コマンド：

```bat
build.cmd benchmark
```

- 特定ケースの実行：

```bat
build.cmd benchmark --filter CoverExtractor/.*
```

## 翻訳データベース

- アプリは外部 SQLite データベース `fling_translations.db` を使用します
- リリースパッケージにはアプリ配下の `resources/` に同梱版が含まれます
- DB 更新後は AppData の上書き先に保存され、アプリは同梱版と上書き版のバージョンを比較して、新しい有効な方を使用します
- 有効な DB には以下が必要です：
  - `metadata.release_tag`
  - `metadata.schema_version`
  - `games.english`
  - `games.normalized_english`
  - `games.chinese_simplified`
  - `games.japanese`

## CI / リリース

- `build.yml` はビルドとテストを実行します
- `make-release.yml` はインストーラーとポータブル版を生成し、`fling_translations.db` を同梱します
- インストーラー版とポータブル版の両方に、ランチャー、本体、外部リソースディレクトリが含まれます

## セキュリティとプライバシー

- Windows Defender による誤検知（例: `Win32/Wacapew.C!ml`）が発生する場合がありますが、誤検知です。
- 個人情報は収集しません。ネットワーク通信は検索、ダウンロード、更新確認のみに使用され、設定と DB 上書きファイルはローカルにのみ保存されます。

## ライセンスとフィードバック

- ライセンス: GNU AGPL v3（[LICENSE](../LICENSE)）
- フィードバック: GitHub の [Issues](../../issues)
