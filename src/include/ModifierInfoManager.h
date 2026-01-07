#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <QDateTime>
#include <QSettings>
#include <QPair>
#include "FileSystem.h"
#include "ModifierParser.h"

/**
 * @brief 修改器信息管理器，负责修改器信息的存储、格式化和转换
 */
class ModifierInfoManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     * @return ModifierInfoManager对象的引用
     */
    static ModifierInfoManager& getInstance() {
        static ModifierInfoManager instance;
        return instance;
    }

    /**
     * @brief 创建一个新的修改器信息对象
     * @param name 修改器名称
     * @param gameVersion 游戏版本
     * @param url 修改器详情页URL
     * @return 新创建的ModifierInfo对象
     */
    ModifierInfo createModifierInfo(const QString& name, 
                                   const QString& gameVersion = QString(),
                                   const QString& url = QString());

    /**
     * @brief 复制一个修改器信息对象
     * @param other 要复制的对象
     * @return 复制后的新对象
     */
    ModifierInfo* cloneModifierInfo(const ModifierInfo& other);

    /**
     * @brief 从URL提取修改器名称
     * @param url 修改器URL
     * @return 提取的修改器名称
     */
    QString extractNameFromUrl(const QString& url);

    /**
     * @brief 格式化修改器名称（移除特殊字符等）
     * @param name 原始名称
     * @return 格式化后的名称
     */
    QString formatModifierName(const QString& name);

    /**
     * @brief 格式化版本号，确保统一格式
     * @param version 原始版本号
     * @return 格式化后的版本号
     */
    QString formatVersionString(const QString& version);

    /**
     * @brief 添加版本信息到修改器
     * @param info 修改器信息对象
     * @param version 版本号
     * @param url 下载URL
     */
    void addVersionToModifier(ModifierInfo& info, const QString& version, const QString& url);

    /**
     * @brief 比较两个修改器信息是否相似（可能来自不同源但表示同一修改器）
     * @param a 第一个修改器信息
     * @param b 第二个修改器信息
     * @return 相似度 0-100
     */
    int compareModifierSimilarity(const ModifierInfo& a, const ModifierInfo& b);

    /**
     * @brief 导出修改器信息为JSON格式
     * @param info 要导出的修改器信息
     * @return JSON字符串
     */
    QString exportToJson(const ModifierInfo& info);

    /**
     * @brief 从JSON导入修改器信息
     * @param json JSON字符串
     * @return 解析的修改器信息
     */
    ModifierInfo importFromJson(const QString& json);

    /**
     * @brief 转换HTML修改器选项为纯文本列表
     * @param htmlOptions HTML格式的选项
     * @return 纯文本选项列表
     */
    QStringList convertHtmlOptionsToPlainText(const QString& htmlOptions);

    /**
     * @brief 根据关键词搜索修改器
     * @param modifiers 修改器列表
     * @param keyword 搜索关键词
     * @return 匹配的修改器列表
     */
    QList<ModifierInfo> searchModifiersByKeyword(const QList<ModifierInfo>& modifiers, const QString& keyword);

private:
    ModifierInfoManager(QObject* parent = nullptr);
    ~ModifierInfoManager() = default;

    ModifierInfoManager(const ModifierInfoManager&) = delete;
    ModifierInfoManager& operator=(const ModifierInfoManager&) = delete;
}; 