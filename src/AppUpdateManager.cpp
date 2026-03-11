#include "AppUpdateManager.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include "FileSystem.h"
#include "NetworkManager.h"

namespace {
constexpr const char* kGithubLatestReleaseUrl =
    "https://api.github.com/repos/Sqhh99/FLiNG-Downloader/releases/latest";
constexpr const char* kGiteeLatestReleaseUrl =
    "https://gitee.com/api/v5/repos/sqhh99/fli-ng-downloader/releases/latest";
constexpr const char* kInstallerSuffix = "-win-x64-setup.exe";
constexpr const char* kInstallerPrefix = "FLiNG-Downloader-v";
}

AppUpdateManager::AppUpdateManager(QObject* parent)
    : QObject(parent)
{
}

void AppUpdateManager::checkForUpdates(const QString& currentVersion, AppUpdateCheckCallback callback)
{
    fetchLatestReleaseFromGithub(currentVersion, callback);
}

void AppUpdateManager::downloadInstaller(const AppReleaseInfo& releaseInfo,
                                         AppUpdateProgressCallback progressCallback,
                                         AppUpdateDownloadCallback finishedCallback)
{
    if (!releaseInfo.isValid()) {
        if (finishedCallback) {
            finishedCallback(false, QString(), tr("Update package information is incomplete"));
        }
        return;
    }

    const QString updatesDir = QDir(FileSystem::getInstance().getCacheDirectory()).filePath("updates");
    if (!FileSystem::getInstance().ensureDirectoryExists(updatesDir)) {
        if (finishedCallback) {
            finishedCallback(false, QString(), tr("Failed to create update cache directory"));
        }
        return;
    }

    const QString installerPath = QDir(updatesDir).filePath(releaseInfo.installerAssetName);
    NetworkManager::getInstance().downloadFile(
        releaseInfo.installerDownloadUrl,
        installerPath,
        [progressCallback](qint64 bytesReceived, qint64 bytesTotal) {
            if (progressCallback) {
                progressCallback(bytesReceived, bytesTotal);
            }
        },
        [finishedCallback, installerPath](bool success, const QString& errorMsg) {
            if (!finishedCallback) {
                return;
            }

            if (success) {
                finishedCallback(true, installerPath, QString());
            } else {
                finishedCallback(false, QString(), errorMsg);
            }
        });
}

QString AppUpdateManager::normalizeVersion(const QString& version)
{
    QString normalized = version.trimmed();
    if (normalized.startsWith('v', Qt::CaseInsensitive)) {
        normalized.remove(0, 1);
    }
    return normalized;
}

int AppUpdateManager::compareVersions(const QString& lhs, const QString& rhs)
{
    AppUpdateManager manager;
    const ParsedVersion left = manager.parseVersion(lhs);
    const ParsedVersion right = manager.parseVersion(rhs);

    if (!left.valid || !right.valid) {
        return QString::compare(normalizeVersion(lhs), normalizeVersion(rhs), Qt::CaseInsensitive);
    }

    if (left.major != right.major) {
        return left.major < right.major ? -1 : 1;
    }
    if (left.minor != right.minor) {
        return left.minor < right.minor ? -1 : 1;
    }
    if (left.patch != right.patch) {
        return left.patch < right.patch ? -1 : 1;
    }

    const bool leftHasPrerelease = !left.prerelease.isEmpty();
    const bool rightHasPrerelease = !right.prerelease.isEmpty();
    if (leftHasPrerelease != rightHasPrerelease) {
        return leftHasPrerelease ? -1 : 1;
    }
    if (!leftHasPrerelease) {
        return 0;
    }

    const int maxCount = qMax(left.prerelease.size(), right.prerelease.size());
    static const QRegularExpression numericPattern("^\\d+$");

    for (int i = 0; i < maxCount; ++i) {
        if (i >= left.prerelease.size()) {
            return -1;
        }
        if (i >= right.prerelease.size()) {
            return 1;
        }

        const QString& leftToken = left.prerelease.at(i);
        const QString& rightToken = right.prerelease.at(i);
        if (leftToken == rightToken) {
            continue;
        }

        const bool leftNumeric = numericPattern.match(leftToken).hasMatch();
        const bool rightNumeric = numericPattern.match(rightToken).hasMatch();

        if (leftNumeric && rightNumeric) {
            const qlonglong leftValue = leftToken.toLongLong();
            const qlonglong rightValue = rightToken.toLongLong();
            if (leftValue != rightValue) {
                return leftValue < rightValue ? -1 : 1;
            }
            continue;
        }

        if (leftNumeric != rightNumeric) {
            return leftNumeric ? -1 : 1;
        }

        const int compareResult = QString::compare(leftToken, rightToken, Qt::CaseInsensitive);
        if (compareResult != 0) {
            return compareResult < 0 ? -1 : 1;
        }
    }

    return 0;
}

void AppUpdateManager::fetchLatestReleaseFromGithub(const QString& currentVersion,
                                                    AppUpdateCheckCallback callback)
{
    NetworkManager::getInstance().sendGetRequest(
        QString::fromLatin1(kGithubLatestReleaseUrl),
        [this, currentVersion, callback](const QByteArray& responseData, bool success) {
            if (!success) {
                fetchLatestReleaseFromGitee(currentVersion,
                                            tr("GitHub release request failed"),
                                            callback);
                return;
            }

            const AppReleaseInfo releaseInfo = parseReleaseResponse(responseData, QStringLiteral("github"));
            if (!releaseInfo.isValid()) {
                fetchLatestReleaseFromGitee(currentVersion,
                                            tr("GitHub release response did not contain a valid installer asset"),
                                            callback);
                return;
            }

            const bool updateAvailable = compareVersions(currentVersion, releaseInfo.version) < 0;
            if (callback) {
                callback(true, updateAvailable, releaseInfo, QString());
            }
        });
}

void AppUpdateManager::fetchLatestReleaseFromGitee(const QString& currentVersion,
                                                   const QString& previousError,
                                                   AppUpdateCheckCallback callback)
{
    NetworkManager::getInstance().sendGetRequest(
        QString::fromLatin1(kGiteeLatestReleaseUrl),
        [this, currentVersion, previousError, callback](const QByteArray& responseData, bool success) {
            if (!success) {
                if (callback) {
                    callback(false,
                             false,
                             AppReleaseInfo(),
                             previousError + tr("; Gitee release request failed"));
                }
                return;
            }

            const AppReleaseInfo releaseInfo = parseReleaseResponse(responseData, QStringLiteral("gitee"));
            if (!releaseInfo.isValid()) {
                if (callback) {
                    callback(false,
                             false,
                             AppReleaseInfo(),
                             previousError + tr("; Gitee release response did not contain a valid installer asset"));
                }
                return;
            }

            const bool updateAvailable = compareVersions(currentVersion, releaseInfo.version) < 0;
            if (callback) {
                callback(true, updateAvailable, releaseInfo, QString());
            }
        });
}

AppReleaseInfo AppUpdateManager::parseReleaseResponse(const QByteArray& responseData, const QString& source) const
{
    AppReleaseInfo releaseInfo;
    releaseInfo.source = source;

    const QJsonDocument document = QJsonDocument::fromJson(responseData);
    if (!document.isObject()) {
        return releaseInfo;
    }

    const QJsonObject releaseObject = document.object();
    releaseInfo.version = normalizeVersion(
        releaseObject.value(QStringLiteral("tag_name")).toString());
    if (releaseInfo.version.isEmpty()) {
        releaseInfo.version = normalizeVersion(
            releaseObject.value(QStringLiteral("tag")).toString());
    }
    if (releaseInfo.version.isEmpty()) {
        releaseInfo.version = normalizeVersion(
            releaseObject.value(QStringLiteral("name")).toString());
    }
    if (!releaseInfo.version.isEmpty() && !parseVersion(releaseInfo.version).valid) {
        releaseInfo.version.clear();
    }
    releaseInfo.releaseUrl = releaseObject.value(QStringLiteral("html_url")).toString();
    if (releaseInfo.releaseUrl.isEmpty()) {
        releaseInfo.releaseUrl = releaseObject.value(QStringLiteral("url")).toString();
    }
    releaseInfo.publishedAt = releaseObject.value(QStringLiteral("published_at")).toString();
    if (releaseInfo.publishedAt.isEmpty()) {
        releaseInfo.publishedAt = releaseObject.value(QStringLiteral("created_at")).toString();
    }

    const QString expectedName = expectedInstallerAssetName(releaseInfo.version);
    const QJsonArray assets = releaseObject.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue& assetValue : assets) {
        if (!assetValue.isObject()) {
            continue;
        }

        const QJsonObject assetObject = assetValue.toObject();
        const QString assetName = assetObject.value(QStringLiteral("name")).toString();
        if (assetName.isEmpty()) {
            continue;
        }

        const bool exactMatch = !expectedName.isEmpty()
            && assetName.compare(expectedName, Qt::CaseInsensitive) == 0;

        QString assetVersion;
        if (exactMatch) {
            assetVersion = releaseInfo.version;
        } else if (releaseInfo.version.isEmpty()) {
            assetVersion = extractVersionFromInstallerAssetName(assetName);
            if (assetVersion.isEmpty()) {
                continue;
            }
        } else {
            continue;
        }

        const QString assetUrl = resolveInstallerAssetUrl(assetObject);
        if (assetUrl.isEmpty()) {
            continue;
        }

        releaseInfo.installerAssetName = assetName;
        releaseInfo.installerDownloadUrl = assetUrl;
        if (releaseInfo.version.isEmpty()) {
            releaseInfo.version = assetVersion;
        }
        break;
    }

    return releaseInfo;
}

QString AppUpdateManager::resolveInstallerAssetUrl(const QJsonObject& assetObject) const
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

QString AppUpdateManager::expectedInstallerAssetName(const QString& version) const
{
    const QString normalizedVersion = normalizeVersion(version);
    if (normalizedVersion.isEmpty()) {
        return QString();
    }

    return QStringLiteral("FLiNG-Downloader-v%1%2")
        .arg(normalizedVersion, QString::fromLatin1(kInstallerSuffix));
}

QString AppUpdateManager::extractVersionFromInstallerAssetName(const QString& assetName) const
{
    if (!assetName.startsWith(QString::fromLatin1(kInstallerPrefix), Qt::CaseInsensitive)
        || !assetName.endsWith(QString::fromLatin1(kInstallerSuffix), Qt::CaseInsensitive)) {
        return QString();
    }

    const int prefixLength = QString::fromLatin1(kInstallerPrefix).size();
    const int suffixLength = QString::fromLatin1(kInstallerSuffix).size();
    const QString version = assetName.mid(prefixLength, assetName.size() - prefixLength - suffixLength);
    return normalizeVersion(version);
}

AppUpdateManager::ParsedVersion AppUpdateManager::parseVersion(const QString& version) const
{
    ParsedVersion parsed;
    QString normalized = normalizeVersion(version);
    if (normalized.isEmpty()) {
        return parsed;
    }

    const int buildSeparator = normalized.indexOf('+');
    if (buildSeparator >= 0) {
        normalized = normalized.left(buildSeparator);
    }

    QString coreVersion = normalized;
    QString prereleaseText;
    const int prereleaseSeparator = normalized.indexOf('-');
    if (prereleaseSeparator >= 0) {
        coreVersion = normalized.left(prereleaseSeparator);
        prereleaseText = normalized.mid(prereleaseSeparator + 1);
    }

    const QStringList coreParts = coreVersion.split('.');
    if (coreParts.size() != 3) {
        return parsed;
    }

    bool majorOk = false;
    bool minorOk = false;
    bool patchOk = false;
    parsed.major = coreParts.at(0).toInt(&majorOk);
    parsed.minor = coreParts.at(1).toInt(&minorOk);
    parsed.patch = coreParts.at(2).toInt(&patchOk);
    if (!majorOk || !minorOk || !patchOk) {
        return parsed;
    }

    if (!prereleaseText.isEmpty()) {
        parsed.prerelease = prereleaseText.split('.', Qt::SkipEmptyParts);
    }
    parsed.valid = true;
    return parsed;
}
