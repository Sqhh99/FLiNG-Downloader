#include "fixtures/test_support.h"

#include <gtest/gtest.h>

#include <QFile>

#include "GameMappingManager.h"
#include "TranslationDatabase.h"

namespace {
constexpr auto kAceCombatJapanese = u8"エースコンバット7 スカイズ・アンノウン";
constexpr auto kAceCombatEnglish = u8"Ace Combat 7: Skies Unknown";
}

class GameMappingManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        QFile::remove(TranslationDatabase::getInstance().overrideDatabasePath());
        ASSERT_TRUE(GameMappingManager::getInstance().reloadMappings());
    }

    void TearDown() override
    {
        QFile::remove(TranslationDatabase::getInstance().overrideDatabasePath());
    }
};

TEST_F(GameMappingManagerTest, TranslateToEnglishResolvesJapaneseTitle)
{
    EXPECT_EQ(
        GameMappingManager::getInstance().translateToEnglish(QString::fromUtf8(kAceCombatJapanese)),
        QString::fromUtf8(kAceCombatEnglish));
}
