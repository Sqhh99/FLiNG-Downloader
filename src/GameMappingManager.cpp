#include "GameMappingManager.h"

#include <QDebug>
#include <QMetaObject>
#include <QMutexLocker>
#include <QRegularExpression>

#include "TranslationDatabase.h"

namespace {
QString normalizeLookupText(const QString& value)
{
    QString normalized = value.toLower().trimmed();
    normalized.remove(QRegularExpression(QStringLiteral("[\\s\\-_:：·'\"()\\[\\]{}.,!?/\\\\]+")));
    return normalized;
}
}

GameMappingManager& GameMappingManager::getInstance()
{
    static GameMappingManager instance;
    return instance;
}

GameMappingManager::GameMappingManager(QObject* parent)
    : QObject(parent), m_initialized(false)
{
}

bool GameMappingManager::initialize()
{
    QMutexLocker locker(&m_mutex);

    if (m_initialized) {
        return true;
    }

    qDebug() << "GameMappingManager: Initializing...";

    if (!loadBuiltinMappings()) {
        qWarning() << "GameMappingManager: Failed to load mappings from SQLite database";
        return false;
    }

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

    const QString trimmedInput = chinese.trimmed();
    if (trimmedInput.isEmpty()) {
        return QString();
    }
    const QString normalizedInput = normalizeLookupText(trimmedInput);

    auto exactMatch = m_builtinMappings.constFind(trimmedInput);
    if (exactMatch != m_builtinMappings.constEnd()) {
        return exactMatch.value().english;
    }

    for (auto it = m_builtinMappings.constBegin(); it != m_builtinMappings.constEnd(); ++it) {
        const QString normalizedKey = normalizeLookupText(it.key());
        if (trimmedInput.contains(it.key()) || it.key().contains(trimmedInput) ||
            normalizedInput == normalizedKey ||
            normalizedInput.contains(normalizedKey) || normalizedKey.contains(normalizedInput)) {
            return it.value().english;
        }
    }

    return QString();
}

void GameMappingManager::translateToEnglishAsync(const QString& chinese,
                                                 std::function<void(const QString&)> callback)
{
    const QString result = translateToEnglish(chinese);
    if (!callback) {
        return;
    }

    QMetaObject::invokeMethod(
        this,
        [callback, result]() {
            callback(result);
        },
        Qt::QueuedConnection);
}

QString GameMappingManager::fuzzyMatch(const QString& input) const
{
    QMutexLocker locker(&m_mutex);

    if (input.isEmpty()) {
        return QString();
    }

    QString bestMatch;
    double maxSimilarity = 0.0;
    const QString trimmedInput = input.trimmed();
    const double threshold = 0.6;

    for (auto it = m_builtinMappings.constBegin(); it != m_builtinMappings.constEnd(); ++it) {
        const double chineseSimilarity = calculateSimilarity(trimmedInput, it.key());
        if (chineseSimilarity > maxSimilarity && chineseSimilarity >= threshold) {
            maxSimilarity = chineseSimilarity;
            bestMatch = it.value().english;
        }

        const double englishSimilarity = calculateSimilarity(trimmedInput, it.value().english);
        if (englishSimilarity > maxSimilarity && englishSimilarity >= threshold) {
            maxSimilarity = englishSimilarity;
            bestMatch = it.value().english;
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
    return m_builtinMappings.keys();
}

QStringList GameMappingManager::getAllAliases() const
{
    return {};
}

bool GameMappingManager::loadBuiltinMappings()
{
    m_builtinMappings.clear();

    const QList<TranslationGameRecord> records = TranslationDatabase::getInstance().loadAllGames();
    if (records.isEmpty()) {
        return false;
    }

    for (const TranslationGameRecord& record : records) {
        if (record.chineseSimplified.isEmpty() || record.english.isEmpty()) {
            continue;
        }

        GameMappingInfo info;
        info.english = record.english;
        info.category = QStringLiteral("database");
        m_builtinMappings.insert(record.chineseSimplified, info);
    }

    qDebug() << "GameMappingManager: Loaded" << m_builtinMappings.size() << "mappings from SQLite";
    return !m_builtinMappings.isEmpty();
}

double GameMappingManager::calculateSimilarity(const QString& str1, const QString& str2) const
{
    if (str1 == str2) {
        return 1.0;
    }
    if (str1.isEmpty() || str2.isEmpty()) {
        return 0.0;
    }

    if (str1.contains(str2) || str2.contains(str1)) {
        return 0.8;
    }

    int matches = 0;
    const int maxLen = qMax(str1.length(), str2.length());
    const int minLen = qMin(str1.length(), str2.length());

    for (int i = 0; i < minLen; ++i) {
        if (str1[i] == str2[i]) {
            ++matches;
        }
    }

    return static_cast<double>(matches) / maxLen;
}

bool GameMappingManager::reloadMappings()
{
    QMutexLocker locker(&m_mutex);
    m_initialized = false;
    locker.unlock();

    return initialize();
}
