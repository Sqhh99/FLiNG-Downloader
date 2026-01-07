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
 * @brief Search manager responsible for handling search functionality and search history
 */
class SearchManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Get the SearchManager singleton instance
     * @return Reference to the SearchManager singleton
     */
    static SearchManager& getInstance() {
        static SearchManager instance;
        return instance;
    }

    /**
     * @brief Execute modifier search
     * @param searchTerm Search keyword
     * @param callback Search result callback function
     */
    void searchModifiers(const QString& searchTerm, 
                          std::function<void(const QList<ModifierInfo>&)> callback);

    /**
     * @brief Execute modifier search - using Qt signal-slot connection method
     * @param searchTerm Search keyword
     * @param receiver Object that receives the result
     * @param slot Slot function pointer that handles the result
     */
    template<typename Receiver, typename Slot>
    void searchModifiers(const QString& searchTerm, const Receiver* receiver, Slot slot) {
        // Use lambda to forward to standard callback method
        searchModifiers(searchTerm, [=](const QList<ModifierInfo>& modifiers) {
            // Use QMetaObject::invokeMethod to call the slot on receiver's thread
            // This ensures the slot function is always called on the correct thread
            QMetaObject::invokeMethod(const_cast<Receiver*>(receiver), [=]() {
                (const_cast<Receiver*>(receiver)->*slot)(modifiers);
            }, Qt::QueuedConnection);
        });
    }

    /**
     * @brief Load featured modifier list (for homepage display)
     * @param callback Search result callback function
     */
    void loadFeaturedModifiers(std::function<void(const QList<ModifierInfo>&)> callback);

    /**
     * @brief Load featured modifier list - using Qt signal-slot connection method
     * @param receiver Object that receives the result
     * @param slot Slot function pointer that handles the result
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
     * @brief Fetch and parse recently updated modifier list (from flingtrainer.com homepage)
     * @param callback Search result callback function
     */
    void fetchRecentlyUpdatedModifiers(std::function<void(const QList<ModifierInfo>&)> callback);

    /**
     * @brief Fetch and parse recently updated modifier list - using Qt signal-slot connection method
     * @param receiver Object that receives the result
     * @param slot Slot function pointer that handles the result
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
     * @brief Update the ModifierManager modifier list
     * @param modifiers Modifier list
     */
    void updateModifierManagerList(const QList<ModifierInfo>& modifiers);

    /**
     * @brief Add search term to history
     * @param searchTerm Search term
     */
    void addSearchToHistory(const QString& searchTerm);

    /**
     * @brief Get search history
     * @return List of search history terms
     */
    QStringList getSearchHistory() const;

    /**
     * @brief Clear search history
     */
    void clearSearchHistory();

    /**
     * @brief Sort search results by relevance
     * @param modifiers Modifier list
     * @param searchTerm Search keyword
     * @return Sorted modifier list
     */
    QList<ModifierInfo> sortByRelevance(const QList<ModifierInfo>& modifiers, 
                                        const QString& searchTerm);

    /**
     * @brief Sort search results by popularity
     * @param modifiers Modifier list
     * @return Sorted modifier list
     */
    QList<ModifierInfo> sortByPopularity(const QList<ModifierInfo>& modifiers);    /**
     * @brief Sort search results by date
     * @param modifiers Modifier list
     * @return Sorted modifier list
     */
    QList<ModifierInfo> sortByDate(const QList<ModifierInfo>& modifiers);

private:
    SearchManager(QObject* parent = nullptr);
    ~SearchManager() = default;

    // Disable copy and assignment
    SearchManager(const SearchManager&) = delete;
    SearchManager& operator=(const SearchManager&) = delete;

    // Execute actual search operation (internal method)
    void performSearch(const QString& searchTerm, 
                      std::function<void(const QList<ModifierInfo>&)> callback);

    // Initialize default search suggestions
    void initializeDefaultSuggestions();

    // Calculate relevance score between search term and modifier
    int calculateRelevanceScore(const ModifierInfo& modifier, const QString& searchTerm);

    // Save and load search history
    void saveSearchHistory();
    void loadSearchHistory();

private:
    QStringList m_searchHistory;    // Search history
    int m_maxHistoryItems;          // Maximum history items count
}; 