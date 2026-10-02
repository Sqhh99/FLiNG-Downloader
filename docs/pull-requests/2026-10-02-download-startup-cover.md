# PR：下载并行队列、启动提速与封面加载提速

- **日期：** 2026-10-02
- **分支：** `perf/download-startup-cover` → `main`
- **基线提交：** `9878c4a`
- **关联记录：** [工作记录](../work-logs/2026-10-02-performance-optimizations.md)

## 关联

没有对应 Issue。来源是作者的请求：排查修改器下载速度、软件启动速度、封面加载速度能否优化。

## 改了什么

**下载**

- 下载队列最多同时下载 **3 个**修改器，原来是严格一个接一个。暂停/取消其中一个不影响其他任务。
- 同一版本被排队两次时，第二个会等第一个结束（两者共用同一个 `.crdownload` 文件）。
- 单个文件仍是一条连接，没有做分段下载。单文件的传输本身没有代码瓶颈，速度取决于网络。

**启动**

- 首页列表启动时**立即**显示上次缓存的结果，再在后台刷新。原来要等 flingtrainer.com
  返回才有内容，网络差时最长要等 3 次 × 30 秒超时。网络结果与缓存相同时不重复刷新，
  避免把用户正在看的列表重置。
- 翻译库的路径解析和全表读取结果会被缓存。原来启动时会打开约十几次 SQLite，全表读两遍。
  缓存按两份候选库文件的路径、修改时间和大小失效，安装数据库更新后也会显式失效。

**封面**

- 选中游戏后直接用列表行里已有的截图地址开始取封面，不再先等详情页返回。只有详情页给出
  不同的截图时才改用详情页的。
- 封面检测模型在启动约 1 秒后于后台预加载，第一次取封面不再额外等模型加载。
  代价：即使这次不看封面，模型也会被载入内存。
- 判断"有没有缓存封面"只检查文件，不再在 UI 线程解码整张 PNG。
- 新封面的 PNG 编码和写盘移到工作线程，用 `QSaveFile` 原子写入。
- 详情面板的封面 `Image` 改为异步解码（`asynchronous: true`）。
- 模型确实识别不到封面的截图会被记住（按截图 URL 和模型版本），之后不再重复下载和推理。
  网络、解码、模型加载或推理出错都不会被记住，下次仍会重试。
- 截图请求加了 30 秒超时，不会再无限转圈。

**没有改动：** 翻译库内容、i18n 文案、模型文件、打包资源。

## 怎么验证

- [x] `build.cmd tests`
- [ ] 本地跑过相关界面 / 下载 / 搜索路径

`build.cmd tests` 由 AI 在 WSL 里通过 `cmd.exe` 调用（先 `call vcvars64.bat`）实际运行：
ctest 全部通过，直接运行测试程序为 `[  PASSED  ] 39 tests.`。`build.cmd`（Release）也已运行，
应用本体构建通过。新增 / 改写的测试：

- `DownloadManagerTest.DownloadsToDifferentPathsRunConcurrently`
- `DownloadManagerTest.SecondDownloadToTheSamePathIsRejected`
- `DownloadManagerTest.CancellingOneDownloadLeavesTheOthersRunning`
- `TranslationDatabaseTest.CachedResolutionFollowsOverrideChangesOnDisk`

**界面实测未做**，合并前需要作者在 Windows 上补测：

1. 有缓存时冷启动，列表应立即出现。
2. 连续加入 4 个下载：3 个同时下载，第 4 个排队；暂停/取消其中一个，其他继续。
3. 选择一个没有缓存封面的游戏，封面应比之前更早出现；再次选择应立即显示。
4. 识别不到封面的游戏，第二次选择时日志里不应再出现截图请求。

另外没有运行 `build.cmd benchmark --filter CoverExtractor/.*`，因为它需要单独开启 benchmark
配置。benchmark 走的 `extractCoverFromLocalImage` 路径没有改动。以下行为没有新的自动化测试：
`SearchManager` 的"先缓存后网络"两次回调、`Backend` 的并发调度和封面提前获取。

## 检查项

- [x] 已阅读 [CONTRIBUTING.md](https://github.com/Sqhh99/FLiNG-Downloader/blob/main/CONTRIBUTING.md)
- [ ] UI / QML 改动附了截图：不适用。唯一的 QML 改动是封面图片改为异步解码，没有可见的布局变化。
- [x] 若改动了翻译库、i18n、模型或打包资源，已在上文写明（本次均未改动）
- [x] AI 使用披露：是。排查、代码改动、单元测试与本记录均由 Claude Code 生成；构建和单元测试
  也由 Claude Code 在本机运行。界面实测尚未进行。
