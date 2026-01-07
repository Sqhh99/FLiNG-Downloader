#include "ModifierManager.h"
#include "NetworkManager.h"
#include "ConfigManager.h"
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include "FileSystem.h"
#include "UpdateManager.h"
#include "DownloadManager.h"
#include "ModifierInfoManager.h"
#include "SearchManager.h"

ModifierManager::ModifierManager(QObject* parent)
    : QObject(parent)
{
    loadDownloadedModifiers();
}

ModifierManager::~ModifierManager()
{
    saveDownloadedModifiers();
}

ModifierManager& ModifierManager::getInstance()
{
    static ModifierManager instance;
    return instance;
}

void ModifierManager::searchModifiers(const QString& searchTerm, ModifierFoundCallback callback)
{
    if (searchTerm.isEmpty() && !callback) {
        return;
    }
    
    // Use SearchManager for searching
    SearchManager::getInstance().searchModifiers(searchTerm, 
        [this, callback](const QList<ModifierInfo>& modifiers) {
            m_modifierList = modifiers;
            
            if (callback) {
                callback(m_modifierList);
            }
        }
    );
}

void ModifierManager::getModifierDetail(const QString& url, ModifierDetailCallback callback)
{
    QString modifierName = ModifierInfoManager::getInstance().extractNameFromUrl(url);
    if (modifierName.isEmpty()) {
        modifierName = QObject::tr("Unknown Modifier");
    }
    
    NetworkManager::getInstance().sendGetRequest(
        url,
        [this, modifierName, callback, url](const QByteArray& data, bool success) {
            if (success) {
                ModifierInfo* modifier = ModifierParser::parseModifierDetailHTML(data.toStdString(), modifierName);
                modifier->url = url;
                
                modifier->name = ModifierInfoManager::getInstance().formatModifierName(modifier->name);
                
                for (int i = 0; i < modifier->versions.size(); ++i) {
                    QString formattedVersion = ModifierInfoManager::getInstance().formatVersionString(modifier->versions[i].first);
                    modifier->versions[i].first = formattedVersion;
                }
                
                callback(modifier);
            } else {
                qWarning() << "ModifierManager: Failed to get modifier details";
                
                ModifierInfo* modifier = new ModifierInfo();
                modifier->name = modifierName;
                modifier->url = url;
                callback(modifier);
            }
        }
    );
}

void ModifierManager::downloadModifier(const ModifierInfo& modifier, 
                                      const QString& version, 
                                      const QString& savePath,
                                      ModifierDownloadFinishedCallback callback,
                                      DLProgressCallback progressCallback)
{    // Use DownloadManager to download the modifier
    DownloadManager::getInstance().downloadModifier(
        modifier,
        version,
        savePath,
        [this, callback, version, savePath](bool success, const QString& errorMsg, const QString& actualPath, const ModifierInfo& modifier, bool isArchive) {
            if (success) {
                // Add to downloaded modifiers list - use actual file path
                addDownloadedModifier(modifier, version, actualPath);
            }
            
            if (callback) {
                callback(success, errorMsg, actualPath, modifier, isArchive);
            }
        },
        progressCallback
    );
}

QList<DownloadedModifierInfo> ModifierManager::getDownloadedModifiers() const
{
    return m_downloadedModifiers;
}

void ModifierManager::addDownloadedModifier(const ModifierInfo& info, const QString& version, const QString& filePath)
{
    // Ensure name is not empty
    QString modifierName = info.name;
    if (modifierName.isEmpty()) {
        // Use FileSystem to get file name
        QFileInfo fileInfo = FileSystem::getInstance().getFileInfo(filePath);
        modifierName = fileInfo.baseName();
    }
    
    // Check if already exists
    for (int i = 0; i < m_downloadedModifiers.size(); ++i) {
        if (m_downloadedModifiers[i].name == modifierName && m_downloadedModifiers[i].version == version) {
            // Update existing entry
            m_downloadedModifiers[i].filePath = filePath;
            m_downloadedModifiers[i].downloadDate = QDateTime::currentDateTime();
            m_downloadedModifiers[i].gameVersion = info.gameVersion;
            m_downloadedModifiers[i].optionsCount = info.optionsCount;
            
            // Ensure URL is not empty and update existing URL
            if (!info.url.isEmpty()) {
                m_downloadedModifiers[i].url = info.url;
            }
            
            m_downloadedModifiers[i].hasUpdate = false;
            
            // Save
            saveDownloadedModifiers();
            return;
        }
    }
    
    // Create new entry
    DownloadedModifierInfo newInfo;
    newInfo.name = modifierName;
    newInfo.version = version;
    newInfo.filePath = filePath;
    newInfo.downloadDate = QDateTime::currentDateTime();
    newInfo.gameVersion = info.gameVersion;
    newInfo.optionsCount = info.optionsCount;
    newInfo.url = info.url;
    newInfo.hasUpdate = false;
    
    // Add to list
    m_downloadedModifiers.append(newInfo);
    
    // Save
    saveDownloadedModifiers();
}

void ModifierManager::removeDownloadedModifier(int index)
{
    if (index < 0 || index >= m_downloadedModifiers.size()) {
        return;
    }
    
    // Delete file - use FileSystem
    FileSystem::getInstance().deleteFile(m_downloadedModifiers[index].filePath);
    
    // Remove from list
    m_downloadedModifiers.removeAt(index);
    
    // Save
    saveDownloadedModifiers();
}

void ModifierManager::checkForUpdates(int index, std::function<void(bool)> callback)
{
    if (index >= 0 && index < m_downloadedModifiers.size()) {
        if (m_downloadedModifiers[index].url.isEmpty()) {
            if (callback) callback(false);
            return;
        }
        
        // Use UpdateManager to check for updates
        UpdateManager::getInstance().checkModifierUpdate(
            m_downloadedModifiers[index].url,
            m_downloadedModifiers[index].version,
            m_downloadedModifiers[index].gameVersion,
            [this, index, callback](bool hasUpdate) {
                // Save update status
                m_downloadedModifiers[index].hasUpdate = hasUpdate;
                
                // Save downloaded list
                saveDownloadedModifiers();
                
                if (callback) callback(hasUpdate);
            }
        );
    } else {
        // Check updates for all modifiers
        bool hasAnyUrl = false;
        
        for (int i = 0; i < m_downloadedModifiers.size(); ++i) {
            if (!m_downloadedModifiers[i].url.isEmpty()) {
                hasAnyUrl = true;
                break;
            }
        }
        
        if (callback) callback(hasAnyUrl);
    }
}

void ModifierManager::batchCheckForUpdates(UpdateProgressCallback progressCallback, 
                                          std::function<void(int)> completedCallback)
{
    // Build list of modifiers to check
    QList<std::tuple<int, QString, QString, QString>> modifiersToCheck;
    
    for (int i = 0; i < m_downloadedModifiers.size(); ++i) {
        if (!m_downloadedModifiers[i].url.isEmpty()) {
            modifiersToCheck.append(std::make_tuple(
                i, 
                m_downloadedModifiers[i].url,
                m_downloadedModifiers[i].version,
                m_downloadedModifiers[i].gameVersion
            ));
        }
    }
    
    // Use UpdateManager for batch checking
    UpdateManager::getInstance().batchCheckUpdates(
        modifiersToCheck,
        [this, progressCallback](int current, int total, bool hasUpdates) {
            // Forward progress callback
            if (progressCallback) {
                progressCallback(current, total, hasUpdates);
            }
        },
        [this, completedCallback](int updatesCount) {
            // Save downloaded list
            saveDownloadedModifiers();
            
            // Forward completed callback
            if (completedCallback) {
                completedCallback(updatesCount);
            }
        }
    );
}

void ModifierManager::setModifierUpdateStatus(int index, bool hasUpdate)
{
    if (index >= 0 && index < m_downloadedModifiers.size()) {
        m_downloadedModifiers[index].hasUpdate = hasUpdate;
        saveDownloadedModifiers();
    }
}

void ModifierManager::sortModifierList(SortType sortType)
{
    switch (sortType) {
        case SortType::ByUpdateDate:
            std::sort(m_modifierList.begin(), m_modifierList.end(), [](const ModifierInfo &a, const ModifierInfo &b) {
                return a.lastUpdate > b.lastUpdate; // Descending order, most recent first
            });
            break;
            
        case SortType::ByName:
            std::sort(m_modifierList.begin(), m_modifierList.end(), [](const ModifierInfo &a, const ModifierInfo &b) {
                return a.name.compare(b.name, Qt::CaseInsensitive) < 0; // Ascending order
            });
            break;
            
        case SortType::ByOptionsCount:
            std::sort(m_modifierList.begin(), m_modifierList.end(), [](const ModifierInfo &a, const ModifierInfo &b) {
                return a.optionsCount > b.optionsCount; // Descending order, more options first
            });
            break;
    }
}

QList<ModifierInfo> ModifierManager::getSortedModifierList() const
{
    return m_modifierList;
}

void ModifierManager::saveDownloadedModifiers()
{
    // Use FileSystem to get app data directory and ensure it exists
    QString dataDir = FileSystem::getInstance().getAppDataDirectory();
    QString settingsPath = dataDir + "/downloaded_modifiers.ini";
    
    QSettings settings(settingsPath, QSettings::IniFormat);
    settings.beginWriteArray("modifiers");
    
    for (int i = 0; i < m_downloadedModifiers.size(); ++i) {
        settings.setArrayIndex(i);
        const DownloadedModifierInfo& info = m_downloadedModifiers.at(i);
        
        settings.setValue("name", info.name);
        settings.setValue("version", info.version);
        settings.setValue("filePath", info.filePath);
        settings.setValue("downloadDate", info.downloadDate);
        settings.setValue("gameVersion", info.gameVersion);
        settings.setValue("optionsCount", info.optionsCount);
        settings.setValue("url", info.url);
    }
    
    settings.endArray();
    settings.sync();
    
    qDebug() << "Downloaded modifiers saved to:" << settingsPath;
}

void ModifierManager::loadDownloadedModifiers()
{
    m_downloadedModifiers.clear();
    
    // Use FileSystem to get app data directory
    QString dataDir = FileSystem::getInstance().getAppDataDirectory();
    QString settingsPath = dataDir + "/downloaded_modifiers.ini";
    
    // Check if file exists
    if (!FileSystem::getInstance().fileExists(settingsPath)) {
        qDebug() << "Downloaded modifiers config not found, using defaults";
        return;
    }
    
    QSettings settings(settingsPath, QSettings::IniFormat);
    int size = settings.beginReadArray("modifiers");
    
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        
        DownloadedModifierInfo info;
        info.name = settings.value("name").toString();
        info.version = settings.value("version").toString();
        info.filePath = settings.value("filePath").toString();
        info.downloadDate = settings.value("downloadDate").toDateTime();
        info.gameVersion = settings.value("gameVersion").toString();
        info.optionsCount = settings.value("optionsCount").toInt();
        info.url = settings.value("url").toString();
        info.hasUpdate = false; // Initialize to no update
        
        // Check if file exists - use FileSystem
        if (FileSystem::getInstance().fileExists(info.filePath)) {
            m_downloadedModifiers.append(info);
        }
    }
    
    settings.endArray();
    
    qDebug() << "Loaded" << m_downloadedModifiers.size() << "downloaded modifiers from:" << settingsPath;
}

QList<ModifierInfo> ModifierManager::filterModifiersByKeyword(const QString& keyword) const
{
    // Use ModifierInfoManager to filter modifiers
    return ModifierInfoManager::getInstance().searchModifiersByKeyword(m_modifierList, keyword);
}

bool ModifierManager::exportModifierToFile(const ModifierInfo& info, const QString& filePath)
{
    // Use ModifierInfoManager to export modifier info as JSON
    QString json = ModifierInfoManager::getInstance().exportToJson(info);
    
    // Save to file
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Cannot open file for writing:" << filePath;
        return false;
    }
    
    QTextStream out(&file);
    out << json;
    file.close();
    
    return true;
}

ModifierInfo ModifierManager::importModifierFromFile(const QString& filePath)
{
    // Read JSON from file
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open file for reading:" << filePath;
        return ModifierInfo();
    }
    
    QTextStream in(&file);
    QString json = in.readAll();
    file.close();
    
    // Use ModifierInfoManager to import modifier info from JSON
    return ModifierInfoManager::getInstance().importFromJson(json);
}

// Set modifier list directly
void ModifierManager::setModifierList(const QList<ModifierInfo>& modifiers)
{
    m_modifierList = modifiers;
    qDebug() << "ModifierManager: Modifier list set externally, count:" << m_modifierList.size();
} 