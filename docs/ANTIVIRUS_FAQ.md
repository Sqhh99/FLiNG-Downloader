# 杀毒软件误报 FAQ

## 结论

- 该项目为开源软件，许可证为 GNU AGPL v3。
- Windows Defender 等杀毒软件可能出现启发式误报。
- 误报不等于存在恶意代码，请先校验来源与完整性。

## 常见误报原因

- 程序包含下载、解压、批量文件写入等行为。
- 可执行文件为静态链接，体积与特征更容易触发模型。
- 新版本样本量少，信誉分尚未建立。

## 推荐处理流程

1. 仅从官方发布页下载：
   https://github.com/Sqhh99/FLiNG-Downloader/releases
2. 校验文件完整性（如 Release 提供哈希值则对比）。
3. 将程序目录加入 Defender 排除项后再运行。
4. 如仍被拦截，向 Microsoft 提交误报样本。

## Windows Defender 排除项

1. 打开“Windows 安全中心” → “病毒和威胁防护”。
2. 进入“病毒和威胁防护设置”。
3. 打开“添加或删除排除项”。
4. 添加本项目所在文件夹。

## 提交误报（Microsoft）

提交地址：
https://www.microsoft.com/en-us/wdsi/filesubmission

建议附带信息：

- 检测名称（例如 `Win32/Wacapew.C!ml`）
- 系统版本与 Defender 引擎版本
- 文件来源（官方 Release 链接）
- 复现步骤与截图

## 其他杀毒软件

- 通用做法：将程序目录加入“排除/白名单/例外”。
- 若不确定入口，请搜索对应产品关键词：`exclude folder` / `allow list`。

## 安全与隐私说明

- 不收集个人身份信息。
- 网络请求仅用于搜索与下载。
- 配置、缓存、日志仅本地存储。

## 反馈渠道

- 安全问题（私密）：
  https://github.com/Sqhh99/FLiNG-Downloader/security/advisories/new
- 一般问题：
  https://github.com/Sqhh99/FLiNG-Downloader/issues

最后更新：2026-03-01
