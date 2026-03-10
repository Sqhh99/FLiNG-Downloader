#pragma once

#include <QObject>
#include <QHash>
#include <QQmlEngine>
#include <QVariantList>
#include <QVariantMap>
#include <QElapsedTimer>
#include <functional>
#include "ModifierListModel.h"
#include "DownloadedModifierModel.h"
#include "ModifierParser.h"
#include "SearchManager.h"
#include "ModifierManager.h"
#include "ConfigManager.h"
#include "ThemeManager.h"
#include "LanguageManager.h"
#include "CoverExtractor.h"
#include "AppUpdateManager.h"

class QGuiApplication;
class QTimer;

/**
 * Backend - QML backend bridge class
 * Exposes C++ business logic to QML
 */
class Backend : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Data models
    Q_PROPERTY(ModifierListModel* modifierListModel READ modifierListModel CONSTANT)
    Q_PROPERTY(DownloadedModifierModel* downloadedModifierModel READ downloadedModifierModel CONSTANT)

    // Current language
    Q_PROPERTY(int currentLanguage READ currentLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)

    // Selected modifier info
    Q_PROPERTY(QString selectedModifierName READ selectedModifierName NOTIFY selectedModifierChanged)
    Q_PROPERTY(QString selectedModifierVersion READ selectedModifierVersion NOTIFY selectedModifierChanged)
    Q_PROPERTY(int selectedModifierOptionsCount READ selectedModifierOptionsCount NOTIFY selectedModifierChanged)
    Q_PROPERTY(QString selectedModifierLastUpdate READ selectedModifierLastUpdate NOTIFY selectedModifierChanged)
    Q_PROPERTY(QString selectedModifierOptions READ selectedModifierOptions NOTIFY selectedModifierOptionsChanged)
    Q_PROPERTY(QVariantList selectedModifierVersions READ selectedModifierVersions NOTIFY selectedModifierChanged)
    Q_PROPERTY(QString selectedModifierCoverUrl READ selectedModifierCoverUrl NOTIFY selectedModifierChanged)

    // Download status
    Q_PROPERTY(bool isDownloading READ isDownloading NOTIFY downloadingChanged)
    Q_PROPERTY(qreal downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(bool searchLoading READ searchLoading NOTIFY searchLoadingChanged)
    Q_PROPERTY(QVariantList downloadTasks READ downloadTasks NOTIFY downloadTasksChanged)
    Q_PROPERTY(bool autoCheckAppUpdates READ autoCheckAppUpdates WRITE setAutoCheckAppUpdates NOTIFY autoCheckAppUpdatesChanged)
    Q_PROPERTY(bool appUpdateChecking READ appUpdateChecking NOTIFY appUpdateStateChanged)
    Q_PROPERTY(bool appUpdateAvailable READ appUpdateAvailable NOTIFY appUpdateStateChanged)
    Q_PROPERTY(QString appLatestVersion READ appLatestVersion NOTIFY appUpdateStateChanged)
    Q_PROPERTY(QString appUpdateSource READ appUpdateSource NOTIFY appUpdateStateChanged)
    Q_PROPERTY(QString appUpdatePublishedAt READ appUpdatePublishedAt NOTIFY appUpdateStateChanged)
    Q_PROPERTY(bool appUpdateDownloading READ appUpdateDownloading NOTIFY appUpdateStateChanged)
    Q_PROPERTY(qreal appUpdateProgress READ appUpdateProgress NOTIFY appUpdateStateChanged)
    Q_PROPERTY(QString appUpdateStatusText READ appUpdateStatusText NOTIFY appUpdateStateChanged)
    
    // Download directory
    Q_PROPERTY(QString downloadPath READ downloadPath WRITE setDownloadPath NOTIFY downloadPathChanged)

public:
    explicit Backend(QObject* parent = nullptr);
    ~Backend();

    // Set QGuiApplication reference (for language switching)
    void setApplication(QGuiApplication* app) { m_app = app; }
    
    // Set QQmlEngine reference (for refreshing QML during language switch)
    void setQmlEngine(QQmlEngine* engine) { m_qmlEngine = engine; }

    // Property access
    ModifierListModel* modifierListModel() const { return m_modifierListModel; }
    DownloadedModifierModel* downloadedModifierModel() const { return m_downloadedModifierModel; }
    int currentLanguage() const;
    QString appVersion() const;

    QString selectedModifierName() const;
    QString selectedModifierVersion() const;
    int selectedModifierOptionsCount() const;
    QString selectedModifierLastUpdate() const;
    QString selectedModifierOptions() const;
    QVariantList selectedModifierVersions() const;
    QString selectedModifierCoverUrl() const;

    // Cover extraction feature
    Q_INVOKABLE void extractCover();
    Q_PROPERTY(QString selectedModifierCoverPath READ selectedModifierCoverPath NOTIFY coverExtracted)
    QString selectedModifierCoverPath() const { return m_currentCoverPath; }

    bool isDownloading() const { return m_isDownloading; }
    qreal downloadProgress() const { return m_downloadProgress; }
    bool searchLoading() const { return m_searchLoading; }
    QVariantList downloadTasks() const;
    bool autoCheckAppUpdates() const;
    bool appUpdateChecking() const { return m_appUpdateChecking; }
    bool appUpdateAvailable() const { return m_appUpdateAvailable; }
    QString appLatestVersion() const { return m_appLatestVersion; }
    QString appUpdateSource() const { return m_appUpdateSource; }
    QString appUpdatePublishedAt() const { return m_appUpdatePublishedAt; }
    bool appUpdateDownloading() const { return m_appUpdateDownloading; }
    qreal appUpdateProgress() const { return m_appUpdateProgress; }
    QString appUpdateStatusText() const { return m_appUpdateStatusText; }
    
    // Download directory
    QString downloadPath() const;
    Q_INVOKABLE void setDownloadPath(const QString& path);
    Q_INVOKABLE void requestDownloadFolderSelection();  // Request to open folder selection dialog

public slots:
    // Search functionality
    Q_INVOKABLE void searchModifiers(const QString& keyword);
    Q_INVOKABLE void fetchRecentModifiers();
    Q_INVOKABLE void setSortOrder(int sortIndex);

    // Modifier selection
    Q_INVOKABLE void selectModifier(int index);
    Q_INVOKABLE void selectVersion(int versionIndex);

    // Download functionality
    Q_INVOKABLE void downloadModifier(int versionIndex);
    Q_INVOKABLE void pauseDownload(const QString& taskId);
    Q_INVOKABLE void resumeDownload(const QString& taskId);
    Q_INVOKABLE void cancelDownload(const QString& taskId);
    Q_INVOKABLE void removeDownloadTask(const QString& taskId);
    Q_INVOKABLE void openDownloadFolder();

    // Downloaded management
    Q_INVOKABLE void runModifier(int index);
    Q_INVOKABLE void deleteModifier(int index);
    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void checkAppUpdate();
    Q_INVOKABLE void downloadAppUpdate();

    // Settings
    Q_INVOKABLE void setTheme(int themeIndex);
    Q_INVOKABLE void setLanguage(int languageIndex);
    Q_INVOKABLE void setAutoCheckAppUpdates(bool enabled);
    
    // Search suggestions - obtained from game_mappings.json
    Q_INVOKABLE QStringList getSuggestions(const QString& keyword, int maxResults = 8);


signals:
    void languageChanged();
    void selectedModifierChanged();
    void selectedModifierOptionsChanged();
    void downloadingChanged();
    void downloadProgressChanged();
    void searchCompleted();
    void downloadCompleted(bool success);
    void statusMessage(const QString& message);
    void coverExtracted();
    void downloadPathChanged();
    void searchLoadingChanged();
    void downloadTasksChanged();
    void downloadFolderSelectionRequested();  // Request to show folder selection dialog
    void autoCheckAppUpdatesChanged();
    void appUpdateStateChanged();

private slots:
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished(bool success);

private:
    struct DownloadTaskMeta {
        QString taskId;
        ModifierInfo modifier;
        QString versionName;
        QString savePath;
        QString tempPath;  // .crdownload temporary file path
    };

    QString createDownloadTask(const ModifierInfo& modifier,
                               const QString& versionName,
                               const QString& savePath);
    void processNextDownloadTask();
    void startDownloadTask(const QString& taskId);
    int findDownloadTaskIndex(const QString& taskId) const;
    void updateDownloadTask(const QString& taskId, const std::function<void(QVariantMap&)>& updater);
    void updateDownloadTaskDeferred(const QString& taskId, const std::function<void(QVariantMap&)>& updater);
    quint64 beginSearchRequest();
    void finishSearchRequest(quint64 requestId, const QList<ModifierInfo>& modifiers);
    void loadDownloadedModifiers();
    void saveDownloadedModifiers();

private:
    QGuiApplication* m_app = nullptr;
    QQmlEngine* m_qmlEngine = nullptr;
    ModifierListModel* m_modifierListModel;
    DownloadedModifierModel* m_downloadedModifierModel;
    
    // Currently selected modifier
    int m_selectedIndex = -1;
    ModifierInfo m_selectedModifier;
    QString m_selectedOptions;
    int m_selectedVersionIndex = 0;
    
    // Download status
    bool m_isDownloading = false;
    qreal m_downloadProgress = 0.0;
    bool m_searchLoading = false;
    quint64 m_nextSearchRequestId = 0;
    quint64 m_activeSearchRequestId = 0;
    quint64 m_nextDownloadTaskId = 0;
    QString m_activeDownloadTaskId;
    QList<QVariantMap> m_downloadTasks;
    QHash<QString, DownloadTaskMeta> m_downloadTaskMeta;
    
    // Speed calculation
    QElapsedTimer m_speedTimer;
    qint64 m_lastSpeedBytes = 0;
    QTimer* m_speedUpdateTimer = nullptr;
    
    // Throttled download task update (prevents QML delegate rebuild flooding)
    bool m_downloadTasksDirty = false;
    QTimer* m_taskUpdateTimer = nullptr;
    
    // Temporary storage for downloaded modifiers list
    QList<DownloadedModifierInfo> m_downloadedList;
    
    // Cover extractor
    CoverExtractor* m_coverExtractor;
    QString m_currentCoverPath;

    AppUpdateManager* m_appUpdateManager = nullptr;
    AppReleaseInfo m_latestAppRelease;
    bool m_appUpdateChecking = false;
    bool m_appUpdateAvailable = false;
    QString m_appLatestVersion;
    QString m_appUpdateSource;
    QString m_appUpdatePublishedAt;
    bool m_appUpdateDownloading = false;
    qreal m_appUpdateProgress = 0.0;
    QString m_appUpdateStatusText;
    
    // Game name mapping data (Chinese-English search suggestions)
    struct GameMapping {
        QString chineseName;   // Chinese name
        QString englishName;   // English name
        QStringList aliases;   // Alias list
    };
    QList<GameMapping> m_gameMappings;
    void loadGameMappings();
};
