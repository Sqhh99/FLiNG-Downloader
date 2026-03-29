#include "TranslationDatabase.h"

#include <QDebug>

#include <SQLiteCpp/SQLiteCpp.h>

#include "FileSystem.h"

namespace {
std::string toUtf8String(const QString& value)
{
    const QByteArray utf8 = value.toUtf8();
    return std::string(utf8.constData(), static_cast<size_t>(utf8.size()));
}

QString fromUtf8String(const std::string& value)
{
    return QString::fromUtf8(value.c_str(), static_cast<int>(value.size()));
}
}

TranslationDatabase& TranslationDatabase::getInstance()
{
    static TranslationDatabase instance;
    return instance;
}

QString TranslationDatabase::databasePath() const
{
    return resolveDatabasePath();
}

bool TranslationDatabase::isAvailable() const
{
    return !databasePath().isEmpty();
}

QList<TranslationGameRecord> TranslationDatabase::loadAllGames() const
{
    QList<TranslationGameRecord> results;
    const QString path = databasePath();
    if (path.isEmpty()) {
        qWarning() << "TranslationDatabase: bundled database not found near application directory";
        return results;
    }

    try {
        SQLite::Database db(toUtf8String(path), SQLite::OPEN_READONLY);
        SQLite::Statement query(
            db,
            "SELECT english, normalized_english, chinese_simplified, japanese "
            "FROM games ORDER BY english COLLATE NOCASE ASC");

        while (query.executeStep()) {
            TranslationGameRecord record;
            record.english = fromUtf8String(query.getColumn(0).getString());
            record.normalizedEnglish = fromUtf8String(query.getColumn(1).getString());
            record.chineseSimplified = fromUtf8String(query.getColumn(2).getString());
            record.japanese = fromUtf8String(query.getColumn(3).getString());
            results.append(record);
        }
    } catch (const SQLite::Exception& ex) {
        qWarning() << "TranslationDatabase: failed to load games from" << path << ":" << ex.what();
        return {};
    }

    return results;
}

QString TranslationDatabase::resolveDatabasePath() const
{
    const QString path =
        FileSystem::getInstance().getBundledResourcePath("resources/fling_translations.db");
    if (!path.isEmpty()) {
        return path;
    }

    return QString();
}
