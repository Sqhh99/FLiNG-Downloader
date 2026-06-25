#pragma once

#include <QByteArray>
#include <QObject>
#include <QJsonObject>
#include <QString>
#include <functional>

struct DatabaseReleaseInfo {
    QString version;
    QString releaseUrl;
    QString assetName;
    QString downloadUrl;
    QString publishedAt;
    QString source;

    bool isValid() const
    {
        return !version.isEmpty() && !assetName.isEmpty() && !downloadUrl.isEmpty();
    }
};

using DatabaseUpdateCheckCallback =
    std::function<void(bool, bool, const DatabaseReleaseInfo&, const QString&)>;
using DatabaseUpdateProgressCallback = std::function<void(qint64, qint64)>;
using DatabaseUpdateDownloadCallback =
    std::function<void(bool, const QString&, const QString&)>;

class DatabaseUpdateManager : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseUpdateManager(QObject* parent = nullptr);

    // source is "github" or "gitee"; the chosen source is used strictly (no
    // fallback to the other one).
    void checkForUpdates(const QString& currentVersion, const QString& source,
                         DatabaseUpdateCheckCallback callback);
    void downloadDatabase(const DatabaseReleaseInfo& releaseInfo,
                          DatabaseUpdateProgressCallback progressCallback,
                          DatabaseUpdateDownloadCallback finishedCallback);

private:
    void fetchLatestReleaseFromGithub(const QString& currentVersion,
                                      DatabaseUpdateCheckCallback callback);
    void fetchLatestReleaseFromGitee(const QString& currentVersion,
                                     const QString& previousError,
                                     DatabaseUpdateCheckCallback callback);
    DatabaseReleaseInfo parseReleaseResponse(const QByteArray& responseData, const QString& source) const;
    QString resolveDatabaseAssetUrl(const QJsonObject& assetObject) const;
};
