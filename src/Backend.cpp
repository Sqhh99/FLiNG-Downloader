#include "Backend.h"
#include <QGuiApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include "FileSystem.h"
#include "ConfigManager.h"
#include "ThemeManager.h"
#include "LanguageManager.h"
#include "DownloadManager.h"
#include <QRegularExpression>
#include <QSet>

Backend::Backend(QObject* parent)
    : QObject(parent)
    , m_modifierListModel(new ModifierListModel(this))
    , m_downloadedModifierModel(new DownloadedModifierModel(this))
    , m_coverExtractor(new CoverExtractor(this))
{
    loadGameMappings();
    loadDownloadedModifiers();
    fetchRecentModifiers();
}

Backend::~Backend()
{
    saveDownloadedModifiers();
}

int Backend::currentLanguage() const
{
    return static_cast<int>(ConfigManager::getInstance().getCurrentLanguage());
}

QString Backend::selectedModifierName() const
{
    return m_selectedModifier.name;
}

QString Backend::selectedModifierVersion() const
{
    return m_selectedModifier.gameVersion;
}

int Backend::selectedModifierOptionsCount() const
{
    return m_selectedModifier.optionsCount;
}

QString Backend::selectedModifierLastUpdate() const
{
    return m_selectedModifier.lastUpdate;
}

QString Backend::selectedModifierOptions() const
{
    return m_selectedOptions;
}

QVariantList Backend::selectedModifierVersions() const
{
    QVariantList versions;
    for (const auto& version : m_selectedModifier.versions) {
        QVariantMap versionMap;
        versionMap["name"] = version.first;  // Version identifier
        versionMap["url"] = version.second;  // Download link
        versions.append(versionMap);
    }
    return versions;
}

QString Backend::selectedModifierCoverUrl() const
{
    // Use modifier's screenshot URL as cover
    // screenshotUrl is usually a game screenshot that can be displayed
    return m_selectedModifier.screenshotUrl;
}

void Backend::searchModifiers(const QString& keyword)
{
    emit statusMessage(tr("Searching: %1").arg(keyword));
    SearchManager::getInstance().searchModifiers(keyword, this, &Backend::onSearchCompleted);
}

void Backend::fetchRecentModifiers()
{
    emit statusMessage(tr("Loading modifiers..."));
    SearchManager::getInstance().fetchRecentlyUpdatedModifiers(this, &Backend::onSearchCompleted);
}

void Backend::setSortOrder(int sortIndex)
{
    QList<ModifierInfo> modifiers = m_modifierListModel->getAllModifiers();
    
    switch (sortIndex) {
        case 0: // Recently updated
            std::sort(modifiers.begin(), modifiers.end(), [](const ModifierInfo& a, const ModifierInfo& b) {
                return a.lastUpdate > b.lastUpdate;
            });
            break;
        case 1: // By name
            std::sort(modifiers.begin(), modifiers.end(), [](const ModifierInfo& a, const ModifierInfo& b) {
                return a.name.toLower() < b.name.toLower();
            });
            break;
        case 2: // By options count
            std::sort(modifiers.begin(), modifiers.end(), [](const ModifierInfo& a, const ModifierInfo& b) {
                return a.optionsCount > b.optionsCount;
            });
            break;
    }
    
    m_modifierListModel->setModifiers(modifiers);
}

void Backend::selectModifier(int index)
{
    if (index < 0 || index >= m_modifierListModel->count()) {
        return;
    }
    
    m_selectedIndex = index;
    m_selectedModifier = m_modifierListModel->getModifier(index);
    m_selectedVersionIndex = 0;
    
    emit selectedModifierChanged();
    
    if (!m_selectedModifier.url.isEmpty()) {
        emit statusMessage(tr("Loading modifier details..."));
        
        ModifierManager::getInstance().getModifierDetail(
            m_selectedModifier.url,
            [this](ModifierInfo* modifier) {
                if (modifier) {
                    m_selectedModifier.versions = modifier->versions;
                    m_selectedModifier.options = modifier->options;
                    m_selectedModifier.optionsCount = modifier->optionsCount;
                    m_selectedModifier.screenshotUrl = modifier->screenshotUrl;
                    m_selectedModifier.description = modifier->description;
                    
                    m_selectedOptions = m_selectedModifier.options.join("\n");
                    
                    emit selectedModifierChanged();
                    emit selectedModifierOptionsChanged();
                    emit statusMessage(tr("Details loaded"));
                    
                    extractCover();
                    
                    delete modifier;
                } else {
                    emit statusMessage(tr("Failed to load details"));
                }
            }
        );
    }
}

void Backend::selectVersion(int versionIndex)
{
    m_selectedVersionIndex = versionIndex;
}

void Backend::downloadModifier(int versionIndex)
{
    if (m_selectedModifier.versions.isEmpty()) {
        emit statusMessage(tr("No download version available"));
        return;
    }
    
    int idx = (versionIndex >= 0 && versionIndex < m_selectedModifier.versions.size()) 
              ? versionIndex : 0;
    
    QString versionName = m_selectedModifier.versions.at(idx).first;
    QString downloadDir = ConfigManager::getInstance().getDownloadDirectory();
    
    // Sanitize filename - remove characters that are illegal in Windows file paths
    QString sanitizedName = m_selectedModifier.name;
    // Replace illegal characters: \ / : * ? " < > |
    sanitizedName.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
    // Also remove trailing/leading spaces and dots (Windows doesn't like them)
    sanitizedName = sanitizedName.trimmed();
    while (sanitizedName.endsWith(".")) {
        sanitizedName.chop(1);
    }
    
    QString savePath = downloadDir + "/" + sanitizedName + "_" + versionName + ".zip";
    
    m_isDownloading = true;
    m_downloadProgress = 0.0;
    emit downloadingChanged();
    emit statusMessage(tr("Downloading: %1").arg(m_selectedModifier.name));
    
    ModifierManager::getInstance().downloadModifier(
        m_selectedModifier,
        versionName,
        savePath,
        [this, versionName](bool success, const QString& errorMsg, const QString& filePath, const ModifierInfo& modifier, bool isArchive) {
            Q_UNUSED(modifier)
            Q_UNUSED(isArchive)
            
            m_isDownloading = false;
            emit downloadingChanged();
            
            if (success) {
                m_downloadProgress = 1.0;
                emit downloadProgressChanged();
                emit downloadCompleted(true);
                emit statusMessage(tr("Download complete: %1").arg(filePath));
                
                DownloadedModifierInfo downloadedInfo;
                downloadedInfo.name = m_selectedModifier.name;
                downloadedInfo.version = versionName;
                downloadedInfo.gameVersion = m_selectedModifier.gameVersion;
                downloadedInfo.downloadDate = QDateTime::currentDateTime();
                downloadedInfo.filePath = filePath;
                downloadedInfo.url = m_selectedModifier.url;
                
                m_downloadedList.append(downloadedInfo);
                m_downloadedModifierModel->setModifiers(m_downloadedList);
                saveDownloadedModifiers();
            } else {
                m_downloadProgress = 0.0;
                emit downloadProgressChanged();
                emit downloadCompleted(false);
                emit statusMessage(tr("Download failed: %1").arg(errorMsg));
            }
        },
        [this](int progress) {
            m_downloadProgress = static_cast<qreal>(progress) / 100.0;
            emit downloadProgressChanged();
        }
    );
}

// Cover extraction
void Backend::extractCover()
{
    if (m_selectedModifier.screenshotUrl.isEmpty()) {
        return;
    }
    
    QString gameId = m_selectedModifier.name;
    gameId.replace(QRegularExpression("[^a-zA-Z0-9]"), "_");
    
    QPixmap cachedCover = CoverExtractor::getCachedCover(gameId);
    if (!cachedCover.isNull()) {
        QString cachePath = CoverExtractor::getCacheDirectory();
        QString coverFilePath = cachePath + "/" + gameId + ".png";
        m_currentCoverPath = "file:///" + coverFilePath;
        emit coverExtracted();
        return;
    }
    
    emit statusMessage(tr("Extracting cover..."));
    
    m_coverExtractor->extractCoverFromTrainerImage(
        m_selectedModifier.screenshotUrl,
        [this, gameId](const QPixmap& cover, bool success) {
            if (success && !cover.isNull()) {
                if (CoverExtractor::saveCoverToCache(gameId, cover)) {
                    QString cachePath = CoverExtractor::getCacheDirectory();
                    QString coverFilePath = cachePath + "/" + gameId + ".png";
                    
                    m_currentCoverPath = "file:///" + coverFilePath;
                    emit coverExtracted();
                    emit statusMessage(tr("Cover extracted"));
                } else {
                    emit statusMessage(tr("Failed to save cover"));
                }
            } else {
                emit statusMessage(tr("Failed to extract cover"));
            }
        }
    );
}

void Backend::openDownloadFolder()
{
    QString downloadDir = ConfigManager::getInstance().getDownloadDirectory();
    QDesktopServices::openUrl(QUrl::fromLocalFile(downloadDir));
}

void Backend::runModifier(int index)
{
    if (index < 0 || index >= m_downloadedModifierModel->count()) {
        return;
    }
    
    DownloadedModifierInfo modifier = m_downloadedModifierModel->getModifier(index);
    
    if (QFile::exists(modifier.filePath)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(modifier.filePath));
    } else {
        emit statusMessage(tr("File not found: %1").arg(modifier.filePath));
    }
}

void Backend::deleteModifier(int index)
{
    if (index < 0 || index >= m_downloadedModifierModel->count()) {
        return;
    }
    
    DownloadedModifierInfo modifier = m_downloadedModifierModel->getModifier(index);
    
    if (QFile::exists(modifier.filePath)) {
        QFile::remove(modifier.filePath);
    }
    
    m_downloadedModifierModel->removeModifier(index);
    saveDownloadedModifiers();
    
    emit statusMessage(tr("Deleted: %1").arg(modifier.name));
}

void Backend::checkForUpdates()
{
    emit statusMessage(tr("Checking for updates..."));
    // TODO: Implement update check logic
}

void Backend::setTheme(int themeIndex)
{
    ConfigManager::Theme theme = static_cast<ConfigManager::Theme>(themeIndex);
    ThemeManager::getInstance().switchTheme(theme);
}

void Backend::setLanguage(int languageIndex)
{
    if (!m_app) return;
    
    LanguageManager::Language language = static_cast<LanguageManager::Language>(languageIndex);
    LanguageManager::getInstance().switchLanguage(*m_app, language);
    
    if (m_qmlEngine) {
        m_qmlEngine->retranslate();
    }
    
    emit languageChanged();
}

QString Backend::downloadPath() const
{
    return ConfigManager::getInstance().getDownloadDirectory();
}

void Backend::setDownloadPath(const QString& path)
{
    if (!path.isEmpty() && path != downloadPath()) {
        ConfigManager::getInstance().setDownloadDirectory(path);
        emit downloadPathChanged();
    }
}

void Backend::requestDownloadFolderSelection()
{
    // Emit signal to request QML side to show folder selection dialog
    emit downloadFolderSelectionRequested();
}

void Backend::onSearchCompleted(const QList<ModifierInfo>& modifiers)
{
    m_modifierListModel->setModifiers(modifiers);
    emit searchCompleted();
    emit statusMessage(tr("Found %1 modifiers").arg(modifiers.size()));
}

void Backend::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        m_downloadProgress = static_cast<qreal>(bytesReceived) / bytesTotal;
        emit downloadProgressChanged();
    }
}

void Backend::onDownloadFinished(bool success)
{
    m_isDownloading = false;
    emit downloadingChanged();
    emit downloadCompleted(success);
    
    if (success) {
        emit statusMessage(tr("Download complete"));
    } else {
        emit statusMessage(tr("Download failed"));
    }
}

void Backend::loadDownloadedModifiers()
{
    QString filePath = FileSystem::getInstance().getAppDataDirectory() + "/downloaded_modifiers.json";
    QFile file(filePath);
    
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isArray()) {
        return;
    }
    
    QList<DownloadedModifierInfo> list;
    QJsonArray array = doc.array();
    
    for (const auto& item : array) {
        QJsonObject obj = item.toObject();
        DownloadedModifierInfo info;
        info.name = obj["name"].toString();
        info.version = obj["version"].toString();
        info.gameVersion = obj["gameVersion"].toString();
        info.downloadDate = QDateTime::fromString(obj["downloadDate"].toString(), Qt::ISODate);
        info.filePath = obj["filePath"].toString();
        info.url = obj["url"].toString();
        list.append(info);
    }
    
    m_downloadedModifierModel->setModifiers(list);
}

void Backend::saveDownloadedModifiers()
{
    QString filePath = FileSystem::getInstance().getAppDataDirectory() + "/downloaded_modifiers.json";
    QFile file(filePath);
    
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Backend: Failed to save downloaded modifiers";
        return;
    }
    
    QJsonArray array;
    for (int i = 0; i < m_downloadedModifierModel->count(); ++i) {
        DownloadedModifierInfo info = m_downloadedModifierModel->getModifier(i);
        QJsonObject obj;
        obj["name"] = info.name;
        obj["version"] = info.version;
        obj["gameVersion"] = info.gameVersion;
        obj["downloadDate"] = info.downloadDate.toString(Qt::ISODate);
        obj["filePath"] = info.filePath;
        obj["url"] = info.url;
        array.append(obj);
    }
    
    QJsonDocument doc(array);
    file.write(doc.toJson());
    file.close();
}

// Load game name mapping database
void Backend::loadGameMappings()
{
    m_gameMappings.clear();
    
    QFile file(":/resources/game_mappings.json");
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Backend: Cannot open game mappings file";
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        qWarning() << "Backend: Invalid game mappings format";
        return;
    }
    
    QJsonObject rootObj = doc.object();
    QJsonObject mappings = rootObj["mappings"].toObject();
    
    for (const QString& chineseName : mappings.keys()) {
        QJsonObject gameObj = mappings[chineseName].toObject();
        
        GameMapping mapping;
        mapping.chineseName = chineseName;
        mapping.englishName = gameObj["english"].toString();
        
        QJsonArray aliasArray = gameObj["aliases"].toArray();
        for (const QJsonValue& val : aliasArray) {
            mapping.aliases.append(val.toString());
        }
        
        m_gameMappings.append(mapping);
    }
}

// Get search suggestions - supports fuzzy matching for Chinese and English
QStringList Backend::getSuggestions(const QString& keyword, int maxResults)
{
    QStringList results;
    
    if (keyword.isEmpty()) {
        return results;
    }
    
    QString lowerKeyword = keyword.toLower();
    
    // Track added names to avoid duplicates
    QSet<QString> addedNames;
    
    for (const GameMapping& mapping : m_gameMappings) {
        if (results.size() >= maxResults) break;
        
        bool matched = false;
        QString displayName;
        
        // Check Chinese name match
        if (mapping.chineseName.toLower().contains(lowerKeyword)) {
            matched = true;
            displayName = mapping.chineseName;
            if (!mapping.englishName.isEmpty()) {
                displayName += " (" + mapping.englishName + ")";
            }
        }
        // Check English name match
        else if (mapping.englishName.toLower().contains(lowerKeyword)) {
            matched = true;
            displayName = mapping.englishName;
            if (!mapping.chineseName.isEmpty()) {
                displayName += " (" + mapping.chineseName + ")";
            }
        }
        // Check alias match
        else {
            for (const QString& alias : mapping.aliases) {
                if (alias.toLower().contains(lowerKeyword)) {
                    matched = true;
                    displayName = mapping.chineseName + " (" + alias + ")";
                    break;
                }
            }
        }
        
        if (matched && !addedNames.contains(displayName)) {
            results.append(displayName);
            addedNames.insert(displayName);
        }
    }
    
    return results;
}
