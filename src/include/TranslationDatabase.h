#pragma once

#include <QList>
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
};
