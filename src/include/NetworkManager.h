#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QFile>
#include <QTimer>
#include <QString>
#include <QUrl>
#include <functional>
#include "ModifierParser.h"

// Network response callback function type definition
using NetworkResponseCallback = std::function<void(const QByteArray&, bool)>;
using DownloadProgressCallback = std::function<void(qint64, qint64)>;

class NetworkManager : public QObject
{
    Q_OBJECT

public:
    static NetworkManager& getInstance();

    // Send GET request
    void sendGetRequest(const QString& url, 
                        NetworkResponseCallback callback, 
                        const QString& userAgent = QString());

    // Download file
    void downloadFile(const QString& url, 
                      const QString& savePath,
                      DownloadProgressCallback progressCallback,
                      std::function<void(bool, const QString&)> finishedCallback,
                      const QString& userAgent = QString());

    // Abort all requests
    void abortAllRequests();
    
    // Cancel current download
    void cancelDownload();

    // Set global timeout interval (milliseconds)
    void setTimeoutInterval(int msec);

    // Set global user agent
    void setGlobalUserAgent(const QString& userAgent);

    // Get global user agent
    QString getGlobalUserAgent() const;

private:
    NetworkManager(QObject* parent = nullptr);
    ~NetworkManager();

    // Disable copy and assignment
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    // Create timeout timer for request
    QTimer* createTimeoutTimer(QNetworkReply* reply);

private:
    QNetworkAccessManager* m_networkManager;
    int m_timeoutInterval;
    QString m_globalUserAgent;
    QNetworkReply* m_currentDownloadReply;

private slots:
    void onTimeoutTriggered();
}; 