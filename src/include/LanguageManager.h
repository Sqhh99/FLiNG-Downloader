#pragma once

#include <QGuiApplication>
#include <QTranslator>
#include <QLocale>
#include <QDebug>
#include <QList>
#include "ConfigManager.h"

/**
 * @brief 语言管理类，负责切换和应用不同的语言
 */
class LanguageManager {
public:
    // 语言类型枚举
    enum class Language {
        Chinese,    // 中文
        English,    // 英文
        Japanese,   // 日文
        Default = Chinese  // 默认使用中文
    };
    
    // 获取单例实例
    static LanguageManager& getInstance() {
        static LanguageManager instance;
        return instance;
    }
    
    // 应用当前语言（从配置中读取）
    void applyCurrentLanguage(QGuiApplication& app) {
        ConfigManager::Language configLang = ConfigManager::getInstance().getCurrentLanguage();
        Language language = convertConfigLanguage(configLang);
        applyLanguage(app, language);
    }
    
    // 切换到指定语言
    void switchLanguage(QGuiApplication& app, Language language) {
        // 保存语言设置
        ConfigManager::Language configLang = convertToConfigLanguage(language);
        ConfigManager::getInstance().setCurrentLanguage(configLang);
        
        // 应用语言
        applyLanguage(app, language);
    }
    
    // 获取语言名称
    QString getLanguageName(Language language) const {
        switch (language) {
            case Language::Chinese:
                return "中文";
            case Language::English:
                return "English";
            case Language::Japanese:
                return "日本語";
            default:
                return "未知语言";
        }
    }
    
    // 获取语言区域码
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
    
    // 获取当前已安装的翻译器
    QTranslator* getCurrentTranslator() const {
        return m_currentTranslator;
    }

private:
    // 私有构造函数，防止外部创建实例
    LanguageManager() : m_currentTranslator(nullptr) {}
    
    // 私有析构函数
    ~LanguageManager() {
        if (m_currentTranslator) {
            delete m_currentTranslator;
            m_currentTranslator = nullptr;
        }
    }
    
    // 禁用拷贝构造函数和赋值操作符
    LanguageManager(const LanguageManager&) = delete;
    LanguageManager& operator=(const LanguageManager&) = delete;
    
    // 将ConfigManager::Language转换为LanguageManager::Language
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
    
    // 将LanguageManager::Language转换为ConfigManager::Language
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
    
    // 应用指定语言
    void applyLanguage(QGuiApplication& app, Language language) {
        // 卸载当前翻译器
        if (m_currentTranslator) {
            app.removeTranslator(m_currentTranslator);
            delete m_currentTranslator;
            m_currentTranslator = nullptr;
        }
        
        // 如果是默认语言（中文），不需要加载翻译
        if (language == Language::Chinese) {
            qDebug() << "已应用默认语言：中文";
            return;
        }
        
        // 创建新的翻译器
        m_currentTranslator = new QTranslator();
        
        // 根据语言加载对应的翻译文件
        QString locale = getLanguageLocale(language);
        bool loaded = m_currentTranslator->load(QString(":/translations/downloadintegrator_%1").arg(locale));
        
        if (loaded) {
            // 安装翻译器
            app.installTranslator(m_currentTranslator);
            qDebug() << "已应用" << getLanguageName(language) << "语言";
        } else {
            qDebug() << "无法加载语言文件:" << locale;
            
            // 如果无法加载翻译文件，回退到中文
            if (language != Language::Chinese) {
                qDebug() << "回退到中文";
                
                // 清理之前创建的翻译器
                delete m_currentTranslator;
                m_currentTranslator = nullptr;
                
                // 更新配置
                ConfigManager::getInstance().setCurrentLanguage(ConfigManager::Language::Chinese);
            }
        }
    }
    
private:
    QTranslator* m_currentTranslator;  // 当前使用的翻译器
}; 