#pragma once

#include <QString>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDebug>
#include <QDateTime>
#include <QRegularExpression>

/**
 * @brief File system utility class providing file and directory operations
 */
class FileSystem {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to FileSystem object
     */
    static FileSystem& getInstance() {
        static FileSystem instance;
        return instance;
    }
    
    /**
     * @brief Ensure directory exists, create if not
     * @param path Directory path to ensure exists
     * @return true if directory exists or was created successfully, false otherwise
     */
    bool ensureDirectoryExists(const QString& path) {
        QDir dir(path);
        if (dir.exists()) {
            return true;
        }
        
        bool success = dir.mkpath(path);
        if (success) {
            qDebug() << "Directory created:" << path;
        } else {
            qWarning() << "Failed to create directory:" << path;
        }
        return success;
    }
    
    /**
     * @brief Check if file exists
     * @param filePath Path to check
     * @return true if file exists, false otherwise
     */
    bool fileExists(const QString& filePath) {
        return QFile::exists(filePath);
    }
    
    /**
     * @brief Delete file
     * @param filePath Path to the file to delete
     * @return true if file was deleted successfully, false otherwise
     */
    bool deleteFile(const QString& filePath) {
        QFile file(filePath);
        if (!file.exists()) {
            qDebug() << "Attempting to delete non-existent file:" << filePath;
            return true; // Non-existent file is considered deleted
        }
        
        bool success = file.remove();
        if (success) {
            qDebug() << "File deleted:" << filePath;
        } else {
            qWarning() << "Failed to delete file:" << filePath << "Error:" << file.errorString();
        }
        return success;
    }
    
    /**
     * @brief Get file information
     * @param filePath File path
     * @return QFileInfo object containing file details
     */
    QFileInfo getFileInfo(const QString& filePath) {
        return QFileInfo(filePath);
    }
    
    /**
     * @brief Get file size (bytes)
     * @param filePath File path
     * @return File size (bytes), returns 0 if file does not exist
     */
    qint64 getFileSize(const QString& filePath) {
        QFileInfo fileInfo(filePath);
        if (!fileInfo.exists()) {
            return 0;
        }
        return fileInfo.size();
    }
    
    /**
     * @brief Get file modification time
     * @param filePath File path
     * @return Last modification time of the file, returns current time if file does not exist
     */
    QDateTime getFileModifiedTime(const QString& filePath) {
        QFileInfo fileInfo(filePath);
        if (!fileInfo.exists()) {
            return QDateTime::currentDateTime();
        }
        return fileInfo.lastModified();
    }
    
    /**
     * @brief Get download directory
     * @return Path to the system download directory
     */
    QString getDownloadDirectory() {
        return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    }
    
    /**
     * @brief Get application data directory
     * @return Path to the application data directory
     */
    QString getAppDataDirectory() {
        QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        ensureDirectoryExists(appDataPath);
        return appDataPath;
    }
    
    /**
     * @brief Get temporary directory
     * @return Path to the system temporary directory
     */
    QString getTempDirectory() {
        return QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    
    /**
     * @brief Sanitize filename by removing illegal characters
     * @param fileName Original filename
     * @return Sanitized filename safe for file creation
     */
    QString sanitizeFileName(const QString& fileName) {
        // Copy file name for modification
        QString sanitized = fileName;
        
        // Define characters not allowed in Windows and general file systems
        QRegularExpression illegalChars("[<>:\"/\\|?*]");
        
        // Replace illegal characters with underscores
        sanitized.replace(illegalChars, "_");
        
        // Replace spaces with underscores
        sanitized.replace(" ", "_");
        
        // Replace consecutive underscores with a single underscore
        sanitized.replace(QRegularExpression("_{2,}"), "_");
        
        // Remove leading and trailing dots and spaces
        sanitized = sanitized.trimmed();
        while (sanitized.startsWith(".") || sanitized.startsWith("_")) {
            sanitized.remove(0, 1);
        }
        while (sanitized.endsWith(".") || sanitized.endsWith("_")) {
            sanitized.chop(1);
        }
        
        // If file name is empty, provide a default name
        if (sanitized.isEmpty()) {
            sanitized = "file";
        }
        
        return sanitized;
    }
    
private:
    // Private constructor to prevent external instantiation
    FileSystem() = default;
    
    // Disable copy constructor and assignment operator
    FileSystem(const FileSystem&) = delete;
    FileSystem& operator=(const FileSystem&) = delete;
}; 