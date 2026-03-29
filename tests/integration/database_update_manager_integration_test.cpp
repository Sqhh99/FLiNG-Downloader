#include "fixtures/test_support.h"

#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "DatabaseUpdateManager.h"

namespace {
QByteArray makeGiteeDatabaseReleasePayload()
{
    const QJsonObject asset{
        {QStringLiteral("name"), QStringLiteral("fling_translations.db")},
        {QStringLiteral("download_url"),
         QStringLiteral("https://gitee.com/sqhh99/game-mappings-updater/releases/download/v2.0.0/"
                        "fling_translations.db")}
    };
    const QJsonObject release{
        {QStringLiteral("tag_name"), QStringLiteral("v2.0.0")},
        {QStringLiteral("html_url"),
         QStringLiteral("https://gitee.com/sqhh99/game-mappings-updater/releases/tag/v2.0.0")},
        {QStringLiteral("published_at"), QStringLiteral("2026-03-29T00:00:00Z")},
        {QStringLiteral("assets"), QJsonArray{asset}}
    };
    return QJsonDocument(release).toJson(QJsonDocument::Compact);
}
}

class DatabaseUpdateManagerIntegrationTest : public ::testing::Test
{
protected:
    TestSupport::ScopedNetworkHooks m_networkHooks;
};

TEST_F(DatabaseUpdateManagerIntegrationTest, FallsBackToGiteeWhenGithubFails)
{
    int githubRequests = 0;
    int giteeRequests = 0;
    bool callbackInvoked = false;
    bool success = false;
    bool updateAvailable = false;
    DatabaseReleaseInfo releaseInfo;

    m_networkHooks.setGetHandler([&githubRequests, &giteeRequests](const QString& url,
                                                                   const QString&,
                                                                   NetworkResponseCallback callback) {
        if (url.contains(QStringLiteral("api.github.com/repos/Sqhh99/game-mappings-updater/releases/latest"))) {
            ++githubRequests;
            if (callback) {
                callback(QByteArray(), false);
            }
            return true;
        }

        if (url.contains(QStringLiteral("gitee.com/api/v5/repos/sqhh99/game-mappings-updater/releases/latest"))) {
            ++giteeRequests;
            if (callback) {
                callback(makeGiteeDatabaseReleasePayload(), true);
            }
            return true;
        }

        return false;
    });

    DatabaseUpdateManager manager;
    manager.checkForUpdates(
        QStringLiteral("1.0.0"),
        [&callbackInvoked, &success, &updateAvailable, &releaseInfo](
            bool ok,
            bool available,
            const DatabaseReleaseInfo& info,
            const QString&) {
            callbackInvoked = true;
            success = ok;
            updateAvailable = available;
            releaseInfo = info;
        });

    EXPECT_EQ(githubRequests, 1);
    EXPECT_EQ(giteeRequests, 1);
    EXPECT_TRUE(callbackInvoked);
    EXPECT_TRUE(success);
    EXPECT_TRUE(updateAvailable);
    EXPECT_EQ(releaseInfo.version, QStringLiteral("2.0.0"));
    EXPECT_EQ(releaseInfo.source, QStringLiteral("gitee"));
    EXPECT_EQ(releaseInfo.assetName, QStringLiteral("fling_translations.db"));
}
