#pragma once

#include <QDateTime>
#include <QList>
#include <QMutex>
#include <QString>

struct TranslationDatabaseMetadata {
    QString releaseTag;
    QString schemaVersion;

    bool isValid() const
    {
        return !releaseTag.isEmpty();
    }
};

struct TranslationGameRecord {
    QString english;
    QString normalizedEnglish;
    QString chineseSimplified;
    QString japanese;
};

class TranslationDatabase
{
public:
    static TranslationDatabase& getInstance();

    QString databasePath() const;
    QString bundledDatabasePath() const;
    QString overrideDatabasePath() const;
    bool isAvailable() const;
    QList<TranslationGameRecord> loadAllGames() const;
    TranslationDatabaseMetadata readMetadata() const;
    TranslationDatabaseMetadata readMetadata(const QString& path) const;
    QString currentReleaseTag() const;
    bool isValidDatabaseFile(const QString& path, QString* errorMessage = nullptr) const;
    bool installOverrideDatabase(const QString& sourcePath, QString* errorMessage = nullptr) const;

private:
    TranslationDatabase() = default;
    QString resolveDatabasePath() const;

    // Resolving opens and validates both candidate files (about six SQLite
    // opens), and startup asks several times; the game table is also read by
    // both GameMappingManager and Backend. Both answers are reused until
    // either candidate changes on disk or an update is installed.
    struct PathCacheKey {
        QString overridePath;
        QDateTime overrideModified;
        qint64 overrideSize = -1;
        QString bundledPath;
        QDateTime bundledModified;
        qint64 bundledSize = -1;

        bool operator==(const PathCacheKey& other) const
        {
            return overridePath == other.overridePath
                && overrideModified == other.overrideModified
                && overrideSize == other.overrideSize
                && bundledPath == other.bundledPath
                && bundledModified == other.bundledModified
                && bundledSize == other.bundledSize;
        }
    };
    PathCacheKey currentPathCacheKey() const;
    void invalidatePathCache() const;

    mutable QMutex m_pathCacheMutex;
    mutable bool m_hasCachedPath = false;
    mutable PathCacheKey m_cachedPathKey;
    mutable QString m_cachedPath;
    mutable bool m_hasCachedGames = false;
    mutable PathCacheKey m_cachedGamesKey;
    mutable QList<TranslationGameRecord> m_cachedGames;
};
