#include "Backend.h"
#include <QGuiApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
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
    // Speed calculation timer - fires every second
    m_speedUpdateTimer = new QTimer(this);
    m_speedUpdateTimer->setInterval(1000);
    connect(m_speedUpdateTimer, &QTimer::timeout, this, [this]() {
        if (m_activeDownloadTaskId.isEmpty()) {
            m_speedUpdateTimer->stop();
            return;
        }
        const int idx = findDownloadTaskIndex(m_activeDownloadTaskId);
        if (idx < 0) return;
        
        const qint64 currentBytes = m_downloadTasks[idx].value("bytesReceived").toLongLong();
        const qint64 elapsed = m_speedTimer.elapsed();
        
        qint64 speed = 0;
        if (elapsed > 0) {
            speed = qMax(0LL, (currentBytes - m_lastSpeedBytes) * 1000 / elapsed);
        }
        m_lastSpeedBytes = currentBytes;
        m_speedTimer.restart();
        
        updateDownloadTaskDeferred(m_activeDownloadTaskId, [speed](QVariantMap& task) {
            task["speed"] = speed;
        });
    });

    // Throttle timer for download task UI updates (prevents QML delegate rebuild flooding)
    m_taskUpdateTimer = new QTimer(this);
    m_taskUpdateTimer->setInterval(200);
    connect(m_taskUpdateTimer, &QTimer::timeout, this, [this]() {
        if (m_downloadTasksDirty) {
            m_downloadTasksDirty = false;
            emit downloadTasksChanged();
        }
    });

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

QVariantList Backend::downloadTasks() const
{
    QVariantList list;
    for (const QVariantMap& task : m_downloadTasks) {
        list.append(task);
    }
    return list;
}

void Backend::searchModifiers(const QString& keyword)
{
    emit statusMessage(tr("Searching: %1").arg(keyword));
    const quint64 requestId = beginSearchRequest();
    SearchManager::getInstance().searchModifiers(
        keyword,
        [this, requestId](const QList<ModifierInfo>& modifiers) {
            finishSearchRequest(requestId, modifiers);
        });
}

void Backend::fetchRecentModifiers()
{
    emit statusMessage(tr("Loading modifiers..."));
    const quint64 requestId = beginSearchRequest();
    SearchManager::getInstance().fetchRecentlyUpdatedModifiers(
        [this, requestId](const QList<ModifierInfo>& modifiers) {
            finishSearchRequest(requestId, modifiers);
        });
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
    
    const QString savePath = downloadDir + "/" + sanitizedName + "_" + versionName + ".zip";
    const QString taskId = createDownloadTask(m_selectedModifier, versionName, savePath);
    emit statusMessage(tr("Added to download queue: %1").arg(m_selectedModifier.name));
    qDebug() << "Backend: queued download task:" << taskId << "for" << m_selectedModifier.name;
    processNextDownloadTask();
}

void Backend::pauseDownload(const QString& taskId)
{
    const int index = findDownloadTaskIndex(taskId);
    if (index < 0) {
        return;
    }
    
    const QString status = m_downloadTasks[index].value("status").toString();
    if (status == "queued") {
        updateDownloadTask(taskId, [](QVariantMap& task) {
            task["status"] = "paused";
            task["resumeRequested"] = true;
        });
        return;
    }
    
    if (status != "downloading") {
        return;
    }
    
    updateDownloadTask(taskId, [](QVariantMap& task) {
        task["status"] = "paused";
        task["resumeRequested"] = true;
    });
    DownloadManager::getInstance().cancelDownload();
}

void Backend::resumeDownload(const QString& taskId)
{
    const int index = findDownloadTaskIndex(taskId);
    if (index < 0) {
        return;
    }
    
    const QString status = m_downloadTasks[index].value("status").toString();
    if (status != "paused" && status != "failed") {
        return;
    }
    
    const bool canResumeFromFile = m_downloadTaskMeta.contains(taskId) && QFile::exists(m_downloadTaskMeta.value(taskId).tempPath);
    if (status == "failed") {
        updateDownloadTask(taskId, [canResumeFromFile](QVariantMap& task) {
            task["status"] = "queued";
            task["resumeRequested"] = canResumeFromFile;
            task["errorMessage"] = QString();
        });
    }
    
    if (m_activeDownloadTaskId.isEmpty()) {
        if (status == "paused") {
            startDownloadTask(taskId);
        } else {
            processNextDownloadTask();
        }
        return;
    }
    
    if (status == "paused") {
        updateDownloadTask(taskId, [](QVariantMap& task) {
            task["status"] = "queued";
            task["resumeRequested"] = true;
            task["errorMessage"] = QString();
        });
    }
}

void Backend::cancelDownload(const QString& taskId)
{
    const int index = findDownloadTaskIndex(taskId);
    if (index < 0) {
        return;
    }
    
    const QString status = m_downloadTasks[index].value("status").toString();
    if (status == "completed" || status == "failed" || status == "canceled") {
        return;
    }
    
    updateDownloadTask(taskId, [](QVariantMap& task) {
        task["status"] = "canceled";
    });
    
    if (taskId == m_activeDownloadTaskId) {
        DownloadManager::getInstance().cancelDownload();
        return;
    }
    
    const DownloadTaskMeta meta = m_downloadTaskMeta.value(taskId);
    if (QFile::exists(meta.tempPath)) {
        QFile::remove(meta.tempPath);
    }
    
    processNextDownloadTask();
}

void Backend::removeDownloadTask(const QString& taskId)
{
    const int index = findDownloadTaskIndex(taskId);
    if (index < 0) {
        return;
    }
    
    const QString status = m_downloadTasks[index].value("status").toString();
    if (status == "downloading") {
        return;
    }
    
    m_downloadTasks.removeAt(index);
    m_downloadTaskMeta.remove(taskId);
    emit downloadTasksChanged();
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
    // Keep m_downloadedList in sync with the model
    if (index >= 0 && index < m_downloadedList.size()) {
        m_downloadedList.removeAt(index);
    }
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

QString Backend::createDownloadTask(const ModifierInfo& modifier,
                                    const QString& versionName,
                                    const QString& savePath)
{
    m_nextDownloadTaskId++;
    const QString taskId = QString("task_%1").arg(m_nextDownloadTaskId);
    
    QVariantMap task;
    task["taskId"] = taskId;
    task["fileName"] = modifier.name;
    task["status"] = "queued";
    task["progress"] = 0.0;
    task["bytesReceived"] = 0;
    task["bytesTotal"] = 0;
    task["version"] = versionName;
    task["savePath"] = savePath;
    task["errorMessage"] = QString();
    task["resumeRequested"] = false;
    task["createdAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    m_downloadTasks.append(task);
    
    DownloadTaskMeta meta;
    meta.taskId = taskId;
    meta.modifier = modifier;
    meta.versionName = versionName;
    meta.savePath = savePath;
    meta.tempPath = savePath + ".crdownload";
    m_downloadTaskMeta.insert(taskId, meta);
    
    emit downloadTasksChanged();
    return taskId;
}

void Backend::processNextDownloadTask()
{
    if (!m_activeDownloadTaskId.isEmpty()) {
        return;
    }
    
    int nextIndex = -1;
    for (int i = 0; i < m_downloadTasks.size(); ++i) {
        if (m_downloadTasks[i].value("status").toString() == "queued") {
            nextIndex = i;
            break;
        }
    }
    
    if (nextIndex < 0) {
        if (m_isDownloading) {
            m_isDownloading = false;
            emit downloadingChanged();
        }
        return;
    }
    
    startDownloadTask(m_downloadTasks[nextIndex].value("taskId").toString());
}

void Backend::startDownloadTask(const QString& taskId)
{
    const int index = findDownloadTaskIndex(taskId);
    if (index < 0 || !m_downloadTaskMeta.contains(taskId)) {
        return;
    }
    
    const QString status = m_downloadTasks[index].value("status").toString();
    const bool resumeRequested = m_downloadTasks[index].value("resumeRequested").toBool();
    if (status != "queued" && status != "paused") {
        return;
    }
    
    DownloadTaskMeta meta = m_downloadTaskMeta.value(taskId);
    qint64 resumeFrom = 0;
    
    // Use .crdownload temp file for resume
    if ((status == "paused" || resumeRequested) && QFile::exists(meta.tempPath)) {
        resumeFrom = QFileInfo(meta.tempPath).size();
    } else {
        QFile::remove(meta.tempPath);
        QFile::remove(meta.savePath);
    }
    
    m_activeDownloadTaskId = taskId;
    if (!m_isDownloading) {
        m_isDownloading = true;
        emit downloadingChanged();
    }
    m_downloadProgress = 0.0;
    emit downloadProgressChanged();
    
    // Start speed tracking
    m_lastSpeedBytes = resumeFrom;
    m_speedTimer.start();
    m_speedUpdateTimer->start();
    m_taskUpdateTimer->start();
    
    updateDownloadTask(taskId, [resumeFrom](QVariantMap& task) {
        task["status"] = "downloading";
        task["bytesReceived"] = resumeFrom;
        task["errorMessage"] = QString();
        task["resumeRequested"] = false;
        task["speed"] = 0;
    });
    
    // Download to .crdownload temp file
    ModifierManager::getInstance().downloadModifier(
        meta.modifier,
        meta.versionName,
        meta.tempPath,
        [this, taskId, versionName = meta.versionName](bool success, const QString& errorMsg, const QString& filePath, const ModifierInfo& modifier, bool isArchive) {
            Q_UNUSED(isArchive)
            const int taskIndex = findDownloadTaskIndex(taskId);
            if (taskIndex < 0) {
                m_activeDownloadTaskId.clear();
                processNextDownloadTask();
                return;
            }
            
            const QString currentStatus = m_downloadTasks[taskIndex].value("status").toString();
            
            if (currentStatus == "paused") {
                m_speedUpdateTimer->stop();
                m_taskUpdateTimer->stop();
                if (m_downloadTasksDirty) {
                    m_downloadTasksDirty = false;
                    emit downloadTasksChanged();
                }
                updateDownloadTask(taskId, [](QVariantMap& task) {
                    task["speed"] = 0;
                });
                m_activeDownloadTaskId.clear();
                m_isDownloading = false;
                emit downloadingChanged();
                m_downloadProgress = 0.0;
                emit downloadProgressChanged();
                processNextDownloadTask();
                return;
            }
            
            if (currentStatus == "canceled") {
                m_speedUpdateTimer->stop();
                m_taskUpdateTimer->stop();
                if (m_downloadTasksDirty) {
                    m_downloadTasksDirty = false;
                    emit downloadTasksChanged();
                }
                const DownloadTaskMeta taskMeta = m_downloadTaskMeta.value(taskId);
                // Delete .crdownload temp file on cancel
                if (QFile::exists(taskMeta.tempPath)) {
                    QFile::remove(taskMeta.tempPath);
                }
                m_activeDownloadTaskId.clear();
                m_isDownloading = false;
                emit downloadingChanged();
                m_downloadProgress = 0.0;
                emit downloadProgressChanged();
                processNextDownloadTask();
                return;
            }
            
            if (success) {
                // Rename .crdownload to final file
                const DownloadTaskMeta taskMeta = m_downloadTaskMeta.value(taskId);
                QString finalPath = taskMeta.savePath;
                
                // filePath may have been corrected by DownloadManager (extension fix)
                // In that case the temp file was already renamed by DownloadManager,
                // so we need to check if the temp-based corrected path exists
                QString correctedTempPath = filePath; // This is the path after correctFileExtension
                
                // If the corrected path is the temp path itself, rename to final
                if (correctedTempPath.endsWith(".crdownload")) {
                    // Remove .crdownload suffix to get the actual final path
                    finalPath = correctedTempPath.left(correctedTempPath.length() - 11);
                    QFile::remove(finalPath); // Remove existing file if any
                    QFile::rename(correctedTempPath, finalPath);
                } else {
                    // DownloadManager already renamed it (extension correction),
                    // just use the returned path
                    finalPath = correctedTempPath;
                }
                
                if (m_downloadTaskMeta.contains(taskId)) {
                    DownloadTaskMeta updatedMeta = m_downloadTaskMeta.value(taskId);
                    updatedMeta.savePath = finalPath;
                    m_downloadTaskMeta.insert(taskId, updatedMeta);
                }
                
                updateDownloadTask(taskId, [finalPath](QVariantMap& task) {
                    task["status"] = "completed";
                    task["progress"] = 1.0;
                    const qint64 fileSize = QFileInfo(finalPath).size();
                    task["bytesReceived"] = fileSize;
                    if (task.value("bytesTotal").toLongLong() <= 0) {
                        task["bytesTotal"] = fileSize;
                    }
                    task["savePath"] = finalPath;
                    task["speed"] = 0;
                });
                
                DownloadedModifierInfo downloadedInfo;
                downloadedInfo.name = modifier.name;
                downloadedInfo.version = versionName;
                downloadedInfo.gameVersion = modifier.gameVersion;
                downloadedInfo.downloadDate = QDateTime::currentDateTime();
                downloadedInfo.filePath = filePath;
                downloadedInfo.url = modifier.url;
                
                m_downloadedList.append(downloadedInfo);
                m_downloadedModifierModel->setModifiers(m_downloadedList);
                saveDownloadedModifiers();
                
                m_downloadProgress = 1.0;
                emit downloadProgressChanged();
                emit downloadCompleted(true);
                emit statusMessage(tr("Download complete: %1").arg(filePath));
            } else {
                updateDownloadTask(taskId, [errorMsg](QVariantMap& task) {
                    task["status"] = "failed";
                    task["errorMessage"] = errorMsg;
                });
                m_downloadProgress = 0.0;
                emit downloadProgressChanged();
                emit downloadCompleted(false);
                emit statusMessage(tr("Download failed: %1").arg(errorMsg));
            }
            
            m_speedUpdateTimer->stop();
            m_taskUpdateTimer->stop();
            if (m_downloadTasksDirty) {
                m_downloadTasksDirty = false;
                emit downloadTasksChanged();
            }
            m_activeDownloadTaskId.clear();
            m_isDownloading = false;
            emit downloadingChanged();
            processNextDownloadTask();
        },
        [this, taskId](qint64 bytesReceived, qint64 bytesTotal) {
            // Always update progress even when bytesTotal is unknown
            qreal progress;
            if (bytesTotal > 0) {
                progress = qBound<qreal>(0.0, static_cast<qreal>(bytesReceived) / bytesTotal, 1.0);
            } else {
                progress = -1.0; // Indeterminate: total size unknown
            }
            
            updateDownloadTaskDeferred(taskId, [progress, bytesReceived, bytesTotal](QVariantMap& task) {
                task["progress"] = progress;
                task["bytesReceived"] = bytesReceived;
                if (bytesTotal > 0) {
                    task["bytesTotal"] = bytesTotal;
                }
            });
            
            if (taskId == m_activeDownloadTaskId) {
                m_downloadProgress = (progress >= 0) ? progress : 0.0;
                emit downloadProgressChanged();
            }
        },
        resumeFrom,
        true
    );
}

int Backend::findDownloadTaskIndex(const QString& taskId) const
{
    for (int i = 0; i < m_downloadTasks.size(); ++i) {
        if (m_downloadTasks[i].value("taskId").toString() == taskId) {
            return i;
        }
    }
    return -1;
}

void Backend::updateDownloadTask(const QString& taskId, const std::function<void(QVariantMap&)>& updater)
{
    const int index = findDownloadTaskIndex(taskId);
    if (index < 0) {
        return;
    }
    
    QVariantMap task = m_downloadTasks[index];
    updater(task);
    m_downloadTasks[index] = task;
    emit downloadTasksChanged();
}

void Backend::updateDownloadTaskDeferred(const QString& taskId, const std::function<void(QVariantMap&)>& updater)
{
    const int index = findDownloadTaskIndex(taskId);
    if (index < 0) {
        return;
    }
    
    QVariantMap task = m_downloadTasks[index];
    updater(task);
    m_downloadTasks[index] = task;
    m_downloadTasksDirty = true;  // Will be flushed by m_taskUpdateTimer
}

quint64 Backend::beginSearchRequest()
{
    m_nextSearchRequestId++;
    m_activeSearchRequestId = m_nextSearchRequestId;
    if (!m_searchLoading) {
        m_searchLoading = true;
        emit searchLoadingChanged();
    }
    return m_activeSearchRequestId;
}

void Backend::finishSearchRequest(quint64 requestId, const QList<ModifierInfo>& modifiers)
{
    // Ignore stale results if a newer request is already in flight.
    if (requestId != m_activeSearchRequestId) {
        return;
    }

    m_modifierListModel->setModifiers(modifiers);
    emit searchCompleted();
    emit statusMessage(tr("Found %1 modifiers").arg(modifiers.size()));

    if (m_searchLoading) {
        m_searchLoading = false;
        emit searchLoadingChanged();
    }
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
    QString filePath = FileSystem::getInstance().getDataDirectory() + "/downloaded_modifiers.json";
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
    
    m_downloadedList = list;
    m_downloadedModifierModel->setModifiers(list);
}

void Backend::saveDownloadedModifiers()
{
    QString filePath = FileSystem::getInstance().getDataDirectory() + "/downloaded_modifiers.json";
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
