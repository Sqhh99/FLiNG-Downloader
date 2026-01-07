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
 * @brief Modifier info manager responsible for modifier information storage, formatting and conversion
 */
class ModifierInfoManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Get singleton instance
     * @return Reference to ModifierInfoManager object
     */
    static ModifierInfoManager& getInstance() {
        static ModifierInfoManager instance;
        return instance;
    }

    /**
     * @brief Create a new modifier info object
     * @param name Modifier name
     * @param gameVersion Game version
     * @param url Modifier detail page URL
     * @return Newly created ModifierInfo object
     */
    ModifierInfo createModifierInfo(const QString& name, 
                                   const QString& gameVersion = QString(),
                                   const QString& url = QString());

    /**
     * @brief Clone a modifier info object
     * @param other Object to clone
     * @return Cloned new object
     */
    ModifierInfo* cloneModifierInfo(const ModifierInfo& other);

    /**
     * @brief Extract modifier name from URL
     * @param url Modifier URL
     * @return Extracted modifier name
     */
    QString extractNameFromUrl(const QString& url);

    /**
     * @brief Format modifier name (remove special characters etc.)
     * @param name Original name
     * @return Formatted name
     */
    QString formatModifierName(const QString& name);

    /**
     * @brief Format version string to ensure unified format
     * @param version Original version string
     * @return Formatted version string
     */
    QString formatVersionString(const QString& version);

    /**
     * @brief Add version info to modifier
     * @param info Modifier info object
     * @param version Version number
     * @param url Download URL
     */
    void addVersionToModifier(ModifierInfo& info, const QString& version, const QString& url);

    /**
     * @brief Compare similarity between two modifier info objects (may come from different sources but represent the same modifier)
     * @param a First modifier info
     * @param b Second modifier info
     * @return Similarity score 0-100
     */
    int compareModifierSimilarity(const ModifierInfo& a, const ModifierInfo& b);

    /**
     * @brief Export modifier info to JSON format
     * @param info Modifier info to export
     * @return JSON string
     */
    QString exportToJson(const ModifierInfo& info);

    /**
     * @brief Import modifier info from JSON
     * @param json JSON string
     * @return Parsed modifier info
     */
    ModifierInfo importFromJson(const QString& json);

    /**
     * @brief Convert HTML modifier options to plain text list
     * @param htmlOptions Options in HTML format
     * @return Plain text options list
     */
    QStringList convertHtmlOptionsToPlainText(const QString& htmlOptions);

    /**
     * @brief Search modifiers by keyword
     * @param modifiers Modifier list
     * @param keyword Search keyword
     * @return Matching modifier list
     */
    QList<ModifierInfo> searchModifiersByKeyword(const QList<ModifierInfo>& modifiers, const QString& keyword);

private:
    ModifierInfoManager(QObject* parent = nullptr);
    ~ModifierInfoManager() = default;

    ModifierInfoManager(const ModifierInfoManager&) = delete;
    ModifierInfoManager& operator=(const ModifierInfoManager&) = delete;
}; 