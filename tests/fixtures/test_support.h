#pragma once

#include <QByteArray>
#include <QVariantList>
#include <QVariantMap>

#include "ConfigManager.h"
#include "NetworkManager.h"

namespace TestSupport {

class ScopedNetworkHooks
{
public:
    ScopedNetworkHooks();
    ~ScopedNetworkHooks();

    void setGetHandler(TestGetRequestHandler handler);
    void setDownloadHandler(TestDownloadRequestHandler handler);
};

class ScopedConfigState
{
public:
    ScopedConfigState();
    ~ScopedConfigState();

    void setLanguage(ConfigManager::Language language);

private:
    bool m_autoCheckUpdates;
    bool m_autoCheckDatabaseUpdates;
    ConfigManager::Language m_language;
};

bool containsChineseScript(const QString& text);
bool containsJapaneseScript(const QString& text);
QVariantMap findSuggestionItem(const QVariantList& items, const QString& searchKeyword);
bool writeFileBytes(const QString& path, const QByteArray& bytes);
bool createTranslationDatabase(const QString& path, const QString& releaseTag, const QString& schemaVersion = QStringLiteral("1"));
bool createTranslationDatabaseMissingNormalizedEnglish(const QString& path, const QString& releaseTag);

} // namespace TestSupport
