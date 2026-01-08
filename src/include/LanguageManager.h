#pragma once

#include <QGuiApplication>
#include <QTranslator>
#include <QLocale>
#include <QDebug>
#include <QList>
#include "ConfigManager.h"

/**
 * @brief Language manager class responsible for switching and applying different languages
 */
class LanguageManager {
public:
    // Language type enumeration
    enum class Language {
        Chinese,    // Chinese
        English,    // English
        Japanese,   // Japanese
        Default = Chinese  // Default to Chinese
    };
    
    // Get singleton instance
    static LanguageManager& getInstance() {
        static LanguageManager instance;
        return instance;
    }
    
    // Apply current language (read from configuration)
    void applyCurrentLanguage(QGuiApplication& app) {
        ConfigManager::Language configLang = ConfigManager::getInstance().getCurrentLanguage();
        Language language = convertConfigLanguage(configLang);
        applyLanguage(app, language);
    }
    
    // Switch to specified language
    void switchLanguage(QGuiApplication& app, Language language) {
        // Save language setting
        ConfigManager::Language configLang = convertToConfigLanguage(language);
        ConfigManager::getInstance().setCurrentLanguage(configLang);
        
        // Apply language
        applyLanguage(app, language);
    }
    
    // Get language name
    QString getLanguageName(Language language) const {
        switch (language) {
            case Language::Chinese:
                return "中文";
            case Language::English:
                return "English";
            case Language::Japanese:
                return "日本語";
            default:
                return "Unknown";
        }
    }
    
    // Get language locale code
    QString getLanguageLocale(Language language) const {
        switch (language) {
            case Language::Chinese:
                return "zh_CN";
            case Language::English:
                return "en_US";
            case Language::Japanese:
                return "ja_JP";
            default:
                return "zh_CN";
        }
    }
    
    // Get current installed translator
    QTranslator* getCurrentTranslator() const {
        return m_currentTranslator;
    }

private:
    // Private constructor, prevent external instantiation
    LanguageManager() : m_currentTranslator(nullptr) {}
    
    // Private destructor
    ~LanguageManager() {
        if (m_currentTranslator) {
            delete m_currentTranslator;
            m_currentTranslator = nullptr;
        }
    }
    
    // Disable copy constructor and assignment operator
    LanguageManager(const LanguageManager&) = delete;
    LanguageManager& operator=(const LanguageManager&) = delete;
    
    // Convert ConfigManager::Language to LanguageManager::Language
    Language convertConfigLanguage(ConfigManager::Language configLang) const {
        switch (configLang) {
            case ConfigManager::Language::Chinese:
                return Language::Chinese;
            case ConfigManager::Language::English:
                return Language::English;
            case ConfigManager::Language::Japanese:
                return Language::Japanese;
            default:
                return Language::Chinese;
        }
    }
    
    // Convert LanguageManager::Language to ConfigManager::Language
    ConfigManager::Language convertToConfigLanguage(Language language) const {
        switch (language) {
            case Language::Chinese:
                return ConfigManager::Language::Chinese;
            case Language::English:
                return ConfigManager::Language::English;
            case Language::Japanese:
                return ConfigManager::Language::Japanese;
            default:
                return ConfigManager::Language::Chinese;
        }
    }
    
    // Apply specified language
    void applyLanguage(QGuiApplication& app, Language language) {
        // Unload current translator
        if (m_currentTranslator) {
            app.removeTranslator(m_currentTranslator);
            delete m_currentTranslator;
            m_currentTranslator = nullptr;
        }
        
        // If default language (Chinese), no need to load translation
        if (language == Language::Chinese) {
            qDebug() << "Applied default language: Chinese";
            return;
        }
        
        // Create new translator
        m_currentTranslator = new QTranslator();
        
        // Load translation file for the language
        QString locale = getLanguageLocale(language);
        bool loaded = m_currentTranslator->load(QString(":/translations/flingdownloader_%1").arg(locale));
        
        if (loaded) {
            // Install translator
            app.installTranslator(m_currentTranslator);
            qDebug() << "Applied language:" << getLanguageName(language);
        } else {
            qDebug() << "Cannot load language file:" << locale;
            
            // If cannot load translation file, fallback to Chinese
            if (language != Language::Chinese) {
                qDebug() << "Falling back to Chinese";
                
                // Clean up previously created translator
                delete m_currentTranslator;
                m_currentTranslator = nullptr;
                
                // Update config
                ConfigManager::getInstance().setCurrentLanguage(ConfigManager::Language::Chinese);
            }
        }
    }
    
private:
    QTranslator* m_currentTranslator;  // Current translator in use
}; 