#include "fixtures/test_support.h"

#include <gtest/gtest.h>

#include <QFile>
#include <QHash>

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

namespace {
// A detail page the parser accepts, with no download links on it.
constexpr auto kDetailPageWithoutVersions =
    "<html><body><h1>Some Trainer</h1><p>No downloads here.</p></body></html>";

ModifierInfo makeModifier(const QString& name, const QString& url)
{
    ModifierInfo info;
    info.name = name;
    info.url = url;
    info.optionsCount = 0;
    return info;
}
}

TEST_F(BackendTest, LateDetailReplyForAnotherGameIsIgnored)
{
    // Regression for the P1 in the 2026-09-04 review: the detail callback
    // captured only `this`, so a reply for the game the user had already
    // navigated away from wrote its versions, options and screenshot into the
    // current selection - and a download then paired this game's name with that
    // game's URL.
    QHash<QString, NetworkResponseCallback> pending;
    m_networkHooks.setGetHandler([&pending](const QString& url,
                                            const QString&,
                                            NetworkResponseCallback callback) {
        pending.insert(url, callback);  // hold the reply instead of answering
        return true;
    });

    Backend backend;
    backend.modifierListModel()->setModifiers(
        {makeModifier(QStringLiteral("Game A"), QStringLiteral("https://example.com/a")),
         makeModifier(QStringLiteral("Game B"), QStringLiteral("https://example.com/b"))});

    backend.selectModifier(0);
    backend.selectModifier(1);
    ASSERT_TRUE(pending.contains(QStringLiteral("https://example.com/a")));
    ASSERT_TRUE(pending.contains(QStringLiteral("https://example.com/b")));
    EXPECT_EQ(backend.detailState(), QStringLiteral("loading"));

    // A's reply lands after the user moved to B and must change nothing.
    pending.value(QStringLiteral("https://example.com/a"))(
        QByteArray(kDetailPageWithoutVersions), true);
    EXPECT_EQ(backend.detailState(), QStringLiteral("loading"));
    EXPECT_EQ(backend.selectedModifierName(), QStringLiteral("Game B"));

    // B's reply is the current one and is applied.
    pending.value(QStringLiteral("https://example.com/b"))(
        QByteArray(kDetailPageWithoutVersions), true);
    EXPECT_EQ(backend.detailState(), QStringLiteral("empty"));
}

TEST_F(BackendTest, DetailStateReportsFailureInsteadOfLoadingForever)
{
    // The drawer used to infer "loading" from "name set, no versions", so a
    // failed fetch left its overlay up permanently - covering the close button.
    Backend backend;  // the fixture's default hook answers every GET with failure
    backend.modifierListModel()->setModifiers(
        {makeModifier(QStringLiteral("Game A"), QStringLiteral("https://example.com/a"))});

    backend.selectModifier(0);
    EXPECT_EQ(backend.detailState(), QStringLiteral("error"));
}

TEST_F(BackendTest, DetailStateIsEmptyWhenTheSelectionHasNoDetailUrl)
{
    Backend backend;
    backend.modifierListModel()->setModifiers(
        {makeModifier(QStringLiteral("Game A"), QString())});

    backend.selectModifier(0);
    EXPECT_EQ(backend.detailState(), QStringLiteral("empty"));
}

TEST_F(BackendTest, UpdatingALibraryRowDoesNotResetTheModel)
{
    // Completing a download used to call setModifiers(), resetting the whole
    // model to change one row: every delegate is rebuilt and the user's
    // selection is dropped while the library is open.
    DownloadedModifierModel model;

    DownloadedModifierInfo first;
    first.name = QStringLiteral("Game A");
    first.version = QStringLiteral("v1");
    first.filePath = QStringLiteral("C:/downloads/a_v1.exe");
    model.addModifier(first);

    // QSignalSpy would pull in Qt::Test, which this target does not link.
    int resets = 0;
    int changes = 0;
    QObject::connect(&model, &QAbstractItemModel::modelReset,
                     [&resets]() { ++resets; });
    QObject::connect(&model, &QAbstractItemModel::dataChanged,
                     [&changes](const QModelIndex&, const QModelIndex&,
                                const QList<int>&) { ++changes; });

    DownloadedModifierInfo updated = first;
    updated.filePath = QStringLiteral("C:/downloads/a_v1_new.exe");
    model.updateModifier(0, updated);

    EXPECT_EQ(model.count(), 1);
    EXPECT_EQ(model.getModifier(0).filePath, updated.filePath);
    EXPECT_EQ(resets, 0);
    EXPECT_EQ(changes, 1);
}

TEST_F(BackendTest, UpdatingAnOutOfRangeLibraryRowIsIgnored)
{
    DownloadedModifierModel model;
    DownloadedModifierInfo info;
    info.name = QStringLiteral("Game A");

    model.updateModifier(0, info);   // empty model
    EXPECT_EQ(model.count(), 0);
}
