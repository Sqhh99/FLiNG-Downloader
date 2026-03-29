#include "fixtures/test_support.h"

#include <gtest/gtest.h>

#include <QFile>

#include "GameMappingManager.h"
#include "SearchManager.h"
#include "TranslationDatabase.h"

namespace {
constexpr auto kAceCombatJapanese = u8"エースコンバット7 スカイズ・アンノウン";
}

class SearchManagerIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        QFile::remove(TranslationDatabase::getInstance().overrideDatabasePath());
        ASSERT_TRUE(GameMappingManager::getInstance().reloadMappings());

        m_networkHooks.setGetHandler([](const QString&,
                                        const QString&,
                                        NetworkResponseCallback callback) {
            if (callback) {
                callback(QByteArray(), false);
            }
            return true;
        });

        SearchManager::getInstance().clearSearchHistory();
    }

    void TearDown() override
    {
        SearchManager::getInstance().clearSearchHistory();
        QFile::remove(TranslationDatabase::getInstance().overrideDatabasePath());
    }

    TestSupport::ScopedNetworkHooks m_networkHooks;
};

TEST_F(SearchManagerIntegrationTest, UsesCanonicalEnglishKeywordForJapaneseInput)
{
    QString capturedUrl;
    bool callbackInvoked = false;
    const QString expectedEnglish =
        GameMappingManager::getInstance().translateToEnglish(QString::fromUtf8(kAceCombatJapanese));

    ASSERT_FALSE(expectedEnglish.isEmpty());

    m_networkHooks.setGetHandler([&capturedUrl](const QString& url,
                                                const QString&,
                                                NetworkResponseCallback callback) {
        capturedUrl = url;
        if (callback) {
            callback(QByteArray(), false);
        }
        return true;
    });

    SearchManager::getInstance().searchModifiers(
        QString::fromUtf8(kAceCombatJapanese),
        [&callbackInvoked](const QList<ModifierInfo>& modifiers) {
            callbackInvoked = true;
            EXPECT_TRUE(modifiers.isEmpty());
        });

    EXPECT_TRUE(callbackInvoked);
    EXPECT_TRUE(capturedUrl.contains(QString(expectedEnglish).replace(QStringLiteral(" "), QStringLiteral("+"))));
}

TEST_F(SearchManagerIntegrationTest, PreservesBroadLatinQueriesForSiteSearch)
{
    QString capturedUrl;
    bool callbackInvoked = false;
    const QString query = QStringLiteral("ace combat");

    m_networkHooks.setGetHandler([&capturedUrl](const QString& url,
                                                const QString&,
                                                NetworkResponseCallback callback) {
        capturedUrl = url;
        if (callback) {
            callback(QByteArray(), false);
        }
        return true;
    });

    SearchManager::getInstance().searchModifiers(
        query,
        [&callbackInvoked](const QList<ModifierInfo>& modifiers) {
            callbackInvoked = true;
            EXPECT_TRUE(modifiers.isEmpty());
        });

    EXPECT_TRUE(callbackInvoked);
    EXPECT_TRUE(capturedUrl.contains(QStringLiteral("ace+combat")));
    EXPECT_FALSE(capturedUrl.contains(QStringLiteral("Ace+Combat+7")));
}
