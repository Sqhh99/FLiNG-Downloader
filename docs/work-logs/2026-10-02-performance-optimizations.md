# 工作记录：下载并行队列、启动提速与封面加载提速

- **日期：** 2026-10-02
- **分支：** `perf/download-startup-cover`（基于 `main` 的 `9878c4a`）
- **关联文档：** [PR 记录](../pull-requests/2026-10-02-download-startup-cover.md)

## 一、用户的请求

> Please look into the project and see if the current download speed of the trainer, the
> software startup speed, and the cover image loading speed can be optimized.

先在计划模式下排查三条路径，再按批准的计划实现。排查中就"下载速度"问了一个范围问题：
单个文件的传输本身已经是一条 HTTP/1.1 流直接写盘，瓶颈是网络；真正能提速的只有改结构。
用户选择了 **"Add parallel queue"**（同时下载 2–3 个修改器），没有选分段（多 Range）下载。

## 二、制定的计划

排查结论（改动前的状态）：

- **下载：** `DownloadManager::m_isDownloading` 拒绝第二个下载，`Backend` 只记一个
  `m_activeDownloadTaskId`，所以队列严格串行。
- **启动：**
  - `fetchRecentModifiers()` 在 flingtrainer.com 首页返回前列表一直为空；
    `recent_modifiers_cache.json` 只在 3 次失败（每次最长 30 秒超时）之后才作为兜底。
  - 翻译库在 `GameMappingManager::initialize()` 和 `Backend::loadGameMappings()` 各全表读一次。
  - `TranslationDatabase::resolveDatabasePath()` 每次调用都打开并校验两份库（约 6 次 SQLite
    open），启动期间调用 3 次以上。
- **封面：**
  - 流程完全串行：详情页 → 截图 → 解码 → YOLO；模型在第一次取封面时才加载（9.8 MB ONNX，
    且开了 `ORT_ENABLE_ALL` 图优化）。
  - 搜索/首页列表行里已经解析出了 `screenshotUrl`（`ModifierParser.cpp:183`），但没用上。
  - `getCachedCover()` 只为判断"有没有缓存"就在 GUI 线程完整解码 PNG，每次选中做两遍。
  - `saveCoverToCache()` 在 GUI 线程做 PNG 编码。
  - QML `Image` 对 `file:///` 是同步解码。
  - 模型识别失败不会被记住，同一张截图每次选中都重新下载、重新推理。

按顺序执行：

1. 下载队列改为最多 3 个并发。
2. 启动：先显示缓存列表再后台刷新；缓存翻译库路径和全表读取结果。
3. 封面：缓存判断改为文件存在检查；用列表行截图提前开始取封面；启动后后台预热模型；
   PNG 编码移到工作线程；记住"识别不到封面"的截图；QML 异步解码；截图请求加超时。
4. 更新/新增单元测试，写本工作记录。

## 三、具体改了哪些文件

| 文件 | 改动 | 对应问题 |
|------|------|----------|
| `src/include/DownloadManager.h`、`src/DownloadManager.cpp` | `m_isDownloading`/`m_currentSavePath` 换成 `QHash<QString, quint64> m_activeDownloads`（按目标路径记录，值是每次启动的 token）。不同路径可并发，同一路径仍返回 "Download already in progress"。新增 `cancelDownload(savePath)`，原 `cancelDownload()` 变为"全部取消"。token 用于防止已取消传输的迟到回调把复用同一路径的新传输注销掉。 | 下载串行 |
| `src/include/Backend.h`、`src/Backend.cpp`（下载部分） | `m_activeDownloadTaskId` → `QSet m_activeDownloadTaskIds`，上限 `kMaxConcurrentDownloads = 3`；测速改为每个任务一个 `SpeedSample`。`processNextDownloadTask()` 先对排队任务拍快照再逐个启动，直到占满名额（任务可能在 `startDownloadTask()` 里同步结束并重入）。同一版本被排队两次时共用同一个 `.crdownload`，第二个会等第一个结束（`isTempPathInUse()`）；取消这种重复的排队任务时也不再删掉正在下载那份的临时文件。暂停/取消只取消对应任务的 `tempPath`。结束收尾集中到 `finishActiveDownloadTask()`：只有全部任务结束才停计时器。`resumeDownload()` 统一改为置回 `queued` 再交给调度。删掉了没人读的 `m_isDownloading`。 | 下载串行 |
| `src/include/SearchManager.h`、`src/SearchManager.cpp` | `fetchRecentlyUpdatedModifiers()` 有缓存时立即回调缓存列表，再请求首页；网络结果与缓存一致时不再重复回调（避免把用户正在看的列表重置），网络全部失败时保留已显示的缓存。`Backend` 侧的 request id 机制保证用户发起搜索后，迟到的首页结果会被丢弃。 | 启动时列表为空 |
| `src/include/TranslationDatabase.h`、`src/TranslationDatabase.cpp` | `databasePath()` 和 `loadAllGames()` 的结果按"两份候选文件的路径 + 修改时间 + 大小"缓存，文件在磁盘上变化就自动失效；`installOverrideDatabase()` 换文件后显式失效（Windows 上 `QFile::copy` 会保留源文件时间戳，不能只靠时间戳判断）。这样 `GameMappingManager` 和 `Backend` 两处全表读取只真正查一次库。 | 启动时重复打开 SQLite |
| `src/include/CoverExtractor.h`、`src/CoverExtractor.cpp` | `extractCoverFromTrainerImage()` 换成 `extractCoverToCache()`：解码、推理和 PNG 写盘都在工作线程完成，用 `QSaveFile` 原子写入。结果分为 `Saved`/`NoCover`/`Failed`。只有模型确实没检测到时才写 `<gameId>.game-cover-v2.nocover` 标记（内容是截图 URL 列表）；网络、解码、模型缺失、推理异常都算 `Failed`，下次还会重试。标记文件名带模型名，换模型会自动重新识别。新增 `hasCachedCover()`（只看文件是否存在且非空）、`cachedCoverPath()`、`isKnownWithoutCover()`、`warmUpModelAsync()`。截图请求加了 30 秒 `setTransferTimeout`。删掉了不再有调用方的 `getCachedCover()`/`saveCoverToCache()`/`extractCoverImageFromData()`。 | 封面慢、GUI 线程阻塞 |
| `src/Backend.cpp`（封面部分） | `selectModifier()` 用 `hasCachedCover()` 判断缓存；没缓存时直接用列表行的 `screenshotUrl` 开始取封面（`startCoverFetch()`），不必等详情页。详情页返回后，只有在截图 URL 不同且还没有封面时才重新取。回调同时校验 `gameId` 和截图 URL，被替换的请求结果会被丢弃。详情页失败或无详情 URL 时，如果列表截图还在处理，就不提前关掉加载状态。构造后 1 秒在后台预热检测模型。 | 封面慢 |
| `qml/components/DetailDrawer.qml` | 封面 `Image` 加 `asynchronous: true`。没有设 `sourceSize`，因为 `coverAR` 依赖原始尺寸。 | 封面解码卡 GUI |
| `tests/unit/download_manager_test.cpp` | 原 `ConcurrentDownloadRequestsAreRejected` 改为三个用例：不同路径可并发、同一路径被拒、取消其中一个不影响另一个。 | — |
| `tests/unit/translation_database_test.cpp` | 新增 `CachedResolutionFollowsOverrideChangesOnDisk`：缓存存在时，增删 override 文件仍能被察觉，路径和游戏列表都跟着变。 | — |

## 四、验证情况

在 WSL 中通过 `cmd.exe` 调用（需要先 `call vcvars64.bat`，否则 MSVC 找不到标准头文件）：

- **已运行** `build.cmd tests`：构建通过，ctest `100% tests passed, 0 tests failed out of 1`。
  第一次构建出现 1 个新警告（`DownloadManager.cpp(154)` C4804：Qt 6 的 `QHash::remove`
  返回 `bool`，不能和 `> 0` 比较），修复后重新构建，警告消失，测试仍然全部通过。
- **已运行** 测试程序本体 `FLiNG Downloader Tests.exe`：`[  PASSED  ] 39 tests.`，
  其中包括新增的 `DownloadsToDifferentPathsRunConcurrently`、
  `SecondDownloadToTheSamePathIsRejected`、`CancellingOneDownloadLeavesTheOthersRunning`、
  `CachedResolutionFollowsOverrideChangesOnDisk`。
- **已运行** `build.cmd`（Release）：应用本体构建通过（`[OK] Build complete.`）。
- **未运行** `build.cmd benchmark --filter CoverExtractor/.*`：需要单独以
  `-DFLING_BUILD_BENCHMARKS=ON` 配置，本次没有做。推理路径（`extractCoverByModel`）只多了
  一个可选出参，benchmark 走的 `extractCoverFromLocalImage` 没有变化。
- **未做** 手动界面验证，需要用户在 Windows 上实际操作：
  - 有缓存时冷启动，列表应立即出现；
  - 连续加入 4 个下载，应有 3 个同时处于"下载中"，第 4 个排队；暂停/取消其中一个，其他继续；
  - 选择一个没有缓存封面的游戏，封面应比之前更早出现；再次选择应立即显示；
  - 识别不到封面的游戏，第二次选择时不应再发起截图请求（看日志）。

## 五、遗留事项

- **未做分段下载**：用户选择了并行队列，单文件仍是一条连接。
- **未加自动化测试的部分**：`SearchManager` 的"先缓存后网络"两次回调，以及 `Backend` 的
  并发调度/封面提前获取，都没有新的单元测试。计划里列的 `SearchManager` 用例没有写，原因是
  缓存文件位于真实数据目录（`FileSystem::getDataDirectory()`），没有测试替身，写测试就会动到
  用户的真实缓存。
- **预热的代价**：后台预热会在启动约 1 秒后把 ONNX Runtime 和模型载入内存，即使用户这次
  不看任何封面。
- **"无封面"标记不会过期**：除非截图 URL 变了或换了模型，同一张截图不会再识别第二次。
  清空封面缓存目录可以重置。
- **列表截图与详情截图可能不一致**：列表行截图取的是文章里第一张 `.jpg/.png/.gif`。如果
  站点在列表里用的是占位图或缩略图，提前取封面会白跑一次，之后由详情页的 URL 纠正。
- `build.cmd i18n` 未执行：本次没有新增或修改用户可见文案。
