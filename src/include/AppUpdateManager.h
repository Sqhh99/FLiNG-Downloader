#pragma once

#include <QObject>
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <functional>

struct AppReleaseInfo {
    QString version;
    QString releaseUrl;
    QString installerAssetName;
    QString installerDownloadUrl;
    QString publishedAt;
    QString source;

    bool isValid() const
    {
        return !version.isEmpty()
            && !installerAssetName.isEmpty()
            && !installerDownloadUrl.isEmpty();
    }
};

using AppUpdateCheckCallback =
    std::function<void(bool, bool, const AppReleaseInfo&, const QString&)>;
using AppUpdateProgressCallback = std::function<void(qint64, qint64)>;
using AppUpdateDownloadCallback = std::function<void(bool, const QString&, const QString&)>;

class AppUpdateManager : public QObject
{
    Q_OBJECT

public:
    explicit AppUpdateManager(QObject* parent = nullptr);

    void checkForUpdates(const QString& currentVersion, AppUpdateCheckCallback callback);
    void downloadInstaller(const AppReleaseInfo& releaseInfo,
                           AppUpdateProgressCallback progressCallback,
                           AppUpdateDownloadCallback finishedCallback);

    static QString normalizeVersion(const QString& version);
    static int compareVersions(const QString& lhs, const QString& rhs);

private:
    struct ParsedVersion {
        int major = 0;
        int minor = 0;
        int patch = 0;
        QStringList prerelease;
        bool valid = false;
    };

    void fetchLatestReleaseFromGithub(const QString& currentVersion,
                                      AppUpdateCheckCallback callback);
    void fetchLatestReleaseFromGitee(const QString& currentVersion,
                                     const QString& previousError,
                                     AppUpdateCheckCallback callback);
    AppReleaseInfo parseReleaseResponse(const QByteArray& responseData, const QString& source) const;
    QString resolveInstallerAssetUrl(const QJsonObject& assetObject) const;
    QString expectedInstallerAssetName(const QString& version) const;
    ParsedVersion parseVersion(const QString& version) const;
};
