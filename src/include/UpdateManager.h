#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <functional>
#include "ModifierParser.h"
#include "NetworkManager.h"
#include "FileSystem.h"

// Update progress callback
using UpdateProgressCallback = std::function<void(int, int, bool)>; // current progress, total, has update

/**
 * @brief Update manager class for handling update checking functionality
 */
class UpdateManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Get singleton instance
     * @return Reference to UpdateManager object
     */
    static UpdateManager& getInstance() {
        static UpdateManager instance;
        return instance;
    }

    /**
     * @brief Check update for specified modifier
     * @param url Modifier URL
     * @param currentVersion Current version
     * @param currentGameVersion Current game version
     * @param callback Completion callback
     */
    void checkModifierUpdate(const QString& url, const QString& currentVersion, const QString& currentGameVersion,
                             std::function<void(bool)> callback);

    /**
     * @brief Batch check modifier updates
     * @param modifiers List of modifiers to check
     * @param progressCallback Progress callback
     * @param completedCallback Completion callback, parameter is the number of updates found
     */
    void batchCheckUpdates(const QList<std::tuple<int, QString, QString, QString>>& modifiers, 
                           UpdateProgressCallback progressCallback,
                           std::function<void(int)> completedCallback);

    /**
     * @brief Compare version numbers to determine if update is available
     * @param oldVersion Current version
     * @param newVersion New version
     * @return true if new version is newer than old version
     */
    bool isNewerVersion(const QString& oldVersion, const QString& newVersion);

private:
    // Private constructor
    UpdateManager(QObject* parent = nullptr);
    ~UpdateManager() = default;

    // Disable copy and assignment
    UpdateManager(const UpdateManager&) = delete;
    UpdateManager& operator=(const UpdateManager&) = delete;
}; 