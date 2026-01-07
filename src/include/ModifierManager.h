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

// Downloaded modifier information structure
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

// Sort type enumeration
enum class SortType {
    ByUpdateDate,
    ByName,
    ByOptionsCount
};

// Modifier search callback function type
using ModifierFoundCallback = std::function<void(const QList<ModifierInfo>&)>;
using ModifierDetailCallback = std::function<void(ModifierInfo*)>;
using ModifierDownloadFinishedCallback = std::function<void(bool, const QString&, const QString&, const ModifierInfo&, bool)>;
using UpdateProgressCallback = std::function<void(int, int, bool)>; // current progress, total, has update

class ModifierManager : public QObject
{
    Q_OBJECT

public:
    static ModifierManager& getInstance();

    // Search modifiers - implemented using SearchManager
    void searchModifiers(const QString& searchTerm, ModifierFoundCallback callback);
    
    // Set modifier list directly - called by SearchManager
    void setModifierList(const QList<ModifierInfo>& modifiers);
    
    // Get modifier details
    void getModifierDetail(const QString& url, ModifierDetailCallback callback);
    
    // Download modifier - implemented using DownloadManager
    void downloadModifier(const ModifierInfo& modifier, 
                         const QString& version, 
                         const QString& savePath,
                         ModifierDownloadFinishedCallback callback,
                         DLProgressCallback progressCallback);
    
    // Downloaded modifier management
    QList<DownloadedModifierInfo> getDownloadedModifiers() const;
    void addDownloadedModifier(const ModifierInfo& info, const QString& version, const QString& filePath);
    void removeDownloadedModifier(int index);
    
    // Update checking - implemented using UpdateManager
    void checkForUpdates(int index = -1, std::function<void(bool)> callback = nullptr);
    void batchCheckForUpdates(UpdateProgressCallback progressCallback, 
                              std::function<void(int)> completedCallback);
    void setModifierUpdateStatus(int index, bool hasUpdate);
    
    // Modifier list sorting and filtering - implemented using SearchManager and ModifierInfoManager
    void sortModifierList(SortType sortType);
    QList<ModifierInfo> getSortedModifierList() const;
    QList<ModifierInfo> filterModifiersByKeyword(const QString& keyword) const;
    
    // Save and load downloaded modifier list - using ModifierInfoManager's JSON functionality
    void saveDownloadedModifiers();
    void loadDownloadedModifiers();
    
    // Export and import modifier info - implemented using ModifierInfoManager
    bool exportModifierToFile(const ModifierInfo& info, const QString& filePath);
    ModifierInfo importModifierFromFile(const QString& filePath);

private:
    ModifierManager(QObject* parent = nullptr);
    ~ModifierManager();

    // Disable copy and assignment
    ModifierManager(const ModifierManager&) = delete;
    ModifierManager& operator=(const ModifierManager&) = delete;

private:
    QList<ModifierInfo> m_modifierList;         // Current modifier list (search results)
    QList<DownloadedModifierInfo> m_downloadedModifiers; // Downloaded modifiers
}; 