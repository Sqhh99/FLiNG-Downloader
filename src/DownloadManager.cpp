#include "DownloadManager.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>

DownloadManager::DownloadManager(QObject* parent)
    : QObject(parent)
    , m_isDownloading(false)
{
}

void DownloadManager::downloadFile(const QString& url, 
                                  const QString& savePath,
                                  DLProgressCallback progressCallback,
                                  DLCompletedCallback completedCallback)
{
    if (m_isDownloading) {
        if (completedCallback) {
            completedCallback(false, "Download already in progress", savePath);
        }
        return;
    }
    
    QString cleanedUrl = cleanUrl(url);
    
    if (cleanedUrl.isEmpty()) {
        if (completedCallback) {
            completedCallback(false, "Download URL is empty", savePath);
        }
        return;
    }
    
    m_isDownloading = true;
    
    // Use NetworkManager to download file
    NetworkManager::getInstance().downloadFile(
        cleanedUrl,
        savePath,
        [this, progressCallback](qint64 bytesReceived, qint64 bytesTotal) {
            if (bytesTotal > 0) {
                int progress = static_cast<int>((bytesReceived * 100) / bytesTotal);
                if (progressCallback) {
                    progressCallback(progress);
                }
            }
        },        [this, completedCallback, savePath](bool success, const QString& errorMsg) {
            m_isDownloading = false;
            
            if (success) {
                QString correctedPath = correctFileExtension(savePath);
                bool pathChanged = (correctedPath != savePath);
                
                if (completedCallback) {
                    if (pathChanged) {
                        QFileInfo newFileInfo(correctedPath);
                        QString message = QString("File format detected and renamed to: %1").arg(newFileInfo.fileName());
                        completedCallback(true, message, correctedPath);
                    } else {
                        completedCallback(true, errorMsg, correctedPath);
                    }
                }
            } else {
                if (completedCallback) {
                    completedCallback(false, errorMsg, savePath);
                }
            }
        }
    );
}

void DownloadManager::downloadModifier(const ModifierInfo& modifier,
                                      const QString& version,
                                      const QString& savePath,
                                      std::function<void(bool, const QString&, const QString&, const ModifierInfo&, bool)> completedCallback,
                                      DLProgressCallback progressCallback)
{
    qDebug() << "DownloadManager: downloadModifier called for:" << modifier.name;
    qDebug() << "DownloadManager: Requested version:" << version;
    qDebug() << "DownloadManager: Available versions count:" << modifier.versions.size();
    
    // Find the selected version link
    QString url;
    for (const auto& ver : modifier.versions) {
        qDebug() << "DownloadManager: Checking version:" << ver.first << "URL:" << ver.second;
        if (ver.first == version) {
            url = ver.second;
            qDebug() << "DownloadManager: Found matching version, URL:" << url;
            break;
        }
    }
    
    if (url.isEmpty() && !modifier.versions.isEmpty()) {
        url = modifier.versions.first().second;
        qDebug() << "DownloadManager: Using first available version, URL:" << url;
    }
    
    if (url.isEmpty()) {
        qDebug() << "DownloadManager: ERROR - No download URL found!";
        if (completedCallback) {
            completedCallback(false, "Download URL not found", savePath, modifier, false);
        }
        return;
    }
    
    qDebug() << "DownloadManager: Final download URL:" << url;
    qDebug() << "DownloadManager: Save path:" << savePath;
    
    bool isArchive = isArchiveFormat(url) || isArchiveFormat(savePath);
    
    // Download file
    downloadFile(
        url,
        savePath,
        progressCallback,
        [completedCallback, modifier, isArchive](bool success, const QString& errorMsg, const QString& actualPath) {
            if (completedCallback) {
                completedCallback(success, errorMsg, actualPath, modifier, isArchive);
            }
        }
    );
}

void DownloadManager::cancelDownload()
{
    if (m_isDownloading) {
        NetworkManager::getInstance().cancelDownload();
        m_isDownloading = false;
    }
}

bool DownloadManager::isDownloading() const
{
    return m_isDownloading;
}

QString DownloadManager::cleanUrl(const QString& url) const
{
    QString cleanedUrl = url.trimmed();
    
    while (cleanedUrl.endsWith(",")) {
        cleanedUrl.chop(1);
    }
    
    if (!cleanedUrl.startsWith("http://") && !cleanedUrl.startsWith("https://")) {
        return QString();
    }
    
    return cleanedUrl;
}

bool DownloadManager::isArchiveFormat(const QString& filePath) const
{
    QString extension = getFileExtension(filePath);
    
    // Supported archive formats
    QStringList archiveExtensions = {
        "zip", "rar", "7z", "tar", "gz", "bz2", "xz", "lzma",
        "tar.gz", "tar.bz2", "tar.xz", "tgz", "tbz2", "txz"
    };
    
    return archiveExtensions.contains(extension);
}

QString DownloadManager::getFileExtension(const QString& filePath) const
{
    QString path = filePath;
    
    // If URL, remove query parameters first
    if (path.contains("?")) {
        path = path.split("?").first();
    }
    
    // Remove fragment part
    if (path.contains("#")) {
        path = path.split("#").first();
    }
    
    // Get file name part
    QString fileName = path.split("/").last();
    
    // Handle special double extensions
    if (fileName.contains(".tar.")) {
        if (fileName.endsWith(".gz")) return "tar.gz";
        if (fileName.endsWith(".bz2")) return "tar.bz2";
        if (fileName.endsWith(".xz")) return "tar.xz";
    }
    
    // Handle common abbreviated extensions
    if (fileName.endsWith(".tgz")) return "tgz";
    if (fileName.endsWith(".tbz2")) return "tbz2";
    if (fileName.endsWith(".txz")) return "txz";
    
    // Get regular extension
    if (!fileName.contains(".")) {
        return QString(); // No extension
    }
    
    return fileName.split(".").last().toLower();
}

QString DownloadManager::detectFileFormat(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    
    // Read file header bytes (magic number)
    QByteArray header = file.read(32);
    file.close();
    
    if (header.isEmpty()) {
        return QString();
    }
    
    // Detect various file format magic numbers
    // ZIP format: 50 4B 03 04 or 50 4B 05 06 or 50 4B 07 08
    if (header.startsWith("\x50\x4B\x03\x04") || 
        header.startsWith("\x50\x4B\x05\x06") ||
        header.startsWith("\x50\x4B\x07\x08")) {
        return "zip";
    }
    
    // RAR format
    if (header.startsWith("Rar!\x1A\x07\x00") || header.startsWith("Rar!\x1A\x07\x01")) {
        return "rar";
    }
    
    // 7Z format
    if (header.startsWith("\x37\x7A\xBC\xAF\x27\x1C")) {
        return "7z";
    }
    
    // PE executable (EXE): MZ
    if (header.startsWith("MZ")) {
        return "exe";
    }
    
    // GZIP format
    if (header.startsWith("\x1F\x8B")) {
        return "gz";
    }
    
    // BZIP2 format
    if (header.startsWith("BZh")) {
        return "bz2";
    }
    
    // TAR format detection
    file.open(QIODevice::ReadOnly);
    if (file.size() > 262) {
        file.seek(257);
        QByteArray tarSignature = file.read(5);
        if (tarSignature == "ustar") {
            file.close();
            return "tar";
        }
    }
    file.close();
    
    return QString();
}

QString DownloadManager::correctFileExtension(const QString& filePath) const
{
    QString detectedFormat = detectFileFormat(filePath);
    if (detectedFormat.isEmpty()) {
        return filePath;
    }
    
    QString currentExtension = getFileExtension(filePath);
    
    if (currentExtension == detectedFormat) {
        return filePath;
    }
    
    // Need to correct the extension
    QFileInfo fileInfo(filePath);
    QString baseName = fileInfo.completeBaseName();
    QString newPath = fileInfo.absolutePath() + "/" + baseName + "." + detectedFormat;
    
    QFile file(filePath);
    if (file.rename(newPath)) {
        return newPath;
    } else {
        return filePath;
    }
} 