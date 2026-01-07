#pragma once

#include <QString>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include "FileSystem.h"

// Forward declaration
enum class Language;

/**
 * @brief 配置管理器类，使用单例模式
 * 
 * 负责管理应用程序的所有配置项，包括下载目录、网络设置等
 */
class ConfigManager {
public:
    // 主题类型枚举
    enum class Theme {
        Light,      // 浅色主题
        Dark,       // 深色主题
        Ocean,      // 海洋主题
        Sunset,     // 日落主题
        Forest,     // 森林主题
        Lavender,   // 薰衣草主题
        Rose,       // 玫瑰主题
        Midnight,   // 午夜主题
        Mocha,      // 摩卡主题
        Default = Light  // 默认使用浅色主题
    };
    
    // 语言类型枚举 (与LanguageManager中的对应)
    enum class Language {
        Chinese,    // 中文
        English,    // 英文
        Japanese,   // 日文
        Default = Chinese  // 默认使用中文
    };
    
    // 获取单例实例
    static ConfigManager& getInstance() {
        static ConfigManager instance;
        return instance;
    }
    
    // 获取下载目录
    QString getDownloadDirectory() const {
        return m_settings->value("downloadDirectory", 
            FileSystem::getInstance().getDownloadDirectory()).toString();
    }
    
    // 设置下载目录
    void setDownloadDirectory(const QString& directory) {
        // 检查目录是否存在，不存在则尝试创建
        if (FileSystem::getInstance().ensureDirectoryExists(directory)) {
            m_settings->setValue("downloadDirectory", directory);
            m_settings->sync();
            qDebug() << "下载目录已设置为:" << directory;
        } else {
            qDebug() << "无法设置下载目录，目录不存在且无法创建:" << directory;
        }
    }
    
    // 是否使用自定义下载路径
    bool getUseCustomDownloadPath() const {
        return m_settings->value("useCustomDownloadPath", false).toBool();
    }
    
    // 设置是否使用自定义下载路径
    void setUseCustomDownloadPath(bool useCustomPath) {
        m_settings->setValue("useCustomDownloadPath", useCustomPath);
        m_settings->sync();
    }
    
    // 获取自定义下载路径
    QString getCustomDownloadPath() const {
        return m_settings->value("customDownloadPath", 
            FileSystem::getInstance().getDownloadDirectory()).toString();
    }
    
    // 设置自定义下载路径
    void setCustomDownloadPath(const QString& path) {
        m_settings->setValue("customDownloadPath", path);
        m_settings->sync();
    }
    
    // 是否使用模拟下载（用于测试）
    bool getUseMockDownload() const {
        return m_settings->value("useMockDownload", false).toBool();
    }
    
    // 设置是否使用模拟下载
    void setUseMockDownload(bool useMock) {
        m_settings->setValue("useMockDownload", useMock);
        m_settings->sync();
    }
    
    // 获取用户代理
    QString getUserAgent() const {
        return m_settings->value("userAgent", 
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36").toString();
    }
    
    // 设置用户代理
    void setUserAgent(const QString& userAgent) {
        m_settings->setValue("userAgent", userAgent);
        m_settings->sync();
    }
    
    // 获取网络超时时间（毫秒）
    int getNetworkTimeout() const {
        return m_settings->value("networkTimeout", 30000).toInt();
    }
    
    // 设置网络超时时间
    void setNetworkTimeout(int timeout) {
        if (timeout >= 1000) { // 至少1秒
            m_settings->setValue("networkTimeout", timeout);
            m_settings->sync();
        }
    }
    
    // 获取是否自动检查更新
    bool getAutoCheckUpdates() const {
        return m_settings->value("autoCheckUpdates", true).toBool();
    }
    
    // 设置是否自动检查更新
    void setAutoCheckUpdates(bool autoCheck) {
        m_settings->setValue("autoCheckUpdates", autoCheck);
        m_settings->sync();
    }

    // 获取当前主题
    Theme getCurrentTheme() const {
        int themeValue = m_settings->value("currentTheme", static_cast<int>(Theme::Default)).toInt();
        return static_cast<Theme>(themeValue);
    }
    
    // 设置当前主题
    void setCurrentTheme(Theme theme) {
        m_settings->setValue("currentTheme", static_cast<int>(theme));
        m_settings->sync();
        qDebug() << "当前主题已设置为:" << static_cast<int>(theme);
    }

    // 获取当前语言
    Language getCurrentLanguage() const {
        int langValue = m_settings->value("currentLanguage", static_cast<int>(Language::Default)).toInt();
        return static_cast<Language>(langValue);
    }
    
    // 设置当前语言
    void setCurrentLanguage(Language language) {
        m_settings->setValue("currentLanguage", static_cast<int>(language));
        m_settings->sync();
        qDebug() << "当前语言已设置为:" << static_cast<int>(language);
    }

    // 重置所有设置为默认值
    void resetToDefaults() {
        m_settings->clear();
        m_settings->sync();
        qDebug() << "所有设置已重置为默认值";
    }

private:
    // 私有构造函数，防止外部创建实例
    ConfigManager() {
        m_settings = new QSettings("DownloadIntegrator", "Settings");
        qDebug() << "配置文件路径:" << m_settings->fileName();
    }
    
    // 私有析构函数
    ~ConfigManager() {
        if (m_settings) {
            delete m_settings;
            m_settings = nullptr;
        }
    }
    
    // 禁用拷贝构造函数和赋值操作符
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    // 设置对象
    QSettings* m_settings;
}; 