#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>
#include "ModifierListModel.h"
#include "DownloadedModifierModel.h"
#include "ModifierParser.h"
#include "SearchManager.h"
#include "ModifierManager.h"
#include "ConfigManager.h"
#include "ThemeManager.h"
#include "LanguageManager.h"
#include "CoverExtractor.h"

class QGuiApplication;

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
    Q_INVOKABLE void openDownloadFolder();

    // Downloaded management
    Q_INVOKABLE void runModifier(int index);
    Q_INVOKABLE void deleteModifier(int index);
    Q_INVOKABLE void checkForUpdates();

    // Settings
    Q_INVOKABLE void setTheme(int themeIndex);
    Q_INVOKABLE void setLanguage(int languageIndex);
    
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
    void downloadFolderSelectionRequested();  // Request to show folder selection dialog

private slots:
    void onSearchCompleted(const QList<ModifierInfo>& modifiers);
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished(bool success);

private:
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
    
    // Temporary storage for downloaded modifiers list
    QList<DownloadedModifierInfo> m_downloadedList;
    
    // Cover extractor
    CoverExtractor* m_coverExtractor;
    QString m_currentCoverPath;
    
    // Game name mapping data (Chinese-English search suggestions)
    struct GameMapping {
        QString chineseName;   // Chinese name
        QString englishName;   // English name
        QStringList aliases;   // Alias list
    };
    QList<GameMapping> m_gameMappings;
    void loadGameMappings();
};
