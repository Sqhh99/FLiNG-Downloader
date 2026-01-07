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

// 前向声明
struct TranslationResult;
class ITranslator;

struct GameMappingInfo {
    QString english;
    QStringList aliases;
    QString category;
    bool isTranslated = false;  // 标记是否是翻译得到的
};

class GameMappingManager : public QObject
{
    Q_OBJECT

public:
    static GameMappingManager& getInstance();
    
    // 初始化映射数据
    bool initialize();
    
    // 中文转英文（支持在线翻译）
    QString translateToEnglish(const QString& chinese);
    
    // 异步翻译（使用您的翻译API）
    void translateToEnglishAsync(const QString& chinese, std::function<void(const QString&)> callback);
    
    // 模糊匹配
    QString fuzzyMatch(const QString& input) const;
    
    // 检测是否包含中文字符
    bool containsChinese(const QString& text) const;
    
    // 获取所有中文游戏名（用于自动完成）
    QStringList getAllChineseNames() const;
    
    // 获取所有别名
    QStringList getAllAliases() const;
    
    // 添加用户自定义映射
    void addUserMapping(const QString& chinese, const QString& english);
    
    // 添加翻译缓存
    void addTranslationCache(const QString& chinese, const QString& english);
    
    // 保存用户映射和翻译缓存
    void saveUserMappings();
    void saveTranslationCache();
    
    // 重新加载配置
    bool reloadMappings();

signals:
    void translationCompleted(const QString& chinese, const QString& english);
    void translationFailed(const QString& chinese, const QString& error);

private slots:
    void onTranslationTimerTimeout();

private:
    GameMappingManager(QObject* parent = nullptr);
    ~GameMappingManager() = default;
    
    // 禁止拷贝
    GameMappingManager(const GameMappingManager&) = delete;
    GameMappingManager& operator=(const GameMappingManager&) = delete;    
    // 加载内置映射
    bool loadBuiltinMappings();
    
    // 加载默认映射（回退方案）
    bool loadDefaultMappings();
    
    // 加载用户自定义映射
    bool loadUserMappings();
    
    // 加载翻译缓存
    bool loadTranslationCache();
    
    // 计算字符串相似度
    double calculateSimilarity(const QString& str1, const QString& str2) const;
    
    // 获取配置文件路径
    QString getBuiltinMappingPath() const;
    QString getUserMappingPath() const;
    QString getTranslationCachePath() const;
    
    // 初始化翻译器
    void initializeTranslator();
      // 执行实际翻译
    void performTranslation(const QString& chinese);
    
    // 从JSON响应中解析翻译结果
    QString parseTranslationFromJson(const QString& jsonResponse) const;

private:
    QMap<QString, GameMappingInfo> m_builtinMappings;
    QMap<QString, QString> m_userMappings;
    QMap<QString, QString> m_translationCache;  // 翻译缓存
    QMap<QString, QString> m_aliasToEnglish; // 别名到英文的映射
    mutable QMutex m_mutex;
    bool m_initialized;
    
    // 翻译相关
    std::unique_ptr<ITranslator> m_translator;
    QTimer* m_translationTimer;
    QStringList m_pendingTranslations;
    QMap<QString, std::function<void(const QString&)>> m_translationCallbacks;
};
