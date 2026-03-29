#include "TranslationDatabase.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

#include <SQLiteCpp/SQLiteCpp.h>

#include "AppUpdateManager.h"
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

constexpr auto kSupportedSchemaVersion = "1";

bool hasTable(SQLite::Database& db, const char* tableName)
{
    SQLite::Statement query(
        db,
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name = ? LIMIT 1");
    query.bind(1, tableName);
    return query.executeStep();
}

bool validateGamesColumns(SQLite::Database& db, QString* errorMessage)
{
    SQLite::Statement query(db, "PRAGMA table_info(games)");
    QSet<QString> columns;

    while (query.executeStep()) {
        columns.insert(fromUtf8String(query.getColumn(1).getString()));
    }

    const QStringList requiredColumns = {
        QStringLiteral("english"),
        QStringLiteral("normalized_english"),
        QStringLiteral("chinese_simplified"),
        QStringLiteral("japanese")
    };

    for (const QString& requiredColumn : requiredColumns) {
        if (!columns.contains(requiredColumn)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Required column missing: games.%1").arg(requiredColumn);
            }
            return false;
        }
    }

    return true;
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

QString TranslationDatabase::bundledDatabasePath() const
{
    return FileSystem::getInstance().getBundledResourcePath("resources/fling_translations.db");
}

QString TranslationDatabase::overrideDatabasePath() const
{
    return QDir(FileSystem::getInstance().getDataDirectory()).filePath("fling_translations.db");
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
        qWarning() << "TranslationDatabase: no valid translation database found"
                   << "Bundled candidate:" << bundledDatabasePath()
                   << "Override candidate:" << overrideDatabasePath();
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

TranslationDatabaseMetadata TranslationDatabase::readMetadata() const
{
    return readMetadata(databasePath());
}

TranslationDatabaseMetadata TranslationDatabase::readMetadata(const QString& path) const
{
    TranslationDatabaseMetadata metadata;
    if (path.isEmpty()) {
        return metadata;
    }

    try {
        SQLite::Database db(toUtf8String(path), SQLite::OPEN_READONLY);
        SQLite::Statement query(
            db,
            "SELECT key, value FROM metadata WHERE key IN ('release_tag', 'schema_version')");

        while (query.executeStep()) {
            const QString key = fromUtf8String(query.getColumn(0).getString());
            const QString value = fromUtf8String(query.getColumn(1).getString());
            if (key == QStringLiteral("release_tag")) {
                metadata.releaseTag = value;
            } else if (key == QStringLiteral("schema_version")) {
                metadata.schemaVersion = value;
            }
        }
    } catch (const SQLite::Exception& ex) {
        qWarning() << "TranslationDatabase: failed to read metadata from" << path << ":" << ex.what();
        return {};
    }

    return metadata;
}

QString TranslationDatabase::currentReleaseTag() const
{
    return readMetadata().releaseTag;
}

bool TranslationDatabase::isValidDatabaseFile(const QString& path, QString* errorMessage) const
{
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Database file does not exist");
        }
        return false;
    }

    try {
        SQLite::Database db(toUtf8String(path), SQLite::OPEN_READONLY);

        if (!hasTable(db, "games") || !hasTable(db, "metadata")) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Database schema is incomplete");
            }
            return false;
        }

        if (!validateGamesColumns(db, errorMessage)) {
            return false;
        }

        const TranslationDatabaseMetadata metadata = readMetadata(path);
        if (!metadata.isValid()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Database metadata is missing release_tag");
            }
            return false;
        }

        if (!metadata.schemaVersion.isEmpty() &&
            metadata.schemaVersion != QString::fromLatin1(kSupportedSchemaVersion)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Unsupported schema_version: %1").arg(metadata.schemaVersion);
            }
            return false;
        }
    } catch (const SQLite::Exception& ex) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(ex.what());
        }
        return false;
    }

    return true;
}

bool TranslationDatabase::installOverrideDatabase(const QString& sourcePath, QString* errorMessage) const
{
    if (!isValidDatabaseFile(sourcePath, errorMessage)) {
        return false;
    }

    const QString targetPath = overrideDatabasePath();
    const QString targetDir = QFileInfo(targetPath).absolutePath();
    if (!FileSystem::getInstance().ensureDirectoryExists(targetDir)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to create database directory");
        }
        return false;
    }

    const QString tempPath = targetPath + QStringLiteral(".tmp");
    QFile::remove(tempPath);
    if (!QFile::copy(sourcePath, tempPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to copy database update into place");
        }
        return false;
    }

    QString validationError;
    if (!isValidDatabaseFile(tempPath, &validationError)) {
        QFile::remove(tempPath);
        if (errorMessage) {
            *errorMessage = validationError;
        }
        return false;
    }

    QFile::remove(targetPath);
    if (!QFile::rename(tempPath, targetPath)) {
        QFile::remove(tempPath);
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to activate updated database");
        }
        return false;
    }

    return true;
}

QString TranslationDatabase::resolveDatabasePath() const
{
    const QString overridePath = overrideDatabasePath();
    const QString bundledPath = bundledDatabasePath();

    QString overrideError;
    const bool hasValidOverride =
        QFileInfo::exists(overridePath) && isValidDatabaseFile(overridePath, &overrideError);
    if (QFileInfo::exists(overridePath) && !hasValidOverride && !overrideError.isEmpty()) {
        qWarning() << "TranslationDatabase: override database invalid, falling back to bundled copy:"
                   << overrideError;
    }

    QString bundledError;
    const bool hasValidBundled =
        !bundledPath.isEmpty() && isValidDatabaseFile(bundledPath, &bundledError);
    if (!bundledPath.isEmpty() && !hasValidBundled && !bundledError.isEmpty()) {
        qWarning() << "TranslationDatabase: bundled database invalid:" << bundledError;
    }

    if (hasValidOverride && hasValidBundled) {
        const QString overrideVersion = AppUpdateManager::normalizeVersion(readMetadata(overridePath).releaseTag);
        const QString bundledVersion = AppUpdateManager::normalizeVersion(readMetadata(bundledPath).releaseTag);
        const int compareResult = AppUpdateManager::compareVersions(overrideVersion, bundledVersion);

        if (compareResult >= 0) {
            return overridePath;
        }

        qWarning() << "TranslationDatabase: override database is older than bundled copy, using bundled version"
                   << bundledVersion << "instead of" << overrideVersion;
        return bundledPath;
    }

    if (hasValidOverride) {
        return overridePath;
    }

    if (hasValidBundled) {
        return bundledPath;
    }

    return QString();
}
