#include "test_support.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <SQLiteCpp/SQLiteCpp.h>

#include <utility>

namespace {
std::string toUtf8String(const QString& value)
{
    const QByteArray utf8 = value.toUtf8();
    return std::string(utf8.constData(), static_cast<size_t>(utf8.size()));
}
}

namespace TestSupport {

ScopedNetworkHooks::ScopedNetworkHooks()
{
    NetworkManager::getInstance().resetTestHooks();
}

ScopedNetworkHooks::~ScopedNetworkHooks()
{
    NetworkManager::getInstance().resetTestHooks();
}

void ScopedNetworkHooks::setGetHandler(TestGetRequestHandler handler)
{
    NetworkManager::getInstance().setGetRequestHandlerForTesting(std::move(handler));
}

void ScopedNetworkHooks::setDownloadHandler(TestDownloadRequestHandler handler)
{
    NetworkManager::getInstance().setDownloadRequestHandlerForTesting(std::move(handler));
}

ScopedConfigState::ScopedConfigState()
    : m_autoCheckUpdates(ConfigManager::getInstance().getAutoCheckUpdates())
    , m_autoCheckDatabaseUpdates(ConfigManager::getInstance().getAutoCheckDatabaseUpdates())
    , m_language(ConfigManager::getInstance().getCurrentLanguage())
{
    ConfigManager::getInstance().setAutoCheckUpdates(false);
    ConfigManager::getInstance().setAutoCheckDatabaseUpdates(false);
}

ScopedConfigState::~ScopedConfigState()
{
    ConfigManager::getInstance().setAutoCheckUpdates(m_autoCheckUpdates);
    ConfigManager::getInstance().setAutoCheckDatabaseUpdates(m_autoCheckDatabaseUpdates);
    ConfigManager::getInstance().setCurrentLanguage(m_language);
}

void ScopedConfigState::setLanguage(ConfigManager::Language language)
{
    ConfigManager::getInstance().setCurrentLanguage(language);
}

bool containsChineseScript(const QString& text)
{
    for (const QChar& ch : text) {
        const ushort code = ch.unicode();
        if (code >= 0x4E00 && code <= 0x9FFF) {
            return true;
        }
    }
    return false;
}

bool containsJapaneseScript(const QString& text)
{
    for (const QChar& ch : text) {
        const ushort code = ch.unicode();
        if ((code >= 0x3040 && code <= 0x309F) || (code >= 0x30A0 && code <= 0x30FF)) {
            return true;
        }
    }
    return false;
}

QVariantMap findSuggestionItem(const QVariantList& items, const QString& searchKeyword)
{
    for (const QVariant& itemVariant : items) {
        const QVariantMap item = itemVariant.toMap();
        if (item.value("searchKeyword").toString() == searchKeyword) {
            return item;
        }
    }

    return {};
}

bool writeFileBytes(const QString& path, const QByteArray& bytes)
{
    const QFileInfo fileInfo(path);
    if (!QDir().mkpath(fileInfo.absolutePath())) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    if (file.write(bytes) != bytes.size()) {
        return false;
    }

    return true;
}

bool createTranslationDatabase(const QString& path, const QString& releaseTag)
{
    const QFileInfo fileInfo(path);
    if (!QDir().mkpath(fileInfo.absolutePath())) {
        return false;
    }

    QFile::remove(path);

    try {
        SQLite::Database db(
            toUtf8String(path),
            SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);

        db.exec("CREATE TABLE metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL)");
        db.exec("CREATE TABLE games ("
                "english TEXT NOT NULL, "
                "normalized_english TEXT, "
                "chinese_simplified TEXT, "
                "japanese TEXT)");

        SQLite::Statement metadataInsert(
            db,
            "INSERT INTO metadata(key, value) VALUES (?, ?)");
        metadataInsert.bind(1, "release_tag");
        metadataInsert.bind(2, toUtf8String(releaseTag));
        metadataInsert.exec();
        metadataInsert.reset();
        metadataInsert.bind(1, "schema_version");
        metadataInsert.bind(2, "1");
        metadataInsert.exec();

        SQLite::Statement gameInsert(
            db,
            "INSERT INTO games(english, normalized_english, chinese_simplified, japanese) "
            "VALUES (?, ?, ?, ?)");
        gameInsert.bind(1, "Sample Game");
        gameInsert.bind(2, "samplegame");
        gameInsert.bind(3, "示例游戏");
        gameInsert.bind(4, "サンプルゲーム");
        gameInsert.exec();
    } catch (const SQLite::Exception&) {
        QFile::remove(path);
        return false;
    }

    return true;
}

} // namespace TestSupport
