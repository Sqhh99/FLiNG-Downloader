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
 * @brief 文件系统操作工具类，提供文件和目录相关操作
 */
class FileSystem {
public:
    /**
     * @brief 获取单例实例
     * @return FileSystem对象的引用
     */
    static FileSystem& getInstance() {
        static FileSystem instance;
        return instance;
    }
    
    /**
     * @brief 确保目录存在，如果不存在则创建
     * @param path 要确保存在的目录路径
     * @return 如果目录已存在或成功创建则返回true，否则返回false
     */
    bool ensureDirectoryExists(const QString& path) {
        QDir dir(path);
        if (dir.exists()) {
            return true;
        }
        
        bool success = dir.mkpath(path);
        if (success) {
            qDebug() << "创建目录成功:" << path;
        } else {
            qWarning() << "创建目录失败:" << path;
        }
        return success;
    }
    
    /**
     * @brief 检查文件是否存在
     * @param filePath 要检查的文件路径
     * @return 如果文件存在则返回true，否则返回false
     */
    bool fileExists(const QString& filePath) {
        return QFile::exists(filePath);
    }
    
    /**
     * @brief 删除文件
     * @param filePath 要删除的文件路径
     * @return 如果文件删除成功返回true，否则返回false
     */
    bool deleteFile(const QString& filePath) {
        QFile file(filePath);
        if (!file.exists()) {
            qDebug() << "尝试删除不存在的文件:" << filePath;
            return true; // 文件不存在视为删除成功
        }
        
        bool success = file.remove();
        if (success) {
            qDebug() << "文件删除成功:" << filePath;
        } else {
            qWarning() << "文件删除失败:" << filePath << "错误:" << file.errorString();
        }
        return success;
    }
    
    /**
     * @brief 获取文件信息
     * @param filePath 文件路径
     * @return QFileInfo对象，包含文件的详细信息
     */
    QFileInfo getFileInfo(const QString& filePath) {
        return QFileInfo(filePath);
    }
    
    /**
     * @brief 获取文件大小（字节）
     * @param filePath 文件路径
     * @return 文件大小（字节），如果文件不存在则返回0
     */
    qint64 getFileSize(const QString& filePath) {
        QFileInfo fileInfo(filePath);
        if (!fileInfo.exists()) {
            return 0;
        }
        return fileInfo.size();
    }
    
    /**
     * @brief 获取文件修改时间
     * @param filePath 文件路径
     * @return 文件的上次修改时间，如果文件不存在则返回当前时间
     */
    QDateTime getFileModifiedTime(const QString& filePath) {
        QFileInfo fileInfo(filePath);
        if (!fileInfo.exists()) {
            return QDateTime::currentDateTime();
        }
        return fileInfo.lastModified();
    }
    
    /**
     * @brief 获取下载目录
     * @return 系统下载目录的路径
     */
    QString getDownloadDirectory() {
        return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    }
    
    /**
     * @brief 获取应用程序数据目录
     * @return 应用程序数据目录的路径
     */
    QString getAppDataDirectory() {
        QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        ensureDirectoryExists(appDataPath);
        return appDataPath;
    }
    
    /**
     * @brief 获取临时目录
     * @return 系统临时目录的路径
     */
    QString getTempDirectory() {
        return QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    
    /**
     * @brief 清理文件名，移除非法字符
     * @param fileName 原始文件名
     * @return 清理后的文件名，可安全用于创建文件
     */
    QString sanitizeFileName(const QString& fileName) {
        // 复制文件名以进行修改
        QString sanitized = fileName;
        
        // 定义Windows和一般文件系统不允许的字符
        QRegularExpression illegalChars("[<>:\"/\\|?*]");
        
        // 替换非法字符为下划线
        sanitized.replace(illegalChars, "_");
        
        // 替换空格为下划线
        sanitized.replace(" ", "_");
        
        // 替换连续的下划线为单个下划线
        sanitized.replace(QRegularExpression("_{2,}"), "_");
        
        // 移除开头和结尾的点和空格
        sanitized = sanitized.trimmed();
        while (sanitized.startsWith(".") || sanitized.startsWith("_")) {
            sanitized.remove(0, 1);
        }
        while (sanitized.endsWith(".") || sanitized.endsWith("_")) {
            sanitized.chop(1);
        }
        
        // 如果文件名为空，提供默认名称
        if (sanitized.isEmpty()) {
            sanitized = "file";
        }
        
        return sanitized;
    }
    
private:
    // 私有构造函数，防止外部创建实例
    FileSystem() = default;
    
    // 禁用拷贝构造函数和赋值操作符
    FileSystem(const FileSystem&) = delete;
    FileSystem& operator=(const FileSystem&) = delete;
}; 