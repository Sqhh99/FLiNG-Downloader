#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <QDateTime>
#include <QSettings>
#include <functional>
#include "ModifierParser.h"
#include "FileSystem.h"
#include "UpdateManager.h"
#include "DownloadManager.h"
#include "ModifierInfoManager.h"
#include "SearchManager.h"

// 已下载修改器信息结构
struct DownloadedModifierInfo {
    QString name;
    QString version;
    QString filePath;
    QDateTime downloadDate;
    QString gameVersion;
    int optionsCount;
    QString url;
    bool hasUpdate;
};

// 排序类型枚举
enum class SortType {
    ByUpdateDate,
    ByName,
    ByOptionsCount
};

// 修改器查找回调函数类型
using ModifierFoundCallback = std::function<void(const QList<ModifierInfo>&)>;
using ModifierDetailCallback = std::function<void(ModifierInfo*)>;
using ModifierDownloadFinishedCallback = std::function<void(bool, const QString&, const QString&, const ModifierInfo&, bool)>;
using UpdateProgressCallback = std::function<void(int, int, bool)>; // 当前进度, 总数, 是否有更新

class ModifierManager : public QObject
{
    Q_OBJECT

public:
    static ModifierManager& getInstance();

    // 搜索修改器 - 使用SearchManager实现
    void searchModifiers(const QString& searchTerm, ModifierFoundCallback callback);
    
    // 直接设置修改器列表 - 供SearchManager调用
    void setModifierList(const QList<ModifierInfo>& modifiers);
    
    // 获取修改器详情
    void getModifierDetail(const QString& url, ModifierDetailCallback callback);
    
    // 下载修改器 - 使用DownloadManager实现
    void downloadModifier(const ModifierInfo& modifier, 
                         const QString& version, 
                         const QString& savePath,
                         ModifierDownloadFinishedCallback callback,
                         DLProgressCallback progressCallback);
    
    // 已下载修改器管理
    QList<DownloadedModifierInfo> getDownloadedModifiers() const;
    void addDownloadedModifier(const ModifierInfo& info, const QString& version, const QString& filePath);
    void removeDownloadedModifier(int index);
    
    // 更新检查 - 使用UpdateManager实现
    void checkForUpdates(int index = -1, std::function<void(bool)> callback = nullptr);
    void batchCheckForUpdates(UpdateProgressCallback progressCallback, 
                              std::function<void(int)> completedCallback);
    void setModifierUpdateStatus(int index, bool hasUpdate);
    
    // 修改器列表排序与过滤 - 使用SearchManager和ModifierInfoManager实现
    void sortModifierList(SortType sortType);
    QList<ModifierInfo> getSortedModifierList() const;
    QList<ModifierInfo> filterModifiersByKeyword(const QString& keyword) const;
    
    // 保存和加载已下载修改器列表 - 使用ModifierInfoManager的JSON功能
    void saveDownloadedModifiers();
    void loadDownloadedModifiers();
    
    // 导出和导入修改器信息 - 使用ModifierInfoManager实现
    bool exportModifierToFile(const ModifierInfo& info, const QString& filePath);
    ModifierInfo importModifierFromFile(const QString& filePath);

private:
    ModifierManager(QObject* parent = nullptr);
    ~ModifierManager();

    // 禁止拷贝和赋值
    ModifierManager(const ModifierManager&) = delete;
    ModifierManager& operator=(const ModifierManager&) = delete;

private:
    QList<ModifierInfo> m_modifierList;         // 当前修改器列表(搜索结果)
    QList<DownloadedModifierInfo> m_downloadedModifiers; // 已下载的修改器
}; 