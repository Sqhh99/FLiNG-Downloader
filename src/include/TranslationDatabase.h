#pragma once

#include <QList>
#include <QString>

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
    bool isAvailable() const;
    QList<TranslationGameRecord> loadAllGames() const;

private:
    TranslationDatabase() = default;
    QString resolveDatabasePath() const;
};
