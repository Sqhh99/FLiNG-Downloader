<div align="center">
  <img src="https://github.com/user-attachments/assets/e8aceb6b-2534-4aaf-a757-020b654aa285" alt="FLiNG Downloader のロゴ" width="120">

  <h1>FLiNG Downloader</h1>

  <p>
    <a href="https://flingtrainer.com/">FLiNG</a> のゲームトレーナーを Windows で検索・ダウンロード・管理。<br>
    ゲーム名は日本語・英語・中国語で検索できます。
  </p>

  <p>
    <a href="https://github.com/Sqhh99/FLiNG-Downloader/releases/latest"><img alt="最新リリース" src="https://img.shields.io/github/v/release/Sqhh99/FLiNG-Downloader"></a>
    <a href="https://github.com/Sqhh99/FLiNG-Downloader/releases"><img alt="総ダウンロード数" src="https://img.shields.io/github/downloads/Sqhh99/FLiNG-Downloader/total"></a>
    <a href="../LICENSE"><img alt="ライセンス：AGPL-3.0" src="https://img.shields.io/badge/license-AGPL--3.0-blue"></a>
    <img alt="対応環境：Windows 10+ x64" src="https://img.shields.io/badge/platform-Windows%2010%2B%20x64-0078D6">
  </p>

  <p><a href="../README.md">English</a> · <a href="README.zh-CN.md">简体中文</a> · <b>日本語</b></p>
</div>

FLiNG Downloader は flingtrainer.com 向けのオープンソースのデスクトップクライアントです。ゲームを検索し、トレーナーのオプションや対応ゲームバージョンを確認して、ワンクリックでダウンロードできます。ダウンロードしたトレーナーはアプリ内でまとめて管理できます。Rust 製のネイティブアプリで、画面は GPU で描画されます。ランタイムのインストールは不要です。

> [!NOTE]
> 本プロジェクトは独立したオープンソースのツールであり、FLiNG および flingtrainer.com **とは無関係**です。トレーナー本体をホストしたり再配布したりはせず、公開サイトを検索して、指定されたものだけをダウンロードします。トレーナーはゲームの利用規約に反する場合があるため、ご利用は自己責任でお願いします。

![FLiNG Downloader の検索結果とトレーナー詳細](../resources/interface.png)

## 目次

- [機能](#機能)
- [ダウンロードとインストール](#ダウンロードとインストール)
- [使い方](#使い方)
- [プライバシーとセキュリティ](#プライバシーとセキュリティ)
- [ソースからのビルド](#ソースからのビルド)
- [サポートと開発への参加](#サポートと開発への参加)
- [ライセンス](#ライセンス)

## 機能

- **日本語で検索。** ゲーム名を日本語・英語・中国語で入力できます。同梱のタイトルデータベースが、日本語・中国語のタイトルを flingtrainer.com の英語タイトルに変換します。タイトルの一部でも検索でき、「バイオハザード」や「生化危机」でバイオハザードシリーズのトレーナーが見つかります。入力中には候補も表示されます。
- **トレーナーの詳細をひと目で。** すべてのオプションとホットキー、対応ゲームバージョン、最終更新日、ダウンロード可能な全バージョンを表示します。ゲームのカバーは、ローカルの ONNX モデルがトレーナーのスクリーンショットから自動で切り出します。
- **ダウンロード管理。** 最大 3 件を同時にダウンロードでき、一時停止・再開・再試行・キャンセルが可能です。中断したダウンロードは続きから再開します。ファイルの拡張子（`.zip`、`.exe`、`.rar` など）は中身に合わせて自動で修正されます。
- **ダウンロード済み一覧。** 「ダウンロード済みMOD」タブに、これまでにダウンロードしたトレーナーが並びます。ここから起動、フォルダーを開く、削除ができます。
- **トレーナー名のローカライズ。** トレーナー名を日本語または中国語で表示できます（例：「エルデンリング (Elden Ring)」）。新しくダウンロードしたファイルも同じ名前で保存されます。
- **好みに合わせた画面。** 表示言語は日本語・英語・簡体字中国語から選べ、即座に切り替わります。テーマはライト、ダーク、オーシャン、夕焼け、森林、ラベンダー、ローズ、深夜、モカの 9 種類です。
- **更新機能を内蔵。** アプリの新バージョンとタイトルデータベースの更新を GitHub または Gitee で確認し、設定画面からインストールできます。

## ダウンロードとインストール

最新版は [Releases ページ](https://github.com/Sqhh99/FLiNG-Downloader/releases/latest)から入手できます。

| ファイル | 内容 |
|----------|------|
| `FLiNG-Downloader-v<バージョン>-win-x64-setup.exe` | **インストーラー。** 旧バージョンに上書きでき、設定・ダウンロード済みトレーナー・カバーは保持されます。 |
| `FLiNG-Downloader-v<バージョン>-win-x64-portable.zip` | **ポータブル版。** 任意の場所に展開して `FLiNG Downloader.exe` を実行します。 |
| `SHA256SUMS.txt` | 上記 2 ファイルの SHA-256 チェックサム。 |

**動作環境：** Windows 10 バージョン 1903 以降、64 ビット。ほかに必要なものはありません。ONNX Runtime は実行ファイルに組み込まれており、どちらのパッケージにも MSVC ランタイムが含まれています。

**ダウンロードしたファイルの検証：** `SHA256SUMS.txt` と照合します。PowerShell の例：

```powershell
Get-FileHash .\FLiNG-Downloader-v<バージョン>-win-x64-setup.exe -Algorithm SHA256
```

> [!TIP]
> Windows Defender などのウイルス対策ソフトが本アプリを誤検知することがあります（例：`Win32/Wacapew.C!ml`）。理由と対処法は [ウイルス対策ソフトの誤検知 FAQ](ANTIVIRUS_FAQ.md)（英語）を参照してください。ダウンロードは本リポジトリの Releases ページからのみ行ってください。

**1.1.x 以前（Qt 版）からのアップグレード：** 同じファイルを読み込むため、設定、ダウンロード済み一覧、カバーキャッシュ、タイトルデータベースはそのまま引き継がれます。

## 使い方

1. **検索する。** ゲーム名を入力して Enter を押すか、候補を選びます。最近更新されたトレーナーを見るには「全て表示」をクリックするか、入力欄を空にしたまま Enter を押します。
2. **トレーナーを選ぶ。** 結果をクリックすると、オプション、対応バージョン、カバーが表示されます。
3. **ダウンロードする。** バージョンを選んで「ダウンロード」をクリックします。進捗はタイトルバーの「ダウンロードリスト」に表示されます。
4. **起動する。** 「ダウンロード済みMOD」タブからトレーナーを開きます。

「設定」では、ダウンロード先フォルダー（既定はユーザーの「ダウンロード」フォルダー）、表示言語とトレーナー名の言語、テーマ、更新ソースを変更できます。

**ゲームが見つからない、または名前の翻訳が間違っている場合：** まずタイトルデータベースを更新してください：**設定 → このアプリについて → 翻訳データベース更新 → データベース更新を確認**。それでも解決しない場合は、[*Game title not found* フォーム](https://github.com/Sqhh99/FLiNG-Downloader/issues/new?template=game-title.yml)から報告してください。

## プライバシーとセキュリティ

- 個人情報は一切収集せず、テレメトリもありません。ネットワークに接続するのは、flingtrainer.com の検索、トレーナーとカバーのダウンロード、GitHub または Gitee での更新確認のときだけです。
- データはすべてお使いの PC に保存されます。
  - 設定：`%APPDATA%\FLiNG Downloader\config`
  - ダウンロード済み一覧と更新後のタイトルデータベース：`%APPDATA%\FLiNG Downloader\data`
  - カバーキャッシュ：`%LOCALAPPDATA%\FLiNG Downloader\cache`
- 脆弱性の報告方法は [SECURITY.md](../SECURITY.md)（英語）を参照してください。公開の Issue には書かないでください。

## ソースからのビルド

Rust stable（`x86_64-pc-windows-msvc`）と Visual Studio 2022 以降の C++ ビルドツールが必要です。Qt・CMake・vcpkg は不要です。ONNX Runtime は初回ビルド時に自動でダウンロードされ、静的リンクされます。

```bat
git clone --recurse-submodules https://github.com/Sqhh99/FLiNG-Downloader.git
cd FLiNG-Downloader

cargo run -p fling-ui                   :: ビルドして実行（Debug）
cargo test --workspace                  :: すべてのテスト
cargo clippy --workspace --all-targets -- -D warnings
cargo fmt --all
cargo bench -p fling-cover --bench detector   :: カバー検出のベンチマーク
cargo xtask dist [--version 1.2.3]      :: dist\FLiNG Downloader\ にリリース用フォルダーを作成
cargo xtask notices                     :: THIRD_PARTY_NOTICES.md の依存一覧を再生成
```

`fling-ui` の初回ビルドには数分かかります。

<details>
<summary><b>プロジェクト構成</b></summary>

フロントエンドとバックエンドを分離した Cargo workspace です。UI は `Command` と `Event` だけでバックエンドとやり取りします。

| Crate | 役割 |
|-------|------|
| `fling-core` | ドメイン型と純粋関数：バージョン比較、タイトル正規化、ファイル種別判定 |
| `fling-net` | HTTP 抽象と reqwest 実装（レジューム、Referer、タイムアウト）、テスト用のフェイク |
| `fling-config` | AppData のパスと QSettings 互換の `settings.ini` |
| `fling-mapping` | 翻訳データベース、日本語・中国語 → 英語のタイトル変換、検索候補 |
| `fling-site` | flingtrainer.com のページ解析、検索、最近の更新一覧 |
| `fling-download` | ダウンロードキュー（一時停止・再開・同時 3 件）とダウンロード済み一覧 |
| `fling-update` | GitHub または Gitee からのアプリ・データベース更新 |
| `fling-cover` | カバーキャッシュと ONNX カバー検出 |
| `fling-app` | バックエンドの窓口：全サービスを保持し、`Command` / `Event` を公開 |
| `fling-ui` | GPUI の画面（`FLiNG Downloader.exe`） |
| `xtask` | パッケージングとリポジトリ作業 |

</details>

<details>
<summary><b>テスト</b></summary>

- 各 crate に単体テストがあります。結合テストは `crates/fling-app/tests`、`crates/fling-site/tests`、`crates/fling-mapping/tests`、`crates/fling-cover/tests` にあります。
- テストは外部ネットワークにも実際の AppData にも触れません。フェイクの HTTP クライアントと一時ディレクトリを使います。
- `crates/fling-site/tests/fixtures/` にサイトのページを保存しています。`crates/fling-cover/tests/screenshots/` はカバー検出用のサンプルです。

</details>

<details>
<summary><b>翻訳データベース</b></summary>

- アプリはゲームタイトルを SQLite データベース `fling_translations.db` から読み込みます。アプリの `resources/` フォルダーに同梱版があります。
- データベースの更新は `%APPDATA%\FLiNG Downloader\data` に保存されます。アプリはそれを同梱版と比較し、新しい有効な方を使います。
- データベースは [game-mappings-updater](https://github.com/Sqhh99/game-mappings-updater) で生成・公開されます。サブモジュールとして `tools/game-mappings-updater` に配置されています（`git submodule update --init` で取得）。
- 有効なデータベースには次の列が必要です。
  - `metadata.release_tag`
  - `metadata.schema_version`（任意。存在する場合は `1` であること）
  - `games.english`、`games.normalized_english`、`games.chinese_simplified`、`games.japanese`

</details>

<details>
<summary><b>CI とリリース</b></summary>

- `build.yml` はフォーマット確認、clippy、テストを実行し、パッケージを作成します。
- `make-release.yml` はインストーラーとポータブル版を作成し、`fling_translations.db` と `SHA256SUMS.txt` と一緒に GitHub Release に添付します。
- どちらのパッケージにも、本体、カバーモデル（`models/`）、同梱データベース（`resources/`）、MSVC ランタイムが含まれます。
- `v` で始まる tag を push すると `make-release.yml` が実行されます。`-` を含まない tag は正式版、`-` を含む tag は GitHub のプレリリースになります。

  ```bash
  git tag v1.2.3 && git push origin v1.2.3                 # 正式版
  git tag v1.2.3-beta.1 && git push origin v1.2.3-beta.1   # プレリリース
  ```

</details>

## サポートと開発への参加

- **使い方の質問とアイデア：** [Discussions](https://github.com/Sqhh99/FLiNG-Downloader/discussions)
- **不具合と合意済みの機能：** [Issues](https://github.com/Sqhh99/FLiNG-Downloader/issues)。新機能は、まず Discussions で提案してください。
- **コードでの貢献：** まず [CONTRIBUTING.md](../CONTRIBUTING.md)（英語）をお読みください。
- **脆弱性の報告：** [Security Advisory](https://github.com/Sqhh99/FLiNG-Downloader/security/advisories/new) から非公開で報告してください。詳しくは [SECURITY.md](../SECURITY.md) を参照してください。

## ライセンス

FLiNG Downloader は [GNU AGPL v3](../LICENSE) で提供されています。サードパーティ製コンポーネントとそのライセンスは [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) に記載しています。
