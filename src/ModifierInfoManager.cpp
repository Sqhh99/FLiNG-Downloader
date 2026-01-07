#include "ModifierInfoManager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QDebug>

ModifierInfoManager::ModifierInfoManager(QObject* parent)
    : QObject(parent)
{
}

ModifierInfo ModifierInfoManager::createModifierInfo(const QString& name, 
                                                   const QString& gameVersion,
                                                   const QString& url)
{
    ModifierInfo info;
    info.name = formatModifierName(name);
    info.gameVersion = gameVersion;
    info.url = url;
    info.lastUpdate = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    info.optionsCount = 0;
    
    return info;
}

ModifierInfo* ModifierInfoManager::cloneModifierInfo(const ModifierInfo& other)
{
    ModifierInfo* info = new ModifierInfo();
    info->name = other.name;
    info->gameVersion = other.gameVersion;
    info->url = other.url;
    info->lastUpdate = other.lastUpdate;
    info->optionsCount = other.optionsCount;
    info->options = other.options;
    info->versions = other.versions;
    
    return info;
}

QString ModifierInfoManager::extractNameFromUrl(const QString& url)
{
    // Extract modifier name from URL
    // Try multiple URL formats
    QStringList patterns = {
        "/([^/]+)-trainer/?$",          // Original format: game-name-trainer
        "/trainer/([^/]+)-trainer/?$",  // FLiNG format: /trainer/game-name-trainer
        "/trainer/([^/]+)/?$"           // Simple format: /trainer/game-name
    };
    
    for (const QString& pattern : patterns) {
        QRegularExpression re(pattern);
        QRegularExpressionMatch match = re.match(url);
        
        if (match.hasMatch()) {
            QString name = match.captured(1);
            name.replace('-', ' ');
            name.replace('_', ' ');
            
            // Remove possible "trainer" suffix
            if (name.endsWith(" trainer", Qt::CaseInsensitive)) {
                name.chop(8); // Remove " trainer"
            }
            
            return formatModifierName(name.trimmed());
        }
    }
    
    return QString();
}

QString ModifierInfoManager::formatModifierName(const QString& name)
{
    if (name.isEmpty()) {
        return name;
    }
    
    // Convert to title case (capitalize first letter of each word)
    QStringList words = name.split(' ', Qt::SkipEmptyParts);
    for (QString& word : words) {
        if (!word.isEmpty()) {
            word[0] = word[0].toUpper();
        }
    }
    
    return words.join(' ');
}

QString ModifierInfoManager::formatVersionString(const QString& version)
{
    // Ensure consistent version number format
    // If version only contains digits, add 'v' prefix
    if (QRegularExpression("^\\d+\\.?\\d*$").match(version).hasMatch()) {
        return QString("v%1").arg(version);
    }
    
    // If already has prefix (v, ver., version, etc.), normalize to 'v'
    QString formattedVersion = version;
    QRegularExpression prefixRe("^(v|ver\\.|version|V)\\s*(\\d.*)$");
    QRegularExpressionMatch match = prefixRe.match(version);
    
    if (match.hasMatch()) {
        formattedVersion = QString("v%1").arg(match.captured(2));
    }
    
    return formattedVersion;
}

void ModifierInfoManager::addVersionToModifier(ModifierInfo& info, const QString& version, const QString& url)
{
    // Format version number
    QString formattedVersion = formatVersionString(version);
    
    // Check if version already exists
    for (int i = 0; i < info.versions.size(); ++i) {
        if (info.versions[i].first == formattedVersion) {
            // Update URL
            info.versions[i].second = url;
            return;
        }
    }
    
    // Add new version
    info.versions.append(qMakePair(formattedVersion, url));
}

int ModifierInfoManager::compareModifierSimilarity(const ModifierInfo& a, const ModifierInfo& b)
{
    int similarity = 0;
    
    // Compare name (50%)
    if (a.name.toLower() == b.name.toLower()) {
        similarity += 50;
    } else {
        // Partial match
        QString nameLowerA = a.name.toLower();
        QString nameLowerB = b.name.toLower();
        
        if (nameLowerA.contains(nameLowerB) || nameLowerB.contains(nameLowerA)) {
            similarity += 30;
        } else {
            // Check word-level matching
            QStringList wordsA = nameLowerA.split(' ', Qt::SkipEmptyParts);
            QStringList wordsB = nameLowerB.split(' ', Qt::SkipEmptyParts);
            
            int matchingWords = 0;
            for (const QString& wordA : wordsA) {
                if (wordA.length() < 3) continue; // Skip short words
                
                for (const QString& wordB : wordsB) {
                    if (wordB.length() < 3) continue;
                    
                    if (wordA == wordB) {
                        matchingWords++;
                        break;
                    }
                }
            }
            
            if (!wordsA.isEmpty() && !wordsB.isEmpty()) {
                similarity += 20 * matchingWords / qMax(wordsA.size(), wordsB.size());
            }
        }
    }
    
    // Compare game version (20%)
    if (!a.gameVersion.isEmpty() && !b.gameVersion.isEmpty()) {
        if (a.gameVersion == b.gameVersion) {
            similarity += 20;
        } else if (a.gameVersion.contains(b.gameVersion) || b.gameVersion.contains(a.gameVersion)) {
            similarity += 10;
        }
    }
    
    // Compare URL (30%)
    if (!a.url.isEmpty() && !b.url.isEmpty()) {
        if (a.url == b.url) {
            similarity += 30;
        } else {
            // Extract key parts of URL for comparison
            QString keyPartA = extractNameFromUrl(a.url);
            QString keyPartB = extractNameFromUrl(b.url);
            
            if (!keyPartA.isEmpty() && !keyPartB.isEmpty() && keyPartA == keyPartB) {
                similarity += 20;
            }
        }
    }
    
    return similarity;
}

QString ModifierInfoManager::exportToJson(const ModifierInfo& info)
{
    QJsonObject rootObj;
    rootObj["name"] = info.name;
    rootObj["gameVersion"] = info.gameVersion;
    rootObj["url"] = info.url;
    rootObj["lastUpdate"] = info.lastUpdate;
    rootObj["optionsCount"] = info.optionsCount;
    
    // Options list
    QJsonArray optionsArray;
    for (const QString& option : info.options) {
        optionsArray.append(option);
    }
    rootObj["options"] = optionsArray;
    
    // Versions list
    QJsonArray versionsArray;
    for (const auto& version : info.versions) {
        QJsonObject versionObj;
        versionObj["version"] = version.first;
        versionObj["url"] = version.second;
        versionsArray.append(versionObj);
    }
    rootObj["versions"] = versionsArray;
    
    QJsonDocument doc(rootObj);
    return doc.toJson(QJsonDocument::Indented);
}

ModifierInfo ModifierInfoManager::importFromJson(const QString& json)
{
    ModifierInfo info;
    
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "Invalid JSON format:" << json;
        return info;
    }
    
    QJsonObject rootObj = doc.object();
    
    // Basic info
    info.name = rootObj["name"].toString();
    info.gameVersion = rootObj["gameVersion"].toString();
    info.url = rootObj["url"].toString();
    info.lastUpdate = rootObj["lastUpdate"].toString();
    info.optionsCount = rootObj["optionsCount"].toInt();
    
    // Options list
    QJsonArray optionsArray = rootObj["options"].toArray();
    for (const QJsonValue& value : optionsArray) {
        info.options.append(value.toString());
    }
    
    // Versions list
    QJsonArray versionsArray = rootObj["versions"].toArray();
    for (const QJsonValue& value : versionsArray) {
        QJsonObject versionObj = value.toObject();
        info.versions.append(qMakePair(
            versionObj["version"].toString(),
            versionObj["url"].toString()
        ));
    }
    
    return info;
}

QStringList ModifierInfoManager::convertHtmlOptionsToPlainText(const QString& htmlOptions)
{
    QStringList result;
    
    // Use regex to remove HTML tags
    QRegularExpression htmlTagRe("<.*?>");
    QString plainText = htmlOptions;
    plainText.replace(htmlTagRe, "");
    
    // Handle HTML entities
    plainText.replace("&nbsp;", " ");
    plainText.replace("&amp;", "&");
    plainText.replace("&lt;", "<");
    plainText.replace("&gt;", ">");
    plainText.replace("&quot;", "\"");
    
    // Split into lines
    QStringList lines = plainText.split('\n', Qt::SkipEmptyParts);
    
    // Clean up each line
    for (QString& line : lines) {
        line = line.trimmed();
        if (!line.isEmpty()) {
            result.append(line);
        }
    }
    
    return result;
}

QList<ModifierInfo> ModifierInfoManager::searchModifiersByKeyword(const QList<ModifierInfo>& modifiers, const QString& keyword)
{
    QList<ModifierInfo> results;
    
    if (keyword.isEmpty()) {
        return modifiers; // If keyword is empty, return all modifiers
    }
    
    QString lowerKeyword = keyword.toLower();
    
    for (const ModifierInfo& info : modifiers) {
        // Search in name
        if (info.name.toLower().contains(lowerKeyword)) {
            results.append(info);
            continue;
        }
        
        // Search in game version
        if (info.gameVersion.toLower().contains(lowerKeyword)) {
            results.append(info);
            continue;
        }
        
        // Search in options
        bool foundInOptions = false;
        for (const QString& option : info.options) {
            if (option.toLower().contains(lowerKeyword)) {
                foundInOptions = true;
                break;
            }
        }
        
        if (foundInOptions) {
            results.append(info);
        }
    }
    
    return results;
} 