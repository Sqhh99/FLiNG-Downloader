#include "fixtures/test_support.h"

#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "AppUpdateManager.h"

namespace {
QByteArray makeGithubAppReleasePayload()
{
    const QJsonObject asset{
        {QStringLiteral("name"), QStringLiteral("FLiNG-Downloader-v1.2.0-win-x64-setup.exe")},
        {QStringLiteral("browser_download_url"),
         QStringLiteral("https://github.com/Sqhh99/FLiNG-Downloader/releases/download/v1.2.0/"
                        "FLiNG-Downloader-v1.2.0-win-x64-setup.exe")}
    };
    const QJsonObject release{
        {QStringLiteral("tag_name"), QStringLiteral("v1.2.0")},
        {QStringLiteral("html_url"),
         QStringLiteral("https://github.com/Sqhh99/FLiNG-Downloader/releases/tag/v1.2.0")},
        {QStringLiteral("published_at"), QStringLiteral("2026-03-29T00:00:00Z")},
        {QStringLiteral("assets"), QJsonArray{asset}}
    };
    return QJsonDocument(release).toJson(QJsonDocument::Compact);
}
}

class AppUpdateManagerTest : public ::testing::Test
{
protected:
    TestSupport::ScopedNetworkHooks m_networkHooks;
};

TEST_F(AppUpdateManagerTest, CompareVersionsHandlesPrereleaseAndBuildMetadata)
{
    EXPECT_LT(AppUpdateManager::compareVersions(QStringLiteral("1.2.3-beta.1"),
                                                QStringLiteral("1.2.3")),
              0);
    EXPECT_EQ(AppUpdateManager::compareVersions(QStringLiteral("1.2.3+build.5"),
                                                QStringLiteral("1.2.3")),
              0);
    EXPECT_GT(AppUpdateManager::compareVersions(QStringLiteral("1.2.4"),
                                                QStringLiteral("1.2.3")),
              0);
}

TEST_F(AppUpdateManagerTest, ParsesGithubReleaseResponse)
{
    bool callbackInvoked = false;
    bool success = false;
    bool updateAvailable = false;
    AppReleaseInfo releaseInfo;
    QString errorMessage;

    m_networkHooks.setGetHandler([](const QString& url,
                                    const QString&,
                                    NetworkResponseCallback callback) {
        if (!url.contains(QStringLiteral("api.github.com/repos/Sqhh99/FLiNG-Downloader/releases/latest"))) {
            return false;
        }

        if (callback) {
            callback(makeGithubAppReleasePayload(), true);
        }
        return true;
    });

    AppUpdateManager manager;
    manager.checkForUpdates(
        QStringLiteral("1.1.0"),
        QStringLiteral("github"),
        [&callbackInvoked, &success, &updateAvailable, &releaseInfo, &errorMessage](
            bool ok,
            bool available,
            const AppReleaseInfo& info,
            const QString& error) {
            callbackInvoked = true;
            success = ok;
            updateAvailable = available;
            releaseInfo = info;
            errorMessage = error;
        });

    EXPECT_TRUE(callbackInvoked);
    EXPECT_TRUE(success);
    EXPECT_TRUE(updateAvailable);
    EXPECT_TRUE(errorMessage.isEmpty());
    EXPECT_EQ(releaseInfo.version, QStringLiteral("1.2.0"));
    EXPECT_EQ(releaseInfo.source, QStringLiteral("github"));
    EXPECT_EQ(releaseInfo.installerAssetName,
              QStringLiteral("FLiNG-Downloader-v1.2.0-win-x64-setup.exe"));
}

TEST_F(AppUpdateManagerTest, ReportsGithubErrorWithoutGiteeFallback)
{
    bool callbackInvoked = false;
    bool success = true;
    QString errorMessage;

    m_networkHooks.setGetHandler([](const QString&, const QString&, NetworkResponseCallback callback) {
        if (callback) {
            callback(QByteArray(), false);
        }
        return true;
    });

    AppUpdateManager manager;
    manager.checkForUpdates(
        QStringLiteral("1.1.0"),
        QStringLiteral("github"),
        [&callbackInvoked, &success, &errorMessage](
            bool ok,
            bool,
            const AppReleaseInfo&,
            const QString& error) {
            callbackInvoked = true;
            success = ok;
            errorMessage = error;
        });

    EXPECT_TRUE(callbackInvoked);
    EXPECT_FALSE(success);
    EXPECT_EQ(errorMessage, QStringLiteral("GitHub release request failed"));
}
