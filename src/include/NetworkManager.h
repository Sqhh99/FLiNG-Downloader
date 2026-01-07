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

// 网络响应回调函数类型定义
using NetworkResponseCallback = std::function<void(const QByteArray&, bool)>;
using DownloadProgressCallback = std::function<void(qint64, qint64)>;

class NetworkManager : public QObject
{
    Q_OBJECT

public:
    static NetworkManager& getInstance();

    // 发送GET请求
    void sendGetRequest(const QString& url, 
                        NetworkResponseCallback callback, 
                        const QString& userAgent = QString());

    // 下载文件
    void downloadFile(const QString& url, 
                      const QString& savePath,
                      DownloadProgressCallback progressCallback,
                      std::function<void(bool, const QString&)> finishedCallback,
                      const QString& userAgent = QString());

    // 中止所有请求
    void abortAllRequests();
    
    // 取消当前下载
    void cancelDownload();

    // 设置全局超时时间(毫秒)
    void setTimeoutInterval(int msec);

    // 设置全局用户代理
    void setGlobalUserAgent(const QString& userAgent);

    // 获取全局用户代理
    QString getGlobalUserAgent() const;

private:
    NetworkManager(QObject* parent = nullptr);
    ~NetworkManager();

    // 禁止拷贝和赋值
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    // 为请求创建定时器
    QTimer* createTimeoutTimer(QNetworkReply* reply);

private:
    QNetworkAccessManager* m_networkManager;
    int m_timeoutInterval;
    QString m_globalUserAgent;
    QNetworkReply* m_currentDownloadReply;

private slots:
    void onTimeoutTriggered();
}; 