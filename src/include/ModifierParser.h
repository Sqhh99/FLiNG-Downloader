#pragma once

#include <QString>
#include <QList>
#include <QPair>
#include <QStringList>
#include <QPixmap>

// Game modifier information structure
struct ModifierInfo {
    QString name;
    QString gameVersion;
    QString lastUpdate;
    int optionsCount;
    QString description;
    QList<QPair<QString, QString>> versions; // <version identifier, download link>
    QStringList options; // Modifier feature options
    QString url; // Detail page URL for loading detailed information
    QString screenshotUrl; // Modifier screenshot URL for extracting game cover
    QPixmap gameCover; // Extracted game cover
};

class ModifierParser {
public:
    ModifierParser();
    ~ModifierParser();
    
    // Parse modifier list HTML
    static QList<ModifierInfo> parseModifierListHTML(const std::string& html, const QString& searchTerm);
    
    // Parse modifier detail HTML
    static ModifierInfo* parseModifierDetailHTML(const std::string& html, const QString& modifierName);
      // Parse modifier options directly from HTML content
    // Used for parsing options from clipboard content or user-provided HTML snippets
    static QStringList parseOptionsFromHTML(const QString& html);
      // Detect modifier name
    static QString detectGameNameFromHTML(const QString& html);
};