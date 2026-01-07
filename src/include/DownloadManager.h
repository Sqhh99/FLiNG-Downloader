#pragma once

#include <QObject>
#include <QString>
#include <functional>
#include "ModifierParser.h"
#include "NetworkManager.h"
#include "FileSystem.h"

// Callback function types - using different names to avoid conflicts
using DLProgressCallback = std::function<void(int)>; // Download progress callback (percentage)
using DLCompletedCallback = std::function<void(bool, const QString&, const QString&)>; // Download completed callback (success status, error message, actual file path)

/**
 * @brief Download manager class for handling file download functionality
 */
class DownloadManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Get singleton instance
     * @return Reference to DownloadManager object
     */
    static DownloadManager& getInstance() {
        static DownloadManager instance;
        return instance;
    }
    
    /**
     * @brief Download file
     * @param url Download URL
     * @param savePath Save path
     * @param progressCallback Progress callback
     * @param completedCallback Completion callback
     */
    void downloadFile(const QString& url, 
                     const QString& savePath,
                     DLProgressCallback progressCallback,
                     DLCompletedCallback completedCallback);    /**
     * @brief Download modifier
     * @param modifier Modifier information
     * @param version Selected version
     * @param savePath Save path
     * @param completedCallback Completion callback (success, error message, actual file path, modifier info, is archive)
     * @param progressCallback Progress callback
     */
    void downloadModifier(const ModifierInfo& modifier,
                         const QString& version,
                         const QString& savePath,
                         std::function<void(bool, const QString&, const QString&, const ModifierInfo&, bool)> completedCallback,
                         DLProgressCallback progressCallback);
    
    /**
     * @brief Cancel current download
     */
    void cancelDownload();
    
    /**
     * @brief Get download status
     * @return Whether a download is in progress
     */
    bool isDownloading() const;
      /**
     * @brief Clean invalid URL, remove trailing commas etc.
     * @param url Original URL
     * @return Cleaned URL
     */
    QString cleanUrl(const QString& url) const;
    
    /**
     * @brief Detect if file is an archive format
     * @param filePath File path or URL
     * @return Whether it is an archive format
     */
    bool isArchiveFormat(const QString& filePath) const;
      /**
     * @brief Get file extension
     * @param filePath File path or URL
     * @return File extension (lowercase)
     */
    QString getFileExtension(const QString& filePath) const;
    
    /**
     * @brief Detect actual file format (via file magic number)
     * @param filePath File path
     * @return Detected file extension, or empty string if cannot detect
     */
    QString detectFileFormat(const QString& filePath) const;
    
    /**
     * @brief Correct file extension (if detected format differs from current extension)
     * @param filePath File path
     * @return Corrected file path, or original path if no correction needed
     */
    QString correctFileExtension(const QString& filePath) const;

private:
    // Private constructor
    DownloadManager(QObject* parent = nullptr);
    ~DownloadManager() = default;
    
    // Disable copy and assignment
    DownloadManager(const DownloadManager&) = delete;
    DownloadManager& operator=(const DownloadManager&) = delete;
    
private:
    bool m_isDownloading;
}; 