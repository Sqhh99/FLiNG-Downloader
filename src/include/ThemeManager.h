#pragma once

#include <QDebug>
#include "ConfigManager.h"

/**
 * @brief Theme manager class
 * 
 * Note: Theme styles have been fully migrated to QML's ThemeProvider.qml
 * This class is now only responsible for saving theme settings to config
 */
class ThemeManager {
public:
    // Get singleton instance
    static ThemeManager& getInstance() {
        static ThemeManager instance;
        return instance;
    }
    
    // Switch to specified theme (only saves settings, QML responds automatically)
    void switchTheme(ConfigManager::Theme theme) {
        // Save theme setting
        ConfigManager::getInstance().setCurrentTheme(theme);
        qDebug() << "Theme switched to:" << getThemeName(theme);
    }
    
    // Get theme name
    QString getThemeName(ConfigManager::Theme theme) const {
        switch (theme) {
            case ConfigManager::Theme::Light:
                return "Light";
            case ConfigManager::Theme::Dark:
                return "Dark";
            case ConfigManager::Theme::Ocean:
                return "Ocean";
            case ConfigManager::Theme::Sunset:
                return "Sunset";
            case ConfigManager::Theme::Forest:
                return "Forest";
            case ConfigManager::Theme::Lavender:
                return "Lavender";
            case ConfigManager::Theme::Rose:
                return "Rose";
            case ConfigManager::Theme::Midnight:
                return "Midnight";
            case ConfigManager::Theme::Mocha:
                return "Mocha";
            default:
                return "Unknown";
        }
    }

private:
    // Private constructor to prevent external instantiation
    ThemeManager() {}
    
    // Private destructor
    ~ThemeManager() {}
    
    // Disable copy constructor and assignment operator
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;
}; 