#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QSettings>
#include <QStringList>
#include "ModifierParser.h"
#include "ModifierInfoManager.h"
#include "GameMappingManager.h"

/**
 * @brief 搜索管理器，负责处理搜索功能和搜索历史
 */
class SearchManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取SearchManager单例实例
     * @return SearchManager单例引用
     */
    static SearchManager& getInstance() {
        static SearchManager instance;
        return instance;
    }

    /**
     * @brief 执行修改器搜索
     * @param searchTerm 搜索关键词
     * @param callback 搜索结果回调函数
     */
    void searchModifiers(const QString& searchTerm, 
                          std::function<void(const QList<ModifierInfo>&)> callback);

    /**
     * @brief 执行修改器搜索 - 使用Qt信号槽连接方式
     * @param searchTerm 搜索关键词
     * @param receiver 接收结果的对象
     * @param slot 处理结果的槽函数指针
     */
    template<typename Receiver, typename Slot>
    void searchModifiers(const QString& searchTerm, const Receiver* receiver, Slot slot) {
        // 使用lambda转发到标准回调方法
        searchModifiers(searchTerm, [=](const QList<ModifierInfo>& modifiers) {
            // 使用QMetaObject::invokeMethod在接收者的线程上调用槽
            // 这确保了槽函数总是在正确的线程上被调用
            QMetaObject::invokeMethod(const_cast<Receiver*>(receiver), [=]() {
                (const_cast<Receiver*>(receiver)->*slot)(modifiers);
            }, Qt::QueuedConnection);
        });
    }

    /**
     * @brief 加载热门修改器列表（用于首页显示）
     * @param callback 搜索结果回调函数
     */
    void loadFeaturedModifiers(std::function<void(const QList<ModifierInfo>&)> callback);

    /**
     * @brief 加载热门修改器列表 - 使用Qt信号槽连接方式
     * @param receiver 接收结果的对象
     * @param slot 处理结果的槽函数指针
     */
    template<typename Receiver, typename Slot>
    void loadFeaturedModifiers(const Receiver* receiver, Slot slot) {
        loadFeaturedModifiers([=](const QList<ModifierInfo>& modifiers) {
            QMetaObject::invokeMethod(const_cast<Receiver*>(receiver), [=]() {
                (const_cast<Receiver*>(receiver)->*slot)(modifiers);
            }, Qt::QueuedConnection);
        });
    }

    /**
     * @brief 获取并解析最近更新的修改器列表（从flingtrainer.com首页）
     * @param callback 搜索结果回调函数
     */
    void fetchRecentlyUpdatedModifiers(std::function<void(const QList<ModifierInfo>&)> callback);

    /**
     * @brief 获取并解析最近更新的修改器列表 - 使用Qt信号槽连接方式
     * @param receiver 接收结果的对象
     * @param slot 处理结果的槽函数指针
     */
    template<typename Receiver, typename Slot>
    void fetchRecentlyUpdatedModifiers(const Receiver* receiver, Slot slot) {
        fetchRecentlyUpdatedModifiers([=](const QList<ModifierInfo>& modifiers) {
            QMetaObject::invokeMethod(const_cast<Receiver*>(receiver), [=]() {
                (const_cast<Receiver*>(receiver)->*slot)(modifiers);
            }, Qt::QueuedConnection);
        });
    }

    /**
     * @brief 更新ModifierManager的修改器列表
     * @param modifiers 修改器列表
     */
    void updateModifierManagerList(const QList<ModifierInfo>& modifiers);

    /**
     * @brief 添加搜索词到历史记录
     * @param searchTerm 搜索词
     */
    void addSearchToHistory(const QString& searchTerm);

    /**
     * @brief 获取搜索历史记录
     * @return 搜索历史词列表
     */
    QStringList getSearchHistory() const;

    /**
     * @brief 清除搜索历史记录
     */
    void clearSearchHistory();

    /**
     * @brief 按相关性排序搜索结果
     * @param modifiers 修改器列表
     * @param searchTerm 搜索关键词
     * @return 排序后的修改器列表
     */
    QList<ModifierInfo> sortByRelevance(const QList<ModifierInfo>& modifiers, 
                                        const QString& searchTerm);

    /**
     * @brief 按热门程度排序搜索结果
     * @param modifiers 修改器列表
     * @return 排序后的修改器列表
     */
    QList<ModifierInfo> sortByPopularity(const QList<ModifierInfo>& modifiers);    /**
     * @brief 按日期排序搜索结果
     * @param modifiers 修改器列表
     * @return 排序后的修改器列表
     */
    QList<ModifierInfo> sortByDate(const QList<ModifierInfo>& modifiers);

private:
    SearchManager(QObject* parent = nullptr);
    ~SearchManager() = default;

    // 禁止拷贝和赋值
    SearchManager(const SearchManager&) = delete;
    SearchManager& operator=(const SearchManager&) = delete;

    // 执行实际的搜索操作（内部方法）
    void performSearch(const QString& searchTerm, 
                      std::function<void(const QList<ModifierInfo>&)> callback);

    // 初始化默认搜索建议
    void initializeDefaultSuggestions();

    // 计算搜索词与修改器的相关性分数
    int calculateRelevanceScore(const ModifierInfo& modifier, const QString& searchTerm);

    // 保存和加载搜索历史
    void saveSearchHistory();
    void loadSearchHistory();

private:
    QStringList m_searchHistory;    // 搜索历史记录
    int m_maxHistoryItems;          // 最大历史记录数量
}; 