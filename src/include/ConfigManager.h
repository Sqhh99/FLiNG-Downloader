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
 * @brief Configuration manager class using singleton pattern
 * 
 * Manages all application configuration including download directory, network settings, etc.
 */
class ConfigManager {
public:
    // Theme type enum
    enum class Theme {
        Light,      // Light theme
        Dark,       // Dark theme
        Ocean,      // Ocean theme
        Sunset,     // Sunset theme
        Forest,     // Forest theme
        Lavender,   // Lavender theme
        Rose,       // Rose theme
        Midnight,   // Midnight theme
        Mocha,      // Mocha theme
        Default = Light  // Default to light theme
    };
    
    // Language type enum (matches LanguageManager)
    enum class Language {
        Chinese,    // Chinese
        English,    // English
        Japanese,   // Japanese
        Default = Chinese  // Default to Chinese
    };
    
    // Get singleton instance
    static ConfigManager& getInstance() {
        static ConfigManager instance;
        return instance;
    }
    
    // Get download directory
    QString getDownloadDirectory() const {
        return m_settings->value("downloadDirectory", 
            FileSystem::getInstance().getDownloadDirectory()).toString();
    }
    
    // Set download directory
    void setDownloadDirectory(const QString& directory) {
        // Check if directory exists, create if not
        if (FileSystem::getInstance().ensureDirectoryExists(directory)) {
            m_settings->setValue("downloadDirectory", directory);
            m_settings->sync();
            qDebug() << "Download directory set to:" << directory;
        } else {
            qDebug() << "Cannot set download directory, path invalid:" << directory;
        }
    }
    
    // Whether to use custom download path
    bool getUseCustomDownloadPath() const {
        return m_settings->value("useCustomDownloadPath", false).toBool();
    }
    
    // Set whether to use custom download path
    void setUseCustomDownloadPath(bool useCustomPath) {
        m_settings->setValue("useCustomDownloadPath", useCustomPath);
        m_settings->sync();
    }
    
    // Get custom download path
    QString getCustomDownloadPath() const {
        return m_settings->value("customDownloadPath", 
            FileSystem::getInstance().getDownloadDirectory()).toString();
    }
    
    // Set custom download path
    void setCustomDownloadPath(const QString& path) {
        m_settings->setValue("customDownloadPath", path);
        m_settings->sync();
    }
    
    // Whether to use mock download (for testing)
    bool getUseMockDownload() const {
        return m_settings->value("useMockDownload", false).toBool();
    }
    
    // Set whether to use mock download
    void setUseMockDownload(bool useMock) {
        m_settings->setValue("useMockDownload", useMock);
        m_settings->sync();
    }
    
    // Get user agent
    QString getUserAgent() const {
        return m_settings->value("userAgent", 
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36").toString();
    }
    
    // Set user agent
    void setUserAgent(const QString& userAgent) {
        m_settings->setValue("userAgent", userAgent);
        m_settings->sync();
    }
    
    // Get network timeout (milliseconds)
    int getNetworkTimeout() const {
        return m_settings->value("networkTimeout", 30000).toInt();
    }
    
    // Set network timeout
    void setNetworkTimeout(int timeout) {
        if (timeout >= 1000) { // Minimum 1 second
            m_settings->setValue("networkTimeout", timeout);
            m_settings->sync();
        }
    }
    
    // Get whether to auto check updates
    bool getAutoCheckUpdates() const {
        return m_settings->value("autoCheckUpdates", true).toBool();
    }
    
    // Set whether to auto check updates
    void setAutoCheckUpdates(bool autoCheck) {
        m_settings->setValue("autoCheckUpdates", autoCheck);
        m_settings->sync();
    }

    // Get current theme
    Theme getCurrentTheme() const {
        int themeValue = m_settings->value("currentTheme", static_cast<int>(Theme::Default)).toInt();
        return static_cast<Theme>(themeValue);
    }
    
    // Set current theme
    void setCurrentTheme(Theme theme) {
        m_settings->setValue("currentTheme", static_cast<int>(theme));
        m_settings->sync();
        qDebug() << "Theme set to:" << static_cast<int>(theme);
    }

    // Get current language
    Language getCurrentLanguage() const {
        int langValue = m_settings->value("currentLanguage", static_cast<int>(Language::Default)).toInt();
        return static_cast<Language>(langValue);
    }
    
    // Set current language
    void setCurrentLanguage(Language language) {
        m_settings->setValue("currentLanguage", static_cast<int>(language));
        m_settings->sync();
        qDebug() << "Language set to:" << static_cast<int>(language);
    }

    // Reset all settings to defaults
    void resetToDefaults() {
        m_settings->clear();
        m_settings->sync();
        qDebug() << "Settings reset to defaults";
    }

private:
    // Private constructor to prevent external instantiation
    ConfigManager() {
        m_settings = new QSettings("DownloadIntegrator", "Settings");
        qDebug() << "Config file path:" << m_settings->fileName();
    }
    
    // Private destructor
    ~ConfigManager() {
        if (m_settings) {
            delete m_settings;
            m_settings = nullptr;
        }
    }
    
    // Disable copy constructor and assignment operator
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    // Settings object
    QSettings* m_settings;
}; 