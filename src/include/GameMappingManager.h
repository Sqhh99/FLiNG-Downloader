#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QStringList>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <memory>

// Forward declaration
struct TranslationResult;
class ITranslator;

struct GameMappingInfo {
    QString english;
    QStringList aliases;
    QString category;
    bool isTranslated = false;  // Flag indicating if obtained through translation
};

class GameMappingManager : public QObject
{
    Q_OBJECT

public:
    static GameMappingManager& getInstance();
    
    // Initialize mapping data
    bool initialize();
    
    // Chinese to English (supports online translation)
    QString translateToEnglish(const QString& chinese);
    
    // Async translation (using your translation API)
    void translateToEnglishAsync(const QString& chinese, std::function<void(const QString&)> callback);
    
    // Fuzzy matching
    QString fuzzyMatch(const QString& input) const;
    
    // Detect if text contains Chinese characters
    bool containsChinese(const QString& text) const;
    
    // Get all Chinese game names (for auto-completion)
    QStringList getAllChineseNames() const;
    
    // Get all aliases
    QStringList getAllAliases() const;
    
    // Add user-defined mapping
    void addUserMapping(const QString& chinese, const QString& english);
    
    // Add translation cache
    void addTranslationCache(const QString& chinese, const QString& english);
    
    // Save user mappings and translation cache
    void saveUserMappings();
    void saveTranslationCache();
    
    // Reload configuration
    bool reloadMappings();

signals:
    void translationCompleted(const QString& chinese, const QString& english);
    void translationFailed(const QString& chinese, const QString& error);

private slots:
    void onTranslationTimerTimeout();

private:
    GameMappingManager(QObject* parent = nullptr);
    ~GameMappingManager() = default;
    
    // Disable copy
    GameMappingManager(const GameMappingManager&) = delete;
    GameMappingManager& operator=(const GameMappingManager&) = delete;    
    // Load built-in mappings from JSON resource
    bool loadBuiltinMappings();
    
    // Load user-defined mappings
    bool loadUserMappings();
    
    // Load translation cache
    bool loadTranslationCache();
    
    // Calculate string similarity
    double calculateSimilarity(const QString& str1, const QString& str2) const;
    
    // Get configuration file paths
    QString getBuiltinMappingPath() const;
    QString getUserMappingPath() const;
    QString getTranslationCachePath() const;
    
    // Initialize translator
    void initializeTranslator();
      // Perform actual translation
    void performTranslation(const QString& chinese);
    
    // Parse translation result from JSON response
    QString parseTranslationFromJson(const QString& jsonResponse) const;

private:
    QMap<QString, GameMappingInfo> m_builtinMappings;
    QMap<QString, QString> m_userMappings;
    QMap<QString, QString> m_translationCache;  // Translation cache
    QMap<QString, QString> m_aliasToEnglish; // Alias to English mapping
    mutable QMutex m_mutex;
    bool m_initialized;
    
    // Translation related
    std::unique_ptr<ITranslator> m_translator;
    QTimer* m_translationTimer;
    QStringList m_pendingTranslations;
    QMap<QString, std::function<void(const QString&)>> m_translationCallbacks;
};
