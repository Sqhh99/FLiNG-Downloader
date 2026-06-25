#include "DatabaseUpdateManager.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "AppUpdateManager.h"
#include "FileSystem.h"
#include "NetworkManager.h"

namespace {
constexpr const char* kGithubLatestReleaseUrl =
    "https://api.github.com/repos/Sqhh99/game-mappings-updater/releases/latest";
constexpr const char* kGiteeLatestReleaseUrl =
    "https://gitee.com/api/v5/repos/sqhh99/game-mappings-updater/releases/latest";
constexpr const char* kDatabaseAssetName = "fling_translations.db";

QString joinErrorMessages(const QString& previousError, const QString& nextError)
{
    if (previousError.isEmpty()) {
        return nextError;
    }
    if (nextError.isEmpty()) {
        return previousError;
    }

    return QStringLiteral("%1; %2").arg(previousError, nextError);
}
}

DatabaseUpdateManager::DatabaseUpdateManager(QObject* parent)
    : QObject(parent)
{
}

void DatabaseUpdateManager::checkForUpdates(const QString& currentVersion,
                                            const QString& source,
                                            DatabaseUpdateCheckCallback callback)
{
    if (source == QLatin1String("gitee")) {
        fetchLatestReleaseFromGitee(currentVersion, QString(), callback);
    } else {
        fetchLatestReleaseFromGithub(currentVersion, callback);
    }
}

void DatabaseUpdateManager::downloadDatabase(const DatabaseReleaseInfo& releaseInfo,
                                             DatabaseUpdateProgressCallback progressCallback,
                                             DatabaseUpdateDownloadCallback finishedCallback)
{
    if (!releaseInfo.isValid()) {
        if (finishedCallback) {
            finishedCallback(false, QString(), tr("Database update information is incomplete"));
        }
        return;
    }

    const QString updatesDir = QDir(FileSystem::getInstance().getCacheDirectory()).filePath("db-updates");
    if (!FileSystem::getInstance().ensureDirectoryExists(updatesDir)) {
        if (finishedCallback) {
            finishedCallback(false, QString(), tr("Failed to create database update cache directory"));
        }
        return;
    }

    const QString versionToken = AppUpdateManager::normalizeVersion(releaseInfo.version)
        .replace('/', '_')
        .replace('\\', '_');
    const QString tempFileName = QStringLiteral("fling_translations-%1.db").arg(versionToken);
    const QString dbPath = QDir(updatesDir).filePath(tempFileName);

    NetworkManager::getInstance().downloadFile(
        releaseInfo.downloadUrl,
        dbPath,
        this,
        [progressCallback](qint64 bytesReceived, qint64 bytesTotal) {
            if (progressCallback) {
                progressCallback(bytesReceived, bytesTotal);
            }
        },
        [finishedCallback, dbPath](bool success, const QString& errorMsg) {
            if (!finishedCallback) {
                return;
            }

            if (success) {
                finishedCallback(true, dbPath, QString());
            } else {
                finishedCallback(false, QString(), errorMsg);
            }
        });
}

void DatabaseUpdateManager::fetchLatestReleaseFromGithub(const QString& currentVersion,
                                                         DatabaseUpdateCheckCallback callback)
{
    NetworkManager::getInstance().sendGetRequest(
        QString::fromLatin1(kGithubLatestReleaseUrl),
        this,
        [this, currentVersion, callback](const QByteArray& responseData, bool success) {
            if (!success) {
                if (callback) {
                    callback(false, false, DatabaseReleaseInfo(),
                             tr("GitHub database release request failed"));
                }
                return;
            }

            const DatabaseReleaseInfo releaseInfo =
                parseReleaseResponse(responseData, QStringLiteral("github"));
            if (!releaseInfo.isValid()) {
                if (callback) {
                    callback(false, false, DatabaseReleaseInfo(),
                             tr("GitHub database release response did not contain a valid database asset"));
                }
                return;
            }

            const bool updateAvailable =
                AppUpdateManager::compareVersions(currentVersion, releaseInfo.version) < 0;
            if (callback) {
                callback(true, updateAvailable, releaseInfo, QString());
            }
        });
}

void DatabaseUpdateManager::fetchLatestReleaseFromGitee(const QString& currentVersion,
                                                        const QString& previousError,
                                                        DatabaseUpdateCheckCallback callback)
{
    NetworkManager::getInstance().sendGetRequest(
        QString::fromLatin1(kGiteeLatestReleaseUrl),
        this,
        [this, currentVersion, previousError, callback](const QByteArray& responseData, bool success) {
            if (!success) {
                if (callback) {
                    callback(false,
                             false,
                             DatabaseReleaseInfo(),
                             joinErrorMessages(previousError, tr("Gitee database release request failed")));
                }
                return;
            }

            const DatabaseReleaseInfo releaseInfo =
                parseReleaseResponse(responseData, QStringLiteral("gitee"));
            if (!releaseInfo.isValid()) {
                if (callback) {
                    callback(
                        false,
                        false,
                        DatabaseReleaseInfo(),
                        joinErrorMessages(
                            previousError,
                            tr("Gitee database release response did not contain a valid database asset")));
                }
                return;
            }

            const bool updateAvailable =
                AppUpdateManager::compareVersions(currentVersion, releaseInfo.version) < 0;
            if (callback) {
                callback(true, updateAvailable, releaseInfo, QString());
            }
        });
}

DatabaseReleaseInfo DatabaseUpdateManager::parseReleaseResponse(const QByteArray& responseData,
                                                                const QString& source) const
{
    DatabaseReleaseInfo releaseInfo;
    releaseInfo.source = source;

    const QJsonDocument document = QJsonDocument::fromJson(responseData);
    if (!document.isObject()) {
        return releaseInfo;
    }

    const QJsonObject releaseObject = document.object();
    releaseInfo.version = AppUpdateManager::normalizeVersion(
        releaseObject.value(QStringLiteral("tag_name")).toString());
    if (releaseInfo.version.isEmpty()) {
        releaseInfo.version = AppUpdateManager::normalizeVersion(
            releaseObject.value(QStringLiteral("tag")).toString());
    }
    if (releaseInfo.version.isEmpty()) {
        releaseInfo.version = AppUpdateManager::normalizeVersion(
            releaseObject.value(QStringLiteral("name")).toString());
    }
    releaseInfo.releaseUrl = releaseObject.value(QStringLiteral("html_url")).toString();
    if (releaseInfo.releaseUrl.isEmpty()) {
        releaseInfo.releaseUrl = releaseObject.value(QStringLiteral("url")).toString();
    }
    releaseInfo.publishedAt = releaseObject.value(QStringLiteral("published_at")).toString();
    if (releaseInfo.publishedAt.isEmpty()) {
        releaseInfo.publishedAt = releaseObject.value(QStringLiteral("created_at")).toString();
    }

    const QJsonArray assets = releaseObject.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue& assetValue : assets) {
        if (!assetValue.isObject()) {
            continue;
        }

        const QJsonObject assetObject = assetValue.toObject();
        const QString assetName = assetObject.value(QStringLiteral("name")).toString();
        if (assetName.compare(QString::fromLatin1(kDatabaseAssetName), Qt::CaseInsensitive) != 0) {
            continue;
        }

        const QString assetUrl = resolveDatabaseAssetUrl(assetObject);
        if (assetUrl.isEmpty()) {
            continue;
        }

        releaseInfo.assetName = assetName;
        releaseInfo.downloadUrl = assetUrl;
        break;
    }

    return releaseInfo;
}

QString DatabaseUpdateManager::resolveDatabaseAssetUrl(const QJsonObject& assetObject) const
{
    const QStringList candidateKeys = {
        QStringLiteral("browser_download_url"),
        QStringLiteral("download_url")
    };

    for (const QString& key : candidateKeys) {
        const QString value = assetObject.value(key).toString();
        if (value.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)
            || value.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)) {
            return value;
        }
    }

    return QString();
}
