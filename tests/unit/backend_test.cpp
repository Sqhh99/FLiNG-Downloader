#include "fixtures/test_support.h"

#include <gtest/gtest.h>

#include <QFile>

#include "Backend.h"
#include "TranslationDatabase.h"

namespace {
constexpr auto kSekiroEnglish = u8"Sekiro: Shadows Die Twice";
constexpr auto kAceCombatEnglish = u8"Ace Combat 7: Skies Unknown";
}

class BackendTest : public ::testing::Test
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
    }

    void TearDown() override
    {
        QFile::remove(TranslationDatabase::getInstance().overrideDatabasePath());
    }

    TestSupport::ScopedConfigState m_configState;
    TestSupport::ScopedNetworkHooks m_networkHooks;
};

TEST_F(BackendTest, SuggestionItemsUseChineseDisplayForEnglishQueriesInChineseUi)
{
    m_configState.setLanguage(ConfigManager::Language::Chinese);

    Backend backend;
    const QVariantMap item = TestSupport::findSuggestionItem(
        backend.getSuggestionItems(QStringLiteral("sekiro")),
        QString::fromUtf8(kSekiroEnglish));

    ASSERT_FALSE(item.isEmpty());
    EXPECT_EQ(item.value("searchKeyword").toString(), QString::fromUtf8(kSekiroEnglish));
    EXPECT_EQ(item.value("inputText").toString(), QString::fromUtf8(kSekiroEnglish));

    const QString displayText = item.value("displayText").toString();
    EXPECT_TRUE(displayText.contains(QString::fromUtf8(kSekiroEnglish)));
    EXPECT_TRUE(TestSupport::containsChineseScript(displayText));
}

TEST_F(BackendTest, SuggestionItemsUseJapaneseDisplayForEnglishQueriesInJapaneseUi)
{
    m_configState.setLanguage(ConfigManager::Language::Japanese);

    Backend backend;
    const QVariantMap item = TestSupport::findSuggestionItem(
        backend.getSuggestionItems(QStringLiteral("ace combat")),
        QString::fromUtf8(kAceCombatEnglish));

    ASSERT_FALSE(item.isEmpty());
    EXPECT_EQ(item.value("searchKeyword").toString(), QString::fromUtf8(kAceCombatEnglish));
    EXPECT_EQ(item.value("inputText").toString(), QString::fromUtf8(kAceCombatEnglish));

    const QString displayText = item.value("displayText").toString();
    EXPECT_TRUE(displayText.contains(QString::fromUtf8(kAceCombatEnglish)));
    EXPECT_TRUE(TestSupport::containsJapaneseScript(displayText));
}
