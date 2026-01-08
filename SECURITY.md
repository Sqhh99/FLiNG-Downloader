# 安全政策 (Security Policy)

## 支持的版本 (Supported Versions)

当前维护的版本：

| 版本   | 支持状态    |
| ------ | ---------- |
| 最新版 | ✅ 支持     |
| 开发版 | ⚠️ 测试版   |

## 报告安全漏洞 (Reporting a Vulnerability)

如果您发现安全漏洞，请通过以下方式报告：

1. **GitHub Issues**: 创建安全相关的 Issue
2. **邮件联系**: 通过项目维护者邮箱私密报告
3. **负责任披露**: 我们承诺在合理时间内响应和修复

## Windows Defender 误报处理

### 常见误报类型

- `Win32/Wacapew.C!ml` - 启发式检测误报
- `Trojan:Win32/Zpevdo.B` - 网络行为误报  
- `PUA:Win32/Presenoker` - 潜在不需要应用误报

### 技术原因分析

1. **网络请求模式**
   - 软件使用 CURL 库进行 HTTP/HTTPS 请求
   - 用户代理字符串可能触发检测
   - 并发下载行为模式

2. **文件操作行为**
   - 自动创建下载目录
   - 解压缩档案文件
   - 修改文件权限和属性

3. **静态链接特征**
   - 包含完整的依赖库
   - 可执行文件体积较大
   - 代码熵值可能异常

### 减少误报的技术措施

#### 1. 代码签名
```bash
# 使用代码签名证书对可执行文件签名
signtool sign /f certificate.pfx /p password /t http://timestamp.server.com program.exe
```

#### 2. 优化网络请求
```cpp
// 在 NetworkManager 中使用标准 User-Agent
request.setRawHeader("User-Agent", "FLiNG Downloader/1.0 (Windows NT 10.0; Win64; x64)");

// 添加延迟避免过于频繁的请求
QTimer::singleShot(100, [this, request]() {
    manager->get(request);
});
```

#### 3. 文件操作优化
```cpp
// 使用 Windows API 进行文件操作，提高兼容性
#ifdef _WIN32
#include <windows.h>
#include <fileapi.h>

bool createDirectorySecurely(const QString& path) {
    std::wstring wpath = path.toStdWString();
    return CreateDirectoryW(wpath.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}
#endif
```

#### 4. 添加元数据信息
在 `icon.rc` 中添加详细的版本信息：

```rc
VS_VERSION_INFO VERSIONINFO
FILEVERSION 1,0,0,0
PRODUCTVERSION 1,0,0,0
FILEFLAGSMASK 0x3fL
FILEFLAGS 0x0L
FILEOS 0x40004L
FILETYPE 0x1L
FILESUBTYPE 0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904b0"
        BEGIN
            VALUE "CompanyName", "FLiNG Downloader Project"
            VALUE "FileDescription", "Game Modifier Download Manager"
            VALUE "FileVersion", "1.0.0.0"
            VALUE "InternalName", "FLiNG Downloader"
            VALUE "LegalCopyright", "Copyright (C) 2025. Licensed under MIT."
            VALUE "OriginalFilename", "FLiNG Downloader.exe"
            VALUE "ProductName", "FLiNG Downloader"
            VALUE "ProductVersion", "1.0.0.0"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1200
    END
END
```

## 隐私保护声明

### 数据收集
- ✅ **不收集**个人身份信息
- ✅ **不收集**浏览历史或使用习惯
- ✅ **不上传**任何用户文件
- ✅ **仅本地存储**配置和缓存

### 网络通信
- 🌐 仅用于修改器搜索和下载
- 🔒 支持 HTTPS 安全连接
- 📝 所有请求记录仅存储在本地日志

### 第三方服务
- 🔍 **搜索服务**: 仅发送游戏名查询
- 📥 **下载服务**: 仅下载用户选择的文件
- 🌍 **翻译服务**: 仅翻译游戏名称（可选）

## 源码审计

### 关键安全检查点

1. **网络请求处理** (`src/NetworkManager.cpp`)
   - SSL/TLS 证书验证
   - 请求超时设置
   - 错误处理机制

2. **文件下载逻辑** (`src/DownloadManager.cpp`)
   - 文件完整性验证
   - 安全的文件路径处理
   - 防止目录遍历攻击

3. **配置文件处理** (`src/ConfigManager.cpp`)
   - 输入验证和清理
   - 防止注入攻击
   - 安全的序列化/反序列化

### 编译时安全选项

```cmake
# 启用安全编译选项
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /GS      # 栈保护
        /DYNAMICBASE  # ASLR 支持
        /NXCOMPAT     # DEP 支持
        /guard:cf     # 控制流保护
    )
    
    target_link_options(${PROJECT_NAME} PRIVATE
        /SAFESEH      # 安全异常处理
        /LARGEADDRESSAWARE  # 大地址感知
    )
endif()
```

## 漏洞报告模板

```markdown
### 漏洞描述
简要描述发现的安全问题

### 影响范围
- 受影响的版本
- 潜在的安全风险级别

### 复现步骤
1. 步骤一
2. 步骤二
3. ...

### 建议修复方案
如有建议的修复方法，请提供

### 联系信息
如需私密沟通，请提供联系方式
```

---

**最后更新**: 2024年
**联系方式**: 通过 GitHub Issues 或项目维护者 