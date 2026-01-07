#pragma once

#include <QString>
#include <QList>
#include <QPair>
#include <QStringList>
#include <QPixmap>

// 游戏修改器信息结构
struct ModifierInfo {
    QString name;
    QString gameVersion;
    QString lastUpdate;
    int optionsCount;
    QString description;
    QList<QPair<QString, QString>> versions; // <版本标识, 下载链接>
    QStringList options; // 修改器提供的功能选项
    QString url; // 详情页URL，用于加载详细信息
    QString screenshotUrl; // 修改器截图URL，用于提取游戏封面
    QPixmap gameCover; // 提取的游戏封面
};

class ModifierParser {
public:
    ModifierParser();
    ~ModifierParser();
    
    // 解析修改器列表HTML
    static QList<ModifierInfo> parseModifierListHTML(const std::string& html, const QString& searchTerm);
    
    // 解析修改器详情HTML
    static ModifierInfo* parseModifierDetailHTML(const std::string& html, const QString& modifierName);
      // 直接从HTML内容解析修改器选项
    // 用于从剪贴板内容或用户提供的HTML片段解析选项
    static QStringList parseOptionsFromHTML(const QString& html);
      // 检测修改器名称
    static QString detectGameNameFromHTML(const QString& html);
};