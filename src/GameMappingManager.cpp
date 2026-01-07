#include "GameMappingManager.h"
#include "translator.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDebug>
#include <QMutexLocker>
#include <QTimer>
#include <QtConcurrent>
#include <QFuture>

GameMappingManager& GameMappingManager::getInstance()
{
    static GameMappingManager instance;
    return instance;
}

GameMappingManager::GameMappingManager(QObject* parent)
    : QObject(parent), m_initialized(false), m_translationTimer(nullptr)
{
    m_translationTimer = new QTimer(this);
    m_translationTimer->setSingleShot(true);
    connect(m_translationTimer, &QTimer::timeout, this, &GameMappingManager::onTranslationTimerTimeout);
}

bool GameMappingManager::initialize()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_initialized) {
        return true;
    }
    
    qDebug() << "GameMappingManager: Initializing...";
    
    initializeTranslator();
    
    if (!loadBuiltinMappings()) {
        qWarning() << "GameMappingManager: Failed to load built-in mappings";
        return false;
    }
    
    loadUserMappings();
    loadTranslationCache();
    
    m_initialized = true;
    qDebug() << "GameMappingManager: Initialized with" << m_builtinMappings.size() << "mappings";
    
    return true;
}

QString GameMappingManager::translateToEnglish(const QString& chinese)
{
    QMutexLocker locker(&m_mutex);
    
    if (chinese.isEmpty()) {
        return QString();
    }
      // 1. Check translation cache
    if (m_translationCache.contains(chinese)) {
        QString cachedResult = m_translationCache[chinese];
        QString parsedResult = parseTranslationFromJson(cachedResult);
        return !parsedResult.isEmpty() ? parsedResult : cachedResult;
    }
    
    // 2. Check user-defined mapping
    if (m_userMappings.contains(chinese)) {
        return m_userMappings[chinese];
    }
    
    // 3. Check built-in mapping
    if (m_builtinMappings.contains(chinese)) {
        return m_builtinMappings[chinese].english;
    }
    
    // 4. Check alias mapping
    if (m_aliasToEnglish.contains(chinese)) {
        return m_aliasToEnglish[chinese];
    }
    
    // 5. Partial match check
    for (auto it = m_builtinMappings.begin(); it != m_builtinMappings.end(); ++it) {
        // Check if contains keyword
        if (chinese.contains(it.key()) || it.key().contains(chinese)) {
            return it.value().english;
        }
        
        // Check aliases
        for (const QString& alias : it.value().aliases) {
            if (chinese.contains(alias) || alias.contains(chinese)) {
                return it.value().english;
            }
        }
    }
    
    return QString(); // Mapping not found, need async translation
}

void GameMappingManager::translateToEnglishAsync(const QString& chinese, std::function<void(const QString&)> callback)
{
    // Try synchronous lookup first
    QString result = translateToEnglish(chinese);
    if (!result.isEmpty()) {
        callback(result);
        return;
    }
    
    // If not found, add to async translation queue
    QMutexLocker locker(&m_mutex);
    
    if (!m_pendingTranslations.contains(chinese)) {
        m_pendingTranslations.append(chinese);
        m_translationCallbacks[chinese] = callback;
        
        if (!m_translationTimer->isActive()) {
            m_translationTimer->start(1000);
        }
    } else {
        m_translationCallbacks[chinese] = callback;
    }
}

QString GameMappingManager::fuzzyMatch(const QString& input) const
{
    QMutexLocker locker(&m_mutex);
    
    if (input.isEmpty()) {
        return QString();
    }
    
    QString bestMatch;
    double maxSimilarity = 0.0;
    const double threshold = 0.6; // Similarity threshold
    
    // Check all built-in mappings
    for (auto it = m_builtinMappings.begin(); it != m_builtinMappings.end(); ++it) {
        double similarity = calculateSimilarity(input, it.key());
        if (similarity > maxSimilarity && similarity >= threshold) {
            maxSimilarity = similarity;
            bestMatch = it.value().english;
        }
        
        // Check aliases
        for (const QString& alias : it.value().aliases) {
            similarity = calculateSimilarity(input, alias);
            if (similarity > maxSimilarity && similarity >= threshold) {
                maxSimilarity = similarity;
                bestMatch = it.value().english;
            }
        }
    }
    
    return bestMatch;
}

bool GameMappingManager::containsChinese(const QString& text) const
{
    for (const QChar& ch : text) {
        if (ch.unicode() >= 0x4E00 && ch.unicode() <= 0x9FFF) {
            return true;
        }
    }
    return false;
}

QStringList GameMappingManager::getAllChineseNames() const
{
    QMutexLocker locker(&m_mutex);
    QStringList result = m_builtinMappings.keys();
    result.append(m_userMappings.keys());
    result.append(m_translationCache.keys());
    result.removeDuplicates();
    return result;
}

QStringList GameMappingManager::getAllAliases() const
{
    QMutexLocker locker(&m_mutex);
    
    QStringList allAliases;
    allAliases << m_builtinMappings.keys(); // Add main Chinese names
    
    for (const GameMappingInfo& info : m_builtinMappings) {
        allAliases << info.aliases;
    }
    
    allAliases.removeDuplicates();
    return allAliases;
}

void GameMappingManager::addUserMapping(const QString& chinese, const QString& english)
{
    QMutexLocker locker(&m_mutex);
    m_userMappings[chinese] = english;
    qDebug() << "GameMappingManager: Added user mapping:" << chinese << "->" << english;
    
    locker.unlock();
    saveUserMappings();
}

void GameMappingManager::addTranslationCache(const QString& chinese, const QString& english)
{
    QMutexLocker locker(&m_mutex);
    m_translationCache[chinese] = english;
}

bool GameMappingManager::loadBuiltinMappings()
{
    // Clear existing mappings
    m_builtinMappings.clear();
    m_aliasToEnglish.clear();
    
    // Try to load JSON mappings from resource file
    QString filePath = getBuiltinMappingPath();
    QFile file(filePath);
    
    if (!file.exists()) {
        qWarning() << "GameMappingManager: Built-in mapping file not found:" << filePath;
        return false;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "GameMappingManager: Cannot open built-in mapping file:" << filePath;
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "GameMappingManager: JSON parse error:" << error.errorString();
        return false;
    }
    
    QJsonObject root = doc.object();
    QJsonObject mappings = root["mappings"].toObject();
    
    // Parse each game mapping
    for (auto it = mappings.begin(); it != mappings.end(); ++it) {
        QString chinese = it.key();
        QJsonObject gameObj = it.value().toObject();
        
        GameMappingInfo info;
        info.english = gameObj["english"].toString();
        info.category = gameObj["category"].toString();
        
        // Parse aliases
        QJsonArray aliasArray = gameObj["aliases"].toArray();
        for (const auto& alias : aliasArray) {
            info.aliases.append(alias.toString());
        }
        
        m_builtinMappings[chinese] = info;
        
        // Build alias mapping
        for (const QString& alias : info.aliases) {
            m_aliasToEnglish[alias] = info.english;
        }
    }
    
    qDebug() << "GameMappingManager: Loaded" << m_builtinMappings.size() << "built-in mappings from JSON";
    return true;
}

// loadDefaultMappings removed - JSON is embedded in resources and cannot be corrupted

bool GameMappingManager::loadUserMappings()
{
    QString filePath = getUserMappingPath();
    QFile file(filePath);
    
    if (!file.exists()) {
        return true; // File not existing is normal
    }
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "GameMappingManager: Cannot open user mapping file:" << filePath;
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "GameMappingManager: User mapping JSON parse error:" << error.errorString();
        return false;
    }
    
    QJsonObject mappings = doc.object();
    m_userMappings.clear();
    
    for (auto it = mappings.begin(); it != mappings.end(); ++it) {
        m_userMappings[it.key()] = it.value().toString();
    }
    
    if (!m_userMappings.isEmpty()) {
        qDebug() << "GameMappingManager: Loaded" << m_userMappings.size() << "user mappings";
    }
    return true;
}

bool GameMappingManager::loadTranslationCache()
{
    QString filePath = getTranslationCachePath();
    QFile file(filePath);
    
    if (!file.exists()) {
        return true; // File not existing is normal
    }
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "GameMappingManager: Cannot open translation cache:" << filePath;
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "GameMappingManager: Translation cache JSON parse error:" << error.errorString();
        return false;
    }
    
    QJsonObject obj = doc.object();
    m_translationCache.clear();
    
    bool needsCleanup = false;
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        QString value = it.value().toString();
        
        // Check if value is raw JSON response that needs parsing
        if (value.contains("{\"code\":") && value.contains("\"data\":")) {
            QString parsedResult = parseTranslationFromJson(value);
            if (!parsedResult.isEmpty() && parsedResult != value) {
                m_translationCache[it.key()] = parsedResult;
                needsCleanup = true;
            } else {
                needsCleanup = true; // Remove invalid entry
            }
        } else {
            m_translationCache[it.key()] = value;
        }
    }
    
    if (needsCleanup) {
        saveTranslationCache();
    }
    
    if (!m_translationCache.isEmpty()) {
        qDebug() << "GameMappingManager: Loaded" << m_translationCache.size() << "cached translations";
    }
    return true;
}

void GameMappingManager::saveUserMappings()
{
    QMutexLocker locker(&m_mutex);
    
    QString filePath = getUserMappingPath();
    QDir dir = QFileInfo(filePath).dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    QJsonObject mappings;
    for (auto it = m_userMappings.begin(); it != m_userMappings.end(); ++it) {
        mappings[it.key()] = it.value();
    }
    
    QJsonDocument doc(mappings);
    
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(doc.toJson());
        file.close();
    } else {
        qWarning() << "GameMappingManager: Cannot save user mappings to:" << filePath;
    }
}

void GameMappingManager::saveTranslationCache()
{
    QMutexLocker locker(&m_mutex);
    
    QString filePath = getTranslationCachePath();
    QDir dir = QFileInfo(filePath).dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    QJsonObject cache;
    for (auto it = m_translationCache.begin(); it != m_translationCache.end(); ++it) {
        cache[it.key()] = it.value();
    }
    
    QJsonDocument doc(cache);
    
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(doc.toJson());
        file.close();
    } else {
        qWarning() << "GameMappingManager: Cannot save translation cache to:" << filePath;
    }
}

double GameMappingManager::calculateSimilarity(const QString& str1, const QString& str2) const
{
    // Simple similarity calculation
    if (str1 == str2) return 1.0;
    if (str1.isEmpty() || str2.isEmpty()) return 0.0;
    
    // Calculate containment relationship
    if (str1.contains(str2) || str2.contains(str1)) {
        return 0.8;
    }
    
    // Calculate character match ratio
    int matches = 0;
    int maxLen = qMax(str1.length(), str2.length());
    int minLen = qMin(str1.length(), str2.length());
    
    for (int i = 0; i < minLen; ++i) {
        if (str1[i] == str2[i]) {
            matches++;
        }
    }
    
    return static_cast<double>(matches) / maxLen;
}

QString GameMappingManager::getUserMappingPath() const
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(appDataPath).filePath("user_game_mappings.json");
}

QString GameMappingManager::getTranslationCachePath() const
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(appDataPath).filePath("translation_cache.json");
}

QString GameMappingManager::getBuiltinMappingPath() const
{
    return ":/game_mappings.json";
}

void GameMappingManager::initializeTranslator()
{
    m_translator = TranslatorFactory::createTranslator(TranslatorFactory::TranslatorType::SU_API);
    
    if (!m_translator) {
        qWarning() << "GameMappingManager: Translator initialization failed";
    }
}

void GameMappingManager::onTranslationTimerTimeout()
{
    if (m_pendingTranslations.isEmpty()) {
        return;
    }
    
    QString chinese = m_pendingTranslations.takeFirst();
    performTranslation(chinese);
}

void GameMappingManager::performTranslation(const QString& chinese)
{
    if (!m_translator) {
        qWarning() << "GameMappingManager: Translator not initialized";
        emit translationFailed(chinese, "Translator not initialized");
        return;
    }
    
    auto future = QtConcurrent::run([this, chinese]() {
        std::string chineseStd = chinese.toStdString();
        TranslationResult result = m_translator->translate(chineseStd, "zh-CN", "en");
        
        if (result.success && !result.translated.empty()) {
            QString english = QString::fromStdString(result.translated);
            
            if (!english.isEmpty()) {
                english[0] = english[0].toUpper();
            }
            
            addTranslationCache(chinese, english);
            saveTranslationCache();
            
            QMutexLocker locker(&m_mutex);
            if (m_translationCallbacks.contains(chinese)) {
                auto callback = m_translationCallbacks[chinese];
                m_translationCallbacks.remove(chinese);
                locker.unlock();
                
                QMetaObject::invokeMethod(this, [callback, english]() {
                    callback(english);
                }, Qt::QueuedConnection);
            }
            
            emit translationCompleted(chinese, english);
        } else {
            QString error = QString::fromStdString(result.error);
            qWarning() << "GameMappingManager: Translation failed for" << chinese << ":" << error;
            
            QMutexLocker locker(&m_mutex);
            m_translationCallbacks.remove(chinese);
            
            emit translationFailed(chinese, error);
        }
    });
}

bool GameMappingManager::reloadMappings()
{
    QMutexLocker locker(&m_mutex);
    m_initialized = false;
    locker.unlock();
    
    return initialize();
}

QString GameMappingManager::parseTranslationFromJson(const QString& jsonResponse) const
{
    if (jsonResponse.isEmpty()) {
        return QString();
    }
    
    if (!jsonResponse.startsWith("{") && !jsonResponse.startsWith("[")) {
        return jsonResponse;
    }
    
    try {
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(jsonResponse.toUtf8(), &error);
        
        if (error.error != QJsonParseError::NoError) {
            return QString();
        }
        
        QJsonObject obj = doc.object();
        
        // Handle SuAPI format
        if (obj.contains("data") && obj["data"].isArray()) {
            QJsonArray dataArray = obj["data"].toArray();
            if (!dataArray.isEmpty()) {
                QJsonObject firstData = dataArray[0].toObject();
                if (firstData.contains("translations") && firstData["translations"].isArray()) {
                    QJsonArray translations = firstData["translations"].toArray();
                    if (!translations.isEmpty()) {
                        QJsonObject translation = translations[0].toObject();
                        if (translation.contains("text")) {
                            return translation["text"].toString().trimmed();
                        }
                    }
                }
            }
        }
        
        // Handle AppWorlds format
        if (obj.contains("data") && obj["data"].isString()) {
            return obj["data"].toString().trimmed();
        }
        
        // Handle other formats
        if (obj.contains("result")) {
            return obj["result"].toString().trimmed();
        }
        
        if (obj.contains("translation")) {
            return obj["translation"].toString().trimmed();
        }
        
    } catch (const std::exception& e) {
        qWarning() << "GameMappingManager: JSON parse exception:" << e.what();
    }
    
    return QString();
}
