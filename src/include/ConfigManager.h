#pragma once

#include <QString>
#include <QSettings>
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

    QString settingsFilePath() const;

    // Get download directory
    QString getDownloadDirectory() const;

    // Set download directory
    void setDownloadDirectory(const QString& directory);

    // Whether to use custom download path
    bool getUseCustomDownloadPath() const;

    // Set whether to use custom download path
    void setUseCustomDownloadPath(bool useCustomPath);

    // Get custom download path
    QString getCustomDownloadPath() const;

    // Set custom download path
    void setCustomDownloadPath(const QString& path);

    // Whether to use mock download (for testing)
    bool getUseMockDownload() const;

    // Set whether to use mock download
    void setUseMockDownload(bool useMock);

    // Get user agent
    QString getUserAgent() const;

    // Set user agent
    void setUserAgent(const QString& userAgent);

    // Get network timeout (milliseconds)
    int getNetworkTimeout() const;

    // Set network timeout
    void setNetworkTimeout(int timeout);

    // Get whether to auto check updates
    bool getAutoCheckUpdates() const;

    // Set whether to auto check updates
    void setAutoCheckUpdates(bool autoCheck);

    // Get current theme
    Theme getCurrentTheme() const;

    // Set current theme
    void setCurrentTheme(Theme theme);

    // Get current language
    Language getCurrentLanguage() const;

    // Set current language
    void setCurrentLanguage(Language language);

    // Reset all settings to defaults
    void resetToDefaults();

private:
    // Private constructor to prevent external instantiation
    ConfigManager();

    // Private destructor
    ~ConfigManager();

    // Disable copy constructor and assignment operator
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // Settings object
    QSettings* m_settings;
};
