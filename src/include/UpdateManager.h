#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <functional>
#include "ModifierParser.h"
#include "NetworkManager.h"
#include "FileSystem.h"

// 更新进度回调
using UpdateProgressCallback = std::function<void(int, int, bool)>; // 当前进度, 总数, 是否有更新

/**
 * @brief 更新管理器类，专门处理更新检查相关功能
 */
class UpdateManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     * @return UpdateManager对象的引用
     */
    static UpdateManager& getInstance() {
        static UpdateManager instance;
        return instance;
    }

    /**
     * @brief 检查指定修改器的更新
     * @param info 修改器信息
     * @param callback 完成回调
     */
    void checkModifierUpdate(const QString& url, const QString& currentVersion, const QString& currentGameVersion,
                             std::function<void(bool)> callback);

    /**
     * @brief 批量检查修改器更新
     * @param modifiers 需要检查的修改器列表
     * @param progressCallback 进度回调
     * @param completedCallback 完成回调，参数为找到的更新数量
     */
    void batchCheckUpdates(const QList<std::tuple<int, QString, QString, QString>>& modifiers, 
                           UpdateProgressCallback progressCallback,
                           std::function<void(int)> completedCallback);

    /**
     * @brief 比较版本号，判断是否有更新
     * @param oldVersion 当前版本
     * @param newVersion 新版本
     * @return 如果新版本比旧版本新，返回true
     */
    bool isNewerVersion(const QString& oldVersion, const QString& newVersion);

private:
    // 私有构造函数
    UpdateManager(QObject* parent = nullptr);
    ~UpdateManager() = default;

    // 禁止拷贝和赋值
    UpdateManager(const UpdateManager&) = delete;
    UpdateManager& operator=(const UpdateManager&) = delete;
}; 