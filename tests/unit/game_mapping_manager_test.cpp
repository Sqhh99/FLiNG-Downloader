#include "fixtures/test_support.h"

#include <gtest/gtest.h>

#include <QFile>

#include "GameMappingManager.h"
#include "TranslationDatabase.h"

namespace {
constexpr auto kAceCombatJapanese = u8"エースコンバット7 スカイズ・アンノウン";
constexpr auto kAceCombatNormalizedEnglish = u8"ace combat 7 skies unknown";
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
    const QString translated =
        GameMappingManager::getInstance().translateToEnglish(QString::fromUtf8(kAceCombatJapanese));

    EXPECT_FALSE(translated.isEmpty());
    EXPECT_TRUE(translated.contains(QStringLiteral("Ace Combat 7"), Qt::CaseInsensitive));
}

TEST_F(GameMappingManagerTest, TranslateToEnglishResolvesNormalizedEnglishVariant)
{
    const QString expected =
        GameMappingManager::getInstance().translateToEnglish(QString::fromUtf8(kAceCombatJapanese));

    ASSERT_FALSE(expected.isEmpty());
    EXPECT_EQ(
        GameMappingManager::getInstance().translateToEnglish(QString::fromUtf8(kAceCombatNormalizedEnglish)),
        expected);
}
