#include "Backend.h"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include "FileSystem.h"
#include "ConfigManager.h"
#include "ThemeManager.h"
#include "LanguageManager.h"
#include "DownloadManager.h"
#include "TranslationDatabase.h"
#include "TranslationTextUtils.h"
#include "Logger.h"
#include <QSet>

namespace {
// Make a string safe to use as a single path component: strip characters
// Windows rejects, collapse separators, and refuse leading/trailing dots.
// Both halves of a download file name go through this - the version label is
// scraped from the site and is no more trustworthy than the modifier name.
QString sanitizePathComponent(const QString& text, const QString& fallback)
{
    QString sanitized = text;
    sanitized.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
    // Control characters are illegal in NTFS names and never intentional here.
    sanitized.replace(QRegularExpression("[\\x00-\\x1F]"), "_");
    sanitized = sanitized.trimmed();
    while (sanitized.startsWith(".")) {
        sanitized.remove(0, 1);
    }
    while (sanitized.endsWith(".")) {
        sanitized.chop(1);
    }
    sanitized = sanitized.trimmed();
    return sanitized.isEmpty() ? fallback : sanitized;
}

bool containsJapaneseScript(const QString& text)
{
    for (const QChar& ch : text) {
        const ushort code = ch.unicode();
        if ((code >= 0x3040 && code <= 0x309F) || (code >= 0x30A0 && code <= 0x30FF)) {
            return true;
        }
    }
    return false;
}

bool containsLatinLetterOrDigit(const QString& text)
{
    for (const QChar& ch : text) {
        if (ch.isLetterOrNumber() && ch.unicode() < 0x80) {
            return true;
        }
    }
    return false;
}

bool isEnglishLikeInput(const QString& text)
{
    return containsLatinLetterOrDigit(text) &&
           !GameMappingManager::getInstance().containsChinese(text) &&
           !containsJapaneseScript(text);
}

QString makeSuggestionDisplayText(const QString& chineseName,
                                  const QString& englishName,
                                  const QString& japaneseName,
                                  const QString& query,
                                  bool chineseMatched,
                                  bool englishMatched,
                                  bool normalizedEnglishMatched,
                                  bool japaneseMatched,
                                  ConfigManager::Language language)
{
    const bool englishInput = isEnglishLikeInput(query);

    auto pickLocalizedSecondary = [&]() -> QString {
        if (language == ConfigManager::Language::Japanese && !japaneseName.isEmpty()) {
            return japaneseName;
        }
        if (language == ConfigManager::Language::Chinese && !chineseName.isEmpty()) {
            return chineseName;
        }
        if (language == ConfigManager::Language::Japanese && !chineseName.isEmpty()) {
            return chineseName;
        }
        if (language == ConfigManager::Language::Chinese && !japaneseName.isEmpty()) {
            return japaneseName;
        }
        return QString();
    };

    if (language == ConfigManager::Language::English) {
        return englishName;
    }

    if (englishInput || englishMatched || normalizedEnglishMatched) {
        const QString secondary = pickLocalizedSecondary();
        return secondary.isEmpty() ? englishName
                                   : QStringLiteral("%1 (%2)").arg(englishName, secondary);
    }

    if (japaneseMatched && !japaneseName.isEmpty()) {
        return englishName.isEmpty()
                   ? japaneseName
                   : QStringLiteral("%1 (%2)").arg(japaneseName, englishName);
    }

    if (chineseMatched && !chineseName.isEmpty()) {
        return englishName.isEmpty()
                   ? chineseName
                   : QStringLiteral("%1 (%2)").arg(chineseName, englishName);
    }

    const QString localizedSecondary = pickLocalizedSecondary();
    return localizedSecondary.isEmpty()
               ? englishName
               : QStringLiteral("%1 (%2)").arg(englishName, localizedSecondary);
}

QString makeSuggestionInputText(const QString& chineseName,
                                const QString& englishName,
                                const QString& japaneseName,
                                const QString& query,
                                bool chineseMatched,
                                bool englishMatched,
                                bool normalizedEnglishMatched,
                                bool japaneseMatched,
                                ConfigManager::Language language)
{
    const bool englishInput = isEnglishLikeInput(query);

    if (language == ConfigManager::Language::English) {
        return englishName;
    }

    if (englishInput || englishMatched || normalizedEnglishMatched) {
        return englishName;
    }

    if (japaneseMatched && !japaneseName.isEmpty()) {
        return japaneseName;
    }

    if (chineseMatched && !chineseName.isEmpty()) {
        return chineseName;
    }

    if (language == ConfigManager::Language::Japanese && !japaneseName.isEmpty()) {
        return japaneseName;
    }

    if (language == ConfigManager::Language::Chinese && !chineseName.isEmpty()) {
        return chineseName;
    }

    return englishName;
}
}

Backend::Backend(QObject* parent)
    : QObject(parent)
    , m_modifierListModel(new ModifierListModel(this))
    , m_downloadedModifierModel(new DownloadedModifierModel(this))
    , m_coverExtractor(new CoverExtractor(this))
    , m_appUpdateManager(new AppUpdateManager(this))
    , m_databaseUpdateManager(new DatabaseUpdateManager(this))
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
    refreshCurrentDatabaseVersion();
    loadDownloadedModifiers();
    fetchRecentModifiers();

    if (autoCheckAppUpdates()) {
        QTimer::singleShot(1500, this, [this]() {
            checkAppUpdate();
        });
    }

    if (autoCheckDatabaseUpdates()) {
        QTimer::singleShot(2200, this, [this]() {
            checkDatabaseUpdate();
        });
    }
}

Backend::~Backend()
{
    saveDownloadedModifiers();
}

int Backend::currentLanguage() const
{
    return static_cast<int>(ConfigManager::getInstance().getCurrentLanguage());
}

QString Backend::appVersion() const
{
    return QCoreApplication::applicationVersion();
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

QVariantList Backend::downloadTasks() const
{
    QVariantList list;
    for (const QVariantMap& task : m_downloadTasks) {
        list.append(task);
    }
    return list;
}

bool Backend::autoCheckAppUpdates() const
{
    return ConfigManager::getInstance().getAutoCheckUpdates();
}

bool Backend::autoCheckDatabaseUpdates() const
{
    return ConfigManager::getInstance().getAutoCheckDatabaseUpdates();
}

int Backend::updateSource() const
{
    return ConfigManager::getInstance().getUpdateSource() == QLatin1String("gitee") ? 1 : 0;
}

void Backend::setUpdateSource(int index)
{
    const QString source = (index == 1) ? QStringLiteral("gitee") : QStringLiteral("github");
    if (ConfigManager::getInstance().getUpdateSource() == source) {
        return;
    }
    ConfigManager::getInstance().setUpdateSource(source);
    emit updateSourceChanged();
}

void Backend::searchModifiers(const QString& keyword)
{
    const quint64 requestId = beginSearchRequest();
    SearchManager::getInstance().searchModifiers(
        keyword,
        [this, requestId](const QList<ModifierInfo>& modifiers) {
            finishSearchRequest(requestId, modifiers);
        });
}

void Backend::fetchRecentModifiers()
{
    const quint64 requestId = beginSearchRequest();
    SearchManager::getInstance().fetchRecentlyUpdatedModifiers(
        [this, requestId](const QList<ModifierInfo>& modifiers) {
            finishSearchRequest(requestId, modifiers);
        });
}

void Backend::applySortOrder(QList<ModifierInfo>& modifiers) const
{
    switch (m_sortOrder) {
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
        default:
            break;
    }
}

void Backend::setSortOrder(int sortIndex)
{
    // Remember the choice: sorting only the current snapshot meant the next
    // search installed unsorted rows while the selector still showed the
    // order the user had picked.
    m_sortOrder = sortIndex;
    
    QList<ModifierInfo> modifiers = m_modifierListModel->getAllModifiers();
    applySortOrder(modifiers);
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

    // Drop the list row's stale detail payload before the request goes out:
    // until it returns, any versions or options on screen would belong to
    // whatever was selected before.
    m_selectedModifier.versions.clear();
    m_selectedModifier.options.clear();
    m_selectedOptions.clear();

    emit selectedModifierChanged();
    emit selectedModifierOptionsChanged();

    // Resolve the cover immediately from the name (known now, before the detail
    // request returns): show a cached cover instantly, otherwise clear the old
    // cover and switch to the loading state so we never display the previous
    // modifier's cover.
    const QString gameId = coverGameId(m_selectedModifier.name);
    m_coverRequestId = gameId;
    if (!gameId.isEmpty() && !CoverExtractor::getCachedCover(gameId).isNull()) {
        m_currentCoverPath = "file:///" + CoverExtractor::getCacheDirectory()
                             + "/" + gameId + ".png";
        m_coverLoading = false;
    } else {
        m_currentCoverPath.clear();
        m_coverLoading = true;
    }
    emit coverExtracted();
    emit coverLoadingChanged();
    
    requestSelectedModifierDetail();
}

void Backend::retrySelectedModifierDetail()
{
    if (m_selectedModifier.url.isEmpty()) {
        return;
    }
    requestSelectedModifierDetail();
}

void Backend::setDetailState(const QString& state)
{
    if (m_detailState == state) {
        return;
    }
    m_detailState = state;
    emit detailStateChanged();
}

void Backend::stopCoverLoading()
{
    if (m_coverLoading) {
        m_coverLoading = false;
        emit coverLoadingChanged();
    }
}

void Backend::requestSelectedModifierDetail()
{
    if (m_selectedModifier.url.isEmpty()) {
        // Nothing to fetch, so the drawer must not sit on a spinner.
        setDetailState(QStringLiteral("empty"));
        stopCoverLoading();
        return;
    }

    // Tag the request so a reply that arrives after the user has moved on can
    // be recognised and dropped: it carries another game's versions, options
    // and screenshot, and writing those into the current selection would let a
    // download combine this game's name with that game's URL.
    const quint64 requestId = ++m_nextDetailRequestId;
    m_activeDetailRequestId = requestId;
    const QString requestedUrl = m_selectedModifier.url;
    setDetailState(QStringLiteral("loading"));

    ModifierManager::getInstance().getModifierDetail(
        requestedUrl,
        [this, requestId, requestedUrl](ModifierInfo* modifier, bool success) {
            if (requestId != m_activeDetailRequestId
                || requestedUrl != m_selectedModifier.url) {
                delete modifier;
                return;
            }

            if (!success || !modifier) {
                delete modifier;
                setDetailState(QStringLiteral("error"));
                stopCoverLoading();
                return;
            }

            m_selectedModifier.versions = modifier->versions;
            m_selectedModifier.options = modifier->options;
            m_selectedModifier.optionsCount = modifier->optionsCount;
            m_selectedModifier.screenshotUrl = modifier->screenshotUrl;
            m_selectedModifier.description = modifier->description;
            delete modifier;

            m_selectedOptions = m_selectedModifier.options.join("\n");

            emit selectedModifierChanged();
            emit selectedModifierOptionsChanged();

            // A page can parse correctly and still offer no downloads; that is
            // a finished request, not a failed one, and its metadata is still
            // worth showing.
            setDetailState(m_selectedModifier.versions.isEmpty()
                               ? QStringLiteral("empty")
                               : QStringLiteral("ready"));

            extractCover();
        }
    );
}

void Backend::selectVersion(int versionIndex)
{
    m_selectedVersionIndex = versionIndex;
}

void Backend::downloadModifier(int versionIndex)
{
    if (m_selectedModifier.versions.isEmpty()) {
        return;
    }
    
    int idx = (versionIndex >= 0 && versionIndex < m_selectedModifier.versions.size()) 
              ? versionIndex : 0;
    
    QString versionName = m_selectedModifier.versions.at(idx).first;
    QString downloadDir = ConfigManager::getInstance().getDownloadDirectory();
    
    // Both components are remote-derived, so both are sanitized: an unescaped
    // separator in either one would place the file outside downloadDir.
    const QString sanitizedName =
        sanitizePathComponent(m_selectedModifier.name, QStringLiteral("trainer"));
    const QString sanitizedVersion =
        sanitizePathComponent(versionName, QStringLiteral("version"));
    
    const QString savePath = downloadDir + "/" + sanitizedName + "_" + sanitizedVersion + ".zip";
    const QString taskId = createDownloadTask(m_selectedModifier, versionName, savePath);
    LOG_DEBUG() << "Backend: queued download task:" << taskId << "for" << m_selectedModifier.name;
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
QString Backend::coverGameId(const QString& name)
{
    QString gameId = name;
    gameId.replace(QRegularExpression("[^a-zA-Z0-9]"), "_");
    return gameId;
}

void Backend::extractCover()
{
    const QString gameId = coverGameId(m_selectedModifier.name);

    // Cached covers were already shown synchronously in selectModifier().
    if (!gameId.isEmpty() && !CoverExtractor::getCachedCover(gameId).isNull()) {
        return;
    }

    if (m_selectedModifier.screenshotUrl.isEmpty()) {
        // Nothing to fetch: stop the spinner and fall back to "暂无封面".
        if (m_coverLoading) {
            m_coverLoading = false;
            emit coverLoadingChanged();
        }
        return;
    }


    m_coverExtractor->extractCoverFromTrainerImage(
        m_selectedModifier.screenshotUrl,
        [this, gameId](const QPixmap& cover, bool success) {
            // Ignore results for a modifier the user has already navigated away
            // from, so a late download never overwrites the current selection.
            if (gameId != m_coverRequestId) {
                return;
            }

            if (success && !cover.isNull() && CoverExtractor::saveCoverToCache(gameId, cover)) {
                m_currentCoverPath = "file:///" + CoverExtractor::getCacheDirectory()
                                     + "/" + gameId + ".png";
                emit coverExtracted();
            } else {
                m_currentCoverPath.clear();
                emit coverExtracted();
            }

            m_coverLoading = false;
            emit coverLoadingChanged();
        }
    );
}

void Backend::openDownloadFolder()
{
    QString downloadDir = ConfigManager::getInstance().getDownloadDirectory();
    QDesktopServices::openUrl(QUrl::fromLocalFile(downloadDir));
}

void Backend::openContainingFolder(const QString& filePath) const
{
    // Fall back to the configured directory only when the item has no usable
    // path of its own - opening the current default for an item downloaded
    // elsewhere sends the user to the wrong folder.
    const QString directory = filePath.isEmpty()
        ? QString()
        : QFileInfo(filePath).absolutePath();

    if (!directory.isEmpty() && QDir(directory).exists()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
        return;
    }

    QDesktopServices::openUrl(
        QUrl::fromLocalFile(ConfigManager::getInstance().getDownloadDirectory()));
}

void Backend::openModifierFolder(int index)
{
    if (index < 0 || index >= m_downloadedModifierModel->count()) {
        openDownloadFolder();
        return;
    }
    openContainingFolder(m_downloadedModifierModel->getModifier(index).filePath);
}

void Backend::openTaskFolder(const QString& taskId)
{
    const int index = findDownloadTaskIndex(taskId);
    if (index < 0) {
        openDownloadFolder();
        return;
    }
    openContainingFolder(m_downloadTasks.at(index).value("savePath").toString());
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
        m_downloadedModifierModel->removeModifier(index);
        if (index >= 0 && index < m_downloadedList.size()) {
            m_downloadedList.removeAt(index);
        }
        saveDownloadedModifiers();
    }
}

bool Backend::deleteModifier(int index)
{
    if (index < 0 || index >= m_downloadedModifierModel->count()) {
        return false;
    }
    
    DownloadedModifierInfo modifier = m_downloadedModifierModel->getModifier(index);
    
    // A file that is open elsewhere, or in a directory that denies deletion,
    // fails here. Keep the record so the library still points at the file that
    // is genuinely still on disk. A record whose file is already gone can be
    // removed either way.
    if (QFile::exists(modifier.filePath) && !QFile::remove(modifier.filePath)) {
        LOG_WARN() << "Backend: could not delete" << modifier.filePath;
        emit deleteFailed(modifier.name, modifier.filePath);
        return false;
    }
    
    m_downloadedModifierModel->removeModifier(index);
    // Keep m_downloadedList in sync with the model
    if (index >= 0 && index < m_downloadedList.size()) {
        m_downloadedList.removeAt(index);
    }
    saveDownloadedModifiers();
    return true;
}

void Backend::checkAppUpdate()
{
    if (m_appUpdateChecking || m_appUpdateDownloading || !m_appUpdateManager) {
        return;
    }

    m_appUpdateChecking = true;
    m_appUpdateStatusText = tr("Checking for updates...");
    emit appUpdateStateChanged();

    m_appUpdateManager->checkForUpdates(
        appVersion(),
        ConfigManager::getInstance().getUpdateSource(),
        [this](bool success, bool updateAvailable, const AppReleaseInfo& releaseInfo, const QString& errorMessage) {
            m_appUpdateChecking = false;

            if (!success) {
                m_appUpdateAvailable = false;
                m_latestAppRelease = AppReleaseInfo();
                m_appLatestVersion.clear();
                m_appUpdateSource.clear();
                m_appUpdatePublishedAt.clear();
                m_appUpdateStatusText = errorMessage.isEmpty()
                    ? tr("Failed to check for updates")
                    : errorMessage;
                emit appUpdateStateChanged();
                return;
            }

            m_latestAppRelease = releaseInfo;
            m_appUpdateAvailable = updateAvailable;
            m_appLatestVersion = releaseInfo.version;
            m_appUpdateSource = releaseInfo.source;
            m_appUpdatePublishedAt = releaseInfo.publishedAt;
            m_appUpdateStatusText = updateAvailable
                ? tr("New version available: %1").arg(releaseInfo.version)
                : tr("You are using the latest version");

            emit appUpdateStateChanged();
        });
}

void Backend::downloadAppUpdate()
{
    if (!m_appUpdateAvailable || m_appUpdateDownloading || !m_latestAppRelease.isValid() || !m_appUpdateManager) {
        return;
    }

    m_appUpdateDownloading = true;
    m_appUpdateProgress = 0.0;
    m_appUpdateStatusText = tr("Downloading installer...");
    emit appUpdateStateChanged();

    m_appUpdateManager->downloadInstaller(
        m_latestAppRelease,
        [this](qint64 bytesReceived, qint64 bytesTotal) {
            if (bytesTotal > 0) {
                m_appUpdateProgress = qBound<qreal>(
                    0.0,
                    static_cast<qreal>(bytesReceived) / static_cast<qreal>(bytesTotal),
                    1.0);
            }
            emit appUpdateStateChanged();
        },
        [this](bool success, const QString& installerPath, const QString& errorMessage) {
            m_appUpdateDownloading = false;

            if (!success) {
                m_appUpdateProgress = 0.0;
                m_appUpdateStatusText = errorMessage.isEmpty()
                    ? tr("Failed to download installer")
                    : errorMessage;
                emit appUpdateStateChanged();
                return;
            }

            m_appUpdateProgress = 1.0;
            m_appUpdateStatusText = tr("Installer downloaded. Launching setup...");
            emit appUpdateStateChanged();

            const bool launched = QProcess::startDetached(installerPath, QStringList());
            if (!launched) {
                m_appUpdateStatusText = tr("Failed to launch installer");
                emit appUpdateStateChanged();
                return;
            }

            QTimer::singleShot(300, QCoreApplication::instance(), []() {
                QCoreApplication::quit();
            });
        });
}

void Backend::checkDatabaseUpdate()
{
    if (m_databaseUpdateChecking || m_databaseUpdateDownloading || !m_databaseUpdateManager) {
        return;
    }

    refreshCurrentDatabaseVersion();
    m_databaseUpdateChecking = true;
    m_databaseUpdateStatusText = tr("Checking database updates...");
    emit databaseUpdateStateChanged();

    m_databaseUpdateManager->checkForUpdates(
        m_databaseCurrentVersion,
        ConfigManager::getInstance().getUpdateSource(),
        [this](bool success,
               bool updateAvailable,
               const DatabaseReleaseInfo& releaseInfo,
               const QString& errorMessage) {
            m_databaseUpdateChecking = false;

            if (!success) {
                m_databaseUpdateAvailable = false;
                m_latestDatabaseRelease = DatabaseReleaseInfo();
                m_databaseLatestVersion.clear();
                m_databaseUpdateSource.clear();
                m_databaseUpdatePublishedAt.clear();
                m_databaseUpdateStatusText = errorMessage.isEmpty()
                    ? tr("Failed to check database updates")
                    : errorMessage;
                emit databaseUpdateStateChanged();
                return;
            }

            m_latestDatabaseRelease = releaseInfo;
            m_databaseUpdateAvailable = updateAvailable;
            m_databaseLatestVersion = releaseInfo.version;
            m_databaseUpdateSource = releaseInfo.source;
            m_databaseUpdatePublishedAt = releaseInfo.publishedAt;
            m_databaseUpdateStatusText = updateAvailable
                ? tr("New database version available: %1").arg(releaseInfo.version)
                : tr("You are using the latest database version");

            emit databaseUpdateStateChanged();
        });
}

void Backend::downloadDatabaseUpdate()
{
    if (!m_databaseUpdateAvailable || m_databaseUpdateDownloading
        || !m_latestDatabaseRelease.isValid() || !m_databaseUpdateManager) {
        return;
    }

    m_databaseUpdateDownloading = true;
    m_databaseUpdateProgress = 0.0;
    m_databaseUpdateStatusText = tr("Downloading database update...");
    emit databaseUpdateStateChanged();

    m_databaseUpdateManager->downloadDatabase(
        m_latestDatabaseRelease,
        [this](qint64 bytesReceived, qint64 bytesTotal) {
            if (bytesTotal > 0) {
                m_databaseUpdateProgress = qBound<qreal>(
                    0.0,
                    static_cast<qreal>(bytesReceived) / static_cast<qreal>(bytesTotal),
                    1.0);
            }
            emit databaseUpdateStateChanged();
        },
        [this](bool success, const QString& databasePath, const QString& errorMessage) {
            m_databaseUpdateDownloading = false;

            if (!success) {
                m_databaseUpdateProgress = 0.0;
                m_databaseUpdateStatusText = errorMessage.isEmpty()
                    ? tr("Failed to download database update")
                    : errorMessage;
                emit databaseUpdateStateChanged();
                return;
            }

            QString installError;
            if (!TranslationDatabase::getInstance().installOverrideDatabase(databasePath, &installError)) {
                m_databaseUpdateProgress = 0.0;
                m_databaseUpdateStatusText = installError.isEmpty()
                    ? tr("Failed to activate database update")
                    : installError;
                emit databaseUpdateStateChanged();
                return;
            }

            if (!reloadTranslationDatabase()) {
                m_databaseUpdateProgress = 0.0;
                m_databaseUpdateStatusText = tr("Database updated, but failed to reload data");
                emit databaseUpdateStateChanged();
                return;
            }

            refreshCurrentDatabaseVersion();
            m_databaseUpdateAvailable = false;
            m_databaseLatestVersion = m_databaseCurrentVersion;
            m_databaseUpdateProgress = 1.0;
            m_databaseUpdateStatusText =
                tr("Database updated successfully: %1").arg(m_databaseCurrentVersion);
            emit databaseUpdateStateChanged();
        });
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

void Backend::setAutoCheckAppUpdates(bool enabled)
{
    if (enabled == autoCheckAppUpdates()) {
        return;
    }

    ConfigManager::getInstance().setAutoCheckUpdates(enabled);
    emit autoCheckAppUpdatesChanged();
}

void Backend::setAutoCheckDatabaseUpdates(bool enabled)
{
    if (enabled == autoCheckDatabaseUpdates()) {
        return;
    }

    ConfigManager::getInstance().setAutoCheckDatabaseUpdates(enabled);
    emit autoCheckDatabaseUpdatesChanged();
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
    }
    
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
        if (task.value("bytesTotal").toLongLong() <= 0) {
            task["progress"] = -1.0;
        }
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
                processNextDownloadTask();
                return;
            }
            
            if (success) {
                // Step 1: Rename .crdownload temp file to the intended save path
                const DownloadTaskMeta taskMeta = m_downloadTaskMeta.value(taskId);
                QString finalPath = taskMeta.savePath;
                
                // The callback filePath is based on tempPath (.crdownload),
                // so we ignore it and work with our known paths directly.
                if (QFile::exists(taskMeta.tempPath)) {
                    QFile::remove(finalPath); // Remove existing file if any
                    QFile::rename(taskMeta.tempPath, finalPath);
                }
                
                // Step 2: Run extension correction on the clean final path
                // (now operating on a path WITHOUT .crdownload suffix)
                finalPath = DownloadManager::getInstance().correctFileExtension(finalPath);
                
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
                downloadedInfo.filePath = finalPath;
                downloadedInfo.url = modifier.url;
                
                // Upsert by name+version. Appending blindly produced two rows
                // for one file after re-downloading a version, and deleting
                // either one removed the file the other still pointed at.
                int existing = -1;
                for (int i = 0; i < m_downloadedList.size(); ++i) {
                    if (m_downloadedList.at(i).name == downloadedInfo.name
                        && m_downloadedList.at(i).version == downloadedInfo.version) {
                        existing = i;
                        break;
                    }
                }
                
                // Insert or update one row rather than resetting the model:
                // a reset rebuilds every delegate and drops the user's
                // selection while the library is open.
                if (existing >= 0) {
                    m_downloadedList[existing] = downloadedInfo;
                    m_downloadedModifierModel->updateModifier(existing, downloadedInfo);
                } else {
                    m_downloadedList.append(downloadedInfo);
                    m_downloadedModifierModel->addModifier(downloadedInfo);
                }
                saveDownloadedModifiers();
                
            } else {
                updateDownloadTask(taskId, [errorMsg](QVariantMap& task) {
                    task["status"] = "failed";
                    task["errorMessage"] = errorMsg;
                });
            }
            
            m_speedUpdateTimer->stop();
            m_taskUpdateTimer->stop();
            if (m_downloadTasksDirty) {
                m_downloadTasksDirty = false;
                emit downloadTasksChanged();
            }
            m_activeDownloadTaskId.clear();
            m_isDownloading = false;
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

    QList<ModifierInfo> sorted = modifiers;
    applySortOrder(sorted);
    m_modifierListModel->setModifiers(sorted);

    if (m_searchLoading) {
        m_searchLoading = false;
        emit searchLoadingChanged();
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
    bool removedMissingEntries = false;
    
    for (const auto& item : array) {
        QJsonObject obj = item.toObject();
        DownloadedModifierInfo info;
        info.name = obj["name"].toString();
        info.version = obj["version"].toString();
        info.gameVersion = obj["gameVersion"].toString();
        info.downloadDate = QDateTime::fromString(obj["downloadDate"].toString(), Qt::ISODate);
        info.filePath = obj["filePath"].toString();
        info.url = obj["url"].toString();

        if (info.filePath.isEmpty() || !QFile::exists(info.filePath)) {
            removedMissingEntries = true;
            continue;
        }

        list.append(info);
    }
    
    m_downloadedList = list;
    m_downloadedModifierModel->setModifiers(list);

    if (removedMissingEntries) {
        saveDownloadedModifiers();
    }
}

void Backend::saveDownloadedModifiers()
{
    QString filePath = FileSystem::getInstance().getDataDirectory() + "/downloaded_modifiers.json";
    QFile file(filePath);
    
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_WARN() << "Backend: Failed to save downloaded modifiers";
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

// Load game names from the bundled translation database.
void Backend::loadGameMappings()
{
    m_gameMappings.clear();

    const QList<TranslationGameRecord> records = TranslationDatabase::getInstance().loadAllGames();
    if (records.isEmpty()) {
        LOG_WARN() << "Backend: No game mappings loaded from translation database";
        return;
    }

    for (const TranslationGameRecord& record : records) {
        if (record.english.isEmpty() && record.chineseSimplified.isEmpty() && record.japanese.isEmpty()) {
            continue;
        }

        GameMapping mapping;
        mapping.chineseName = record.chineseSimplified;
        mapping.englishName = record.english;
        mapping.japaneseName = record.japanese;
        mapping.chineseNameLower = mapping.chineseName.toLower();
        mapping.englishNameLower = mapping.englishName.toLower();
        mapping.japaneseNameLower = mapping.japaneseName.toLower();
        mapping.chineseNameNormalized = TranslationTextUtils::normalizeLookupText(mapping.chineseName);
        mapping.englishNameNormalized = TranslationTextUtils::normalizeLookupText(mapping.englishName);
        mapping.japaneseNameNormalized = TranslationTextUtils::normalizeLookupText(mapping.japaneseName);
        mapping.normalizedEnglish = record.normalizedEnglish.isEmpty()
            ? mapping.englishNameNormalized
            : record.normalizedEnglish.toLower();
        m_gameMappings.append(mapping);
    }

    LOG_DEBUG() << "Backend: Loaded" << m_gameMappings.size() << "game mappings from SQLite";
}

void Backend::refreshCurrentDatabaseVersion()
{
    m_databaseCurrentVersion = AppUpdateManager::normalizeVersion(
        TranslationDatabase::getInstance().currentReleaseTag());
}

bool Backend::reloadTranslationDatabase()
{
    if (!GameMappingManager::getInstance().reloadMappings()) {
        return false;
    }

    loadGameMappings();
    return true;
}

QVariantList Backend::getSuggestionItems(const QString& keyword, int maxResults)
{
    QVariantList results;

    const QString trimmedKeyword = keyword.trimmed();
    if (trimmedKeyword.isEmpty()) {
        return results;
    }

    const QString normalizedKeyword = TranslationTextUtils::normalizeLookupText(trimmedKeyword);
    if (normalizedKeyword.isEmpty()) {
        return results;
    }

    const QString lowerKeyword = trimmedKeyword.toLower();
    const ConfigManager::Language language = ConfigManager::getInstance().getCurrentLanguage();
    QSet<QString> addedSearchKeywords;

    for (const GameMapping& mapping : m_gameMappings) {
        if (results.size() >= maxResults) {
            break;
        }

        bool matched = false;

        const bool chineseMatched =
            !mapping.chineseName.isEmpty() &&
            (mapping.chineseNameLower.contains(lowerKeyword) ||
             mapping.chineseNameNormalized.contains(normalizedKeyword));
        const bool englishMatched =
            !mapping.englishName.isEmpty() &&
            (mapping.englishNameLower.contains(lowerKeyword) ||
             mapping.englishNameNormalized.contains(normalizedKeyword));
        const bool normalizedEnglishMatched =
            !mapping.normalizedEnglish.isEmpty() &&
            mapping.normalizedEnglish.contains(normalizedKeyword);
        const bool japaneseMatched =
            !mapping.japaneseName.isEmpty() &&
            (mapping.japaneseNameLower.contains(lowerKeyword) ||
             mapping.japaneseNameNormalized.contains(normalizedKeyword));

        if (chineseMatched) {
            matched = true;
        }
        else if (englishMatched || normalizedEnglishMatched) {
            matched = true;
        }
        else if (japaneseMatched) {
            matched = true;
        }

        if (matched) {
            const QString searchKeyword = mapping.englishName;
            if (searchKeyword.isEmpty() || addedSearchKeywords.contains(searchKeyword)) {
                continue;
            }

            QVariantMap item;
            item["displayText"] = makeSuggestionDisplayText(
                mapping.chineseName,
                mapping.englishName,
                mapping.japaneseName,
                trimmedKeyword,
                chineseMatched,
                englishMatched,
                normalizedEnglishMatched,
                japaneseMatched,
                language);
            item["inputText"] = makeSuggestionInputText(
                mapping.chineseName,
                mapping.englishName,
                mapping.japaneseName,
                trimmedKeyword,
                chineseMatched,
                englishMatched,
                normalizedEnglishMatched,
                japaneseMatched,
                language);
            item["searchKeyword"] = searchKeyword;
            results.append(item);
            addedSearchKeywords.insert(searchKeyword);
        }
    }
    
    return results;
}
