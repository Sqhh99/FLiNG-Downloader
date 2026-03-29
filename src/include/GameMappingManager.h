#pragma once

#include <QObject>
#include <QMap>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <functional>

struct GameMappingInfo {
    QString english;
    QString category;
    bool isTranslated = false;
};

class GameMappingManager : public QObject
{
    Q_OBJECT

public:
    static GameMappingManager& getInstance();

    // Load bundled translation mappings from the SQLite database.
    bool initialize();

    // Translate a Chinese game name to the FLiNG English title.
    QString translateToEnglish(const QString& chinese);

    // Compatibility wrapper around the synchronous local DB lookup.
    void translateToEnglishAsync(const QString& chinese, std::function<void(const QString&)> callback);

    QString fuzzyMatch(const QString& input) const;
    bool containsChinese(const QString& text) const;
    QStringList getAllChineseNames() const;
    bool reloadMappings();

private:
    GameMappingManager(QObject* parent = nullptr);
    ~GameMappingManager() = default;

    GameMappingManager(const GameMappingManager&) = delete;
    GameMappingManager& operator=(const GameMappingManager&) = delete;

    bool loadBuiltinMappings();
    double calculateSimilarity(const QString& str1, const QString& str2) const;

private:
    QMap<QString, GameMappingInfo> m_builtinMappings;
    mutable QMutex m_mutex;
    bool m_initialized;
};
