#include "fixtures/test_support.h"

#include <gtest/gtest.h>

#include <QFile>

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
    EXPECT_TRUE(capturedUrl.contains(QStringLiteral("Ace+Combat+7:+Skies+Unknown")));
}
