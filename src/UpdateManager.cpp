#include "UpdateManager.h"
#include "ModifierParser.h"
#include <QDebug>
#include <QRegularExpression>

UpdateManager::UpdateManager(QObject* parent)
    : QObject(parent)
{
}

void UpdateManager::checkModifierUpdate(const QString& url, const QString& currentVersion, 
                                       const QString& currentGameVersion,
                                       std::function<void(bool)> callback)
{
    if (url.isEmpty()) {
        qDebug() << "Update check URL is empty";
        if (callback) {
            callback(false);
        }
        return;
    }

    // Use NetworkManager to get modifier details page
    NetworkManager::getInstance().sendGetRequest(
        url,
        [this, currentVersion, currentGameVersion, callback](const QByteArray& data, bool success) {
            if (success) {
                // Parse page content
                ModifierInfo* modifier = ModifierParser::parseModifierDetailHTML(data.toStdString(), "");
                
                if (modifier) {
                    // Compare game version
                    bool hasUpdate = false;
                    
                    // If game version differs, consider it updated
                    if (modifier->gameVersion != currentGameVersion) {
                        hasUpdate = true;
                    }
                    
                    // Compare modifier version - if version exists, compare versions
                    if (!modifier->versions.isEmpty() && !currentVersion.isEmpty()) {
                        QString latestVersion = modifier->versions.first().first;
                        if (isNewerVersion(currentVersion, latestVersion)) {
                            hasUpdate = true;
                        }
                    }
                    
                    delete modifier;
                    
                    if (callback) {
                        callback(hasUpdate);
                    }
                } else {
                    qDebug() << "Failed to parse modifier details";
                    if (callback) {
                        callback(false);
                    }
                }
            } else {
                qDebug() << "Failed to fetch modifier details page";
                if (callback) {
                    callback(false);
                }
            }
        }
    );
}

void UpdateManager::batchCheckUpdates(const QList<std::tuple<int, QString, QString, QString>>& modifiers,
                                      UpdateProgressCallback progressCallback,
                                      std::function<void(int)> completedCallback)
{
    // Check if there are modifiers to update
    if (modifiers.isEmpty()) {
        if (completedCallback) {
            completedCallback(0);
        }
        return;
    }
    
    // Create counters to track completed checks
    int* completedChecks = new int(0);
    int* updatesFound = new int(0);
    QList<int>* updatedIndices = new QList<int>();
    
    // Check each modifier
    for (const auto& modifierInfo : modifiers) {
        int index = std::get<0>(modifierInfo);
        const QString& url = std::get<1>(modifierInfo);
        const QString& version = std::get<2>(modifierInfo);
        const QString& gameVersion = std::get<3>(modifierInfo);
        
        // Check if URL is valid
        if (url.isEmpty()) {
            (*completedChecks)++;
            
            // Update progress
            if (progressCallback) {
                progressCallback(*completedChecks, modifiers.size(), !updatedIndices->isEmpty());
            }
            
            // Check if all complete
            if (*completedChecks >= modifiers.size()) {
                if (completedCallback) {
                    completedCallback(*updatesFound);
                }
                
                // Clean up memory
                delete completedChecks;
                delete updatesFound;
                delete updatedIndices;
            }
            
            continue;
        }
        
        // Check for update
        checkModifierUpdate(
            url, version, gameVersion,
            [this, index, completedChecks, updatesFound, updatedIndices, modifiers, progressCallback, completedCallback](bool hasUpdate) {
                // Update counters
                if (hasUpdate) {
                    (*updatesFound)++;
                    updatedIndices->append(index);
                }
                
                (*completedChecks)++;
                
                // Update progress
                if (progressCallback) {
                    progressCallback(*completedChecks, modifiers.size(), !updatedIndices->isEmpty());
                }
                
                // Check if all complete
                if (*completedChecks >= modifiers.size()) {
                    if (completedCallback) {
                        completedCallback(*updatesFound);
                    }
                    
                    // Clean up memory
                    delete completedChecks;
                    delete updatesFound;
                    delete updatedIndices;
                }
            }
        );
    }
}

bool UpdateManager::isNewerVersion(const QString& oldVersion, const QString& newVersion)
{
    // If versions are the same, no update
    if (oldVersion == newVersion) {
        return false;
    }
    
    // If either version is empty, consider it updated
    if (oldVersion.isEmpty() || newVersion.isEmpty()) {
        return true;
    }
    
    // Extract numeric parts from version strings
    QRegularExpression re("(\\d+)");
    QRegularExpressionMatchIterator oldMatches = re.globalMatch(oldVersion);
    QRegularExpressionMatchIterator newMatches = re.globalMatch(newVersion);
    
    // Compare each numeric part
    while (oldMatches.hasNext() && newMatches.hasNext()) {
        QRegularExpressionMatch oldMatch = oldMatches.next();
        QRegularExpressionMatch newMatch = newMatches.next();
        
        int oldNum = oldMatch.captured(1).toInt();
        int newNum = newMatch.captured(1).toInt();
        
        if (newNum > oldNum) {
            return true;
        } else if (newNum < oldNum) {
            return false;
        }
    }
    
    // If new version has more numeric parts, consider it newer
    if (newMatches.hasNext()) {
        return true;
    }
    
    // Default to no update
    return false;
} 