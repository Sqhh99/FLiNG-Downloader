#include "NetworkManager.h"
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include "ConfigManager.h"

NetworkManager::NetworkManager(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_timeoutInterval(30000) // Default 30 second timeout
{
    // Set default user agent
    m_globalUserAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";
}

NetworkManager::~NetworkManager()
{
    abortAllRequests();
}

NetworkManager& NetworkManager::getInstance()
{
    static NetworkManager instance;
    return instance;
}

void NetworkManager::sendGetRequest(const QString& url, NetworkResponseCallback callback, const QString& userAgent)
{
    QNetworkRequest request((QUrl(url)));
    
    // Set user agent
    QString effectiveUserAgent = userAgent.isEmpty() ? m_globalUserAgent : userAgent;
    request.setHeader(QNetworkRequest::UserAgentHeader, effectiveUserAgent);
    
    // Send GET request
    QNetworkReply* reply = m_networkManager->get(request);
    
    // Set timeout
    QTimer* timer = createTimeoutTimer(reply);
    
    // Connect finished signal
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback, timer]() {
        timer->stop();
        timer->deleteLater();
        
        if (reply->error() == QNetworkReply::NoError) {
            // Read response data
            QByteArray responseData = reply->readAll();
            callback(responseData, true);
        } else {
            qDebug() << "Network request failed:" << reply->errorString();
            callback(QByteArray(), false);
        }
        
        // Clean up resources
        reply->deleteLater();
    });
    
    // Connect error signal
    connect(reply, &QNetworkReply::errorOccurred, this, [reply, callback, timer](QNetworkReply::NetworkError) {
        timer->stop();
        qDebug() << "Network error:" << reply->errorString();
    });
}

void NetworkManager::downloadFile(const QString& url, 
                                  const QString& savePath,
                                  DownloadProgressCallback progressCallback,
                                  std::function<void(bool, const QString&)> finishedCallback,
                                  const QString& userAgent)
{
    // Ensure directory exists
    QFileInfo fileInfo(savePath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // Create file
    QFile* file = new QFile(savePath);
    if (!file->open(QIODevice::WriteOnly)) {
        qDebug() << "Cannot create file:" << file->errorString();
        finishedCallback(false, "Cannot create file: " + file->errorString());
        delete file;
        return;
    }
    
    // Create network request
    QNetworkRequest request((QUrl(url)));
    
    // Set user agent
    QString effectiveUserAgent = userAgent.isEmpty() ? m_globalUserAgent : userAgent;
    request.setHeader(QNetworkRequest::UserAgentHeader, effectiveUserAgent);
    
    // Start download
    QNetworkReply* reply = m_networkManager->get(request);
    m_currentDownloadReply = reply; // Save current download reply
    
    // Set timeout
    QTimer* timer = createTimeoutTimer(reply);
    
    // Connect progress signal
    connect(reply, &QNetworkReply::downloadProgress, this, [progressCallback, timer](qint64 bytesReceived, qint64 bytesTotal) {
        timer->start(); // Reset timer
        progressCallback(bytesReceived, bytesTotal);
    });
    
    // Connect data ready signal
    connect(reply, &QNetworkReply::readyRead, this, [reply, file, timer]() {
        timer->start(); // Reset timer
        file->write(reply->readAll());
    });
    
    // Connect finished signal
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, finishedCallback, timer]() {
        timer->stop();
        timer->deleteLater();
        
        file->close();
        
        if (reply->error() == QNetworkReply::NoError) {
            finishedCallback(true, QString());
        } else {
            qDebug() << "Download failed:" << reply->errorString();
            file->remove(); // Delete incomplete file
            finishedCallback(false, reply->errorString());
        }
        
        // Clean up resources
        file->deleteLater();
        reply->deleteLater();
        
        // Clear current download reply
        if (m_currentDownloadReply == reply) {
            m_currentDownloadReply = nullptr;
        }
    });
    
    // Connect error signal
    connect(reply, &QNetworkReply::errorOccurred, this, [file, timer](QNetworkReply::NetworkError) {
        timer->start(); // Reset timer
    });
}

void NetworkManager::abortAllRequests()
{
    const auto replies = m_networkManager->findChildren<QNetworkReply*>();
    for (QNetworkReply* reply : replies) {
        reply->abort();
    }
}

void NetworkManager::setTimeoutInterval(int msec)
{
    m_timeoutInterval = msec;
}

void NetworkManager::setGlobalUserAgent(const QString& userAgent)
{
    m_globalUserAgent = userAgent;
}

QString NetworkManager::getGlobalUserAgent() const
{
    return m_globalUserAgent;
}

QTimer* NetworkManager::createTimeoutTimer(QNetworkReply* reply)
{
    QTimer* timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(m_timeoutInterval);
    
    connect(timer, &QTimer::timeout, this, &NetworkManager::onTimeoutTriggered);
    connect(timer, &QTimer::timeout, reply, [reply]() {
        if (reply && reply->isRunning()) {
            reply->abort();
        }
    });
    
    timer->start();
    return timer;
}

void NetworkManager::onTimeoutTriggered()
{
    qDebug() << "Network request timeout";
}

void NetworkManager::cancelDownload()
{
    if (m_currentDownloadReply && m_currentDownloadReply->isRunning()) {
        qDebug() << "Cancelling download";
        m_currentDownloadReply->abort();
    }
} 