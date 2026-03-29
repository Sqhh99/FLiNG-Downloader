#include "GameMappingManager.h"

#include <QDebug>
#include <QMetaObject>
#include <QMutexLocker>

#include "TranslationDatabase.h"
#include "TranslationTextUtils.h"

namespace {
QString exactLookupKey(const QString& value)
{
    return value.trimmed().toCaseFolded();
}

bool containsLookupValue(const QString& input, const QString& normalizedInput, const QString& candidate, const QString& normalizedCandidate)
{
    if (!candidate.isEmpty() &&
        (input.contains(candidate, Qt::CaseInsensitive) || candidate.contains(input, Qt::CaseInsensitive))) {
        return true;
    }

    if (normalizedInput.isEmpty() || normalizedCandidate.isEmpty()) {
        return false;
    }

    return normalizedInput.contains(normalizedCandidate) ||
           normalizedCandidate.contains(normalizedInput);
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

QString GameMappingManager::translateToEnglish(const QString& input)
{
    QMutexLocker locker(&m_mutex);
    return translateToEnglishInternal(input, true);
}

QString GameMappingManager::translateToEnglishForSearch(const QString& input)
{
    QMutexLocker locker(&m_mutex);
    return translateToEnglishInternal(input, false);
}

QString GameMappingManager::translateToEnglishInternal(const QString& input, bool allowContainsFallback)
{
    if (input.isEmpty()) {
        return QString();
    }

    const QString trimmedInput = input.trimmed();
    if (trimmedInput.isEmpty()) {
        return QString();
    }

    const QString exactKey = exactLookupKey(trimmedInput);
    auto exactMatch = m_exactLookupToEnglish.constFind(exactKey);
    if (exactMatch != m_exactLookupToEnglish.constEnd()) {
        return exactMatch.value();
    }

    if (!TranslationTextUtils::hasNormalizedLookupText(trimmedInput)) {
        return QString();
    }

    const QString normalizedInput = TranslationTextUtils::normalizeLookupText(trimmedInput);
    auto normalizedMatch = m_normalizedLookupToEnglish.constFind(normalizedInput);
    if (normalizedMatch != m_normalizedLookupToEnglish.constEnd()) {
        return normalizedMatch.value();
    }

    if (!allowContainsFallback) {
        return QString();
    }

    for (auto it = m_builtinMappings.constBegin(); it != m_builtinMappings.constEnd(); ++it) {
        const GameMappingInfo& info = it.value();
        const bool matched =
            containsLookupValue(trimmedInput, normalizedInput, info.chinese, info.normalizedChinese) ||
            containsLookupValue(trimmedInput, normalizedInput, info.english, info.normalizedEnglish) ||
            containsLookupValue(trimmedInput, normalizedInput, info.japanese, info.normalizedJapanese);

        if (matched) {
            return it.value().english;
        }
    }

    return QString();
}

void GameMappingManager::translateToEnglishAsync(const QString& input,
                                                 std::function<void(const QString&)> callback)
{
    const QString result = translateToEnglish(input);
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

void GameMappingManager::addLookupValue(QHash<QString, QString>& lookup, const QString& key, const QString& english)
{
    if (key.isEmpty() || english.isEmpty() || lookup.contains(key)) {
        return;
    }

    lookup.insert(key, english);
}

bool GameMappingManager::loadBuiltinMappings()
{
    m_builtinMappings.clear();
    m_exactLookupToEnglish.clear();
    m_normalizedLookupToEnglish.clear();

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
        info.chinese = record.chineseSimplified;
        info.japanese = record.japanese;
        info.normalizedChinese = TranslationTextUtils::normalizeLookupText(record.chineseSimplified);
        info.normalizedJapanese = TranslationTextUtils::normalizeLookupText(record.japanese);
        info.normalizedEnglish = record.normalizedEnglish.isEmpty()
            ? TranslationTextUtils::normalizeLookupText(record.english)
            : TranslationTextUtils::normalizeLookupText(record.normalizedEnglish);
        info.category = QStringLiteral("database");
        m_builtinMappings.insert(record.chineseSimplified, info);

        addLookupValue(m_exactLookupToEnglish, exactLookupKey(info.chinese), info.english);
        addLookupValue(m_exactLookupToEnglish, exactLookupKey(info.english), info.english);
        addLookupValue(m_exactLookupToEnglish, exactLookupKey(info.japanese), info.english);
        addLookupValue(m_exactLookupToEnglish, exactLookupKey(info.normalizedEnglish), info.english);

        addLookupValue(m_normalizedLookupToEnglish, info.normalizedChinese, info.english);
        addLookupValue(m_normalizedLookupToEnglish, info.normalizedEnglish, info.english);
        addLookupValue(m_normalizedLookupToEnglish, info.normalizedJapanese, info.english);
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
