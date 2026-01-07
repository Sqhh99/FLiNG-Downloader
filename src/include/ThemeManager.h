#pragma once

#include <QDebug>
#include "ConfigManager.h"

/**
 * @brief 主题管理类
 * 
 * 注意：主题样式已完全迁移到 QML 的 ThemeProvider.qml
 * 此类现在仅负责保存主题设置到配置文件
 */
class ThemeManager {
public:
    // 获取单例实例
    static ThemeManager& getInstance() {
        static ThemeManager instance;
        return instance;
    }
    
    // 切换到指定主题（仅保存设置，QML端会自动响应）
    void switchTheme(ConfigManager::Theme theme) {
        // 保存主题设置
        ConfigManager::getInstance().setCurrentTheme(theme);
        qDebug() << "主题已切换为:" << getThemeName(theme);
    }
    
    // 获取主题名称
    QString getThemeName(ConfigManager::Theme theme) const {
        switch (theme) {
            case ConfigManager::Theme::Light:
                return "浅色主题";
            case ConfigManager::Theme::Dark:
                return "深色主题";
            case ConfigManager::Theme::Ocean:
                return "海洋主题";
            case ConfigManager::Theme::Sunset:
                return "日落主题";
            case ConfigManager::Theme::Forest:
                return "森林主题";
            case ConfigManager::Theme::Lavender:
                return "薰衣草主题";
            case ConfigManager::Theme::Rose:
                return "玫瑰主题";
            case ConfigManager::Theme::Midnight:
                return "午夜主题";
            case ConfigManager::Theme::Mocha:
                return "摩卡主题";
            default:
                return "未知主题";
        }
    }

private:
    // 私有构造函数，防止外部创建实例
    ThemeManager() {}
    
    // 私有析构函数
    ~ThemeManager() {}
    
    // 禁用拷贝构造函数和赋值操作符
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;
}; 