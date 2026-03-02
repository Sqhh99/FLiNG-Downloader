#include "SearchManager.h"
#include <QSettings>
#include <QDateTime>
#include <QRegularExpression>
#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include <QMap>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <algorithm>
#include <memory>
#include "NetworkManager.h"
#include "ModifierParser.h"
#include "ModifierManager.h"
#include "ConfigManager.h"
#include "FileSystem.h"

// Constructor
SearchManager::SearchManager(QObject* parent)
    : QObject(parent),
      m_maxHistoryItems(20)
{
    // Load search history
    loadSearchHistory();
    
    // If search history is empty, initialize with common game names as suggestions
    if (m_searchHistory.isEmpty()) {
        initializeDefaultSuggestions();
    }
}

// Initialize search suggestions from game mappings
void SearchManager::initializeDefaultSuggestions()
{
    // Add Chinese game names from the mapping database
    GameMappingManager& mappingManager = GameMappingManager::getInstance();
    if (mappingManager.initialize()) {
        QStringList chineseNames = mappingManager.getAllChineseNames();
        for (const QString& name : chineseNames) {
            if (!m_searchHistory.contains(name)) {
                m_searchHistory.append(name);
            }
        }
    }
    
    saveSearchHistory();
}

// Execute modifier search
void SearchManager::searchModifiers(const QString& searchTerm, 
                                   std::function<void(const QList<ModifierInfo>&)> callback)
{
    if (!searchTerm.isEmpty()) {
        addSearchToHistory(searchTerm);
    }
    
    if (searchTerm.isEmpty()) {
        loadFeaturedModifiers(callback);
        return;
    }
    
    // Check for Chinese characters and translate if needed
    GameMappingManager& mappingManager = GameMappingManager::getInstance();
    if (mappingManager.containsChinese(searchTerm)) {
        QString englishTerm = mappingManager.translateToEnglish(searchTerm);
        
        if (!englishTerm.isEmpty() && englishTerm != searchTerm) {
            performSearch(englishTerm, callback);
        } else {
            // Use async translation
            mappingManager.translateToEnglishAsync(searchTerm, [this, searchTerm, callback](const QString& translatedTerm) {
                performSearch(!translatedTerm.isEmpty() ? translatedTerm : searchTerm, callback);
            });
            return;
        }
    } else {
        performSearch(searchTerm, callback);
    }
}

// Execute actual search operation
void SearchManager::performSearch(const QString& searchTerm, 
                                 std::function<void(const QList<ModifierInfo>&)> callback)
{
    // Build URL with proper encoding
    QString searchTermEncoded = searchTerm;
    searchTermEncoded.replace(" ", "+");
    
    QString url = "https://flingtrainer.com/?s=" + searchTermEncoded;
    
    NetworkManager::getInstance().sendGetRequest(
        url,
        [this, searchTerm, callback](const QByteArray& data, bool success) {
            QList<ModifierInfo> modifierList;
            
            if (success) {
                modifierList = ModifierParser::parseModifierListHTML(data.toStdString(), searchTerm);
                
                // Format modifier names
                ModifierInfoManager& infoManager = ModifierInfoManager::getInstance();
                for (int i = 0; i < modifierList.size(); ++i) {
                    modifierList[i].name = infoManager.formatModifierName(modifierList[i].name);
                }
                
                // Sort by relevance
                if (!searchTerm.isEmpty()) {
                    modifierList = sortByRelevance(modifierList, searchTerm);
                }
                
                // Check if any modifier is missing options count or game version
                // If so, enrich them from detail pages
                bool needsEnrichment = false;
                for (const auto& mod : modifierList) {
                    if (mod.optionsCount == 0 || mod.gameVersion.isEmpty()) {
                        needsEnrichment = true;
                        break;
                    }
                }
                
                if (needsEnrichment && !modifierList.isEmpty()) {
                    qDebug() << "SearchManager: Search results missing data, fetching from detail pages...";
                    enrichSearchResultsWithDetails(modifierList, [this, callback](const QList<ModifierInfo>& enrichedList) {
                        updateModifierManagerList(enrichedList);
                        if (callback) {
                            callback(enrichedList);
                        }
                    });
                    return;
                }
                
                updateModifierManagerList(modifierList);
            } else {
                qWarning() << "SearchManager: Failed to fetch search results";
            }
            
            if (callback) {
                callback(modifierList);
            }
        }
    );
}

// Add search term to history
void SearchManager::addSearchToHistory(const QString& searchTerm)
{
    if (searchTerm.isEmpty()) {
        return;
    }
    
    // If already exists, remove the old one first
    m_searchHistory.removeAll(searchTerm);
    
    // Add to the front
    m_searchHistory.prepend(searchTerm);
    
    // Limit history count
    while (m_searchHistory.size() > m_maxHistoryItems) {
        m_searchHistory.removeLast();
    }
    
    // Save history
    saveSearchHistory();
}

// Get search history
QStringList SearchManager::getSearchHistory() const
{
    return m_searchHistory;
}

// Clear search history
void SearchManager::clearSearchHistory()
{
    m_searchHistory.clear();
    saveSearchHistory();
}

// Sort search results by relevance
QList<ModifierInfo> SearchManager::sortByRelevance(const QList<ModifierInfo>& modifiers, 
                                                  const QString& searchTerm)
{
    QList<ModifierInfo> result = modifiers;
    
    // Calculate relevance score for each modifier and sort
    std::sort(result.begin(), result.end(), [this, &searchTerm](const ModifierInfo& a, const ModifierInfo& b) {
        int scoreA = calculateRelevanceScore(a, searchTerm);
        int scoreB = calculateRelevanceScore(b, searchTerm);
        return scoreA > scoreB; // Descending order
    });
    
    return result;
}

// Sort search results by popularity
QList<ModifierInfo> SearchManager::sortByPopularity(const QList<ModifierInfo>& modifiers)
{
    QList<ModifierInfo> result = modifiers;
    
    // Sort by download count (if available)
    std::sort(result.begin(), result.end(), [](const ModifierInfo& a, const ModifierInfo& b) {
        // Should use actual popularity data here
        // Temporarily using options count as popularity indicator
        return a.optionsCount > b.optionsCount;
    });
    
    return result;
}

// Sort search results by date
QList<ModifierInfo> SearchManager::sortByDate(const QList<ModifierInfo>& modifiers)
{
    QList<ModifierInfo> result = modifiers;
    
    // Sort by update date
    std::sort(result.begin(), result.end(), [](const ModifierInfo& a, const ModifierInfo& b) {
        return a.lastUpdate > b.lastUpdate;
    });
    
    return result;
}

// Calculate relevance score between search term and modifier
int SearchManager::calculateRelevanceScore(const ModifierInfo& modifier, const QString& searchTerm)
{
    int score = 0;
    
    // If name contains search term, increase relevance score
    if (modifier.name.contains(searchTerm, Qt::CaseInsensitive)) {
        score += 50;
        
        // Higher score for exact match
        if (modifier.name.compare(searchTerm, Qt::CaseInsensitive) == 0) {
            score += 50;
        }
    }
    
    // Game version contains search term
    if (modifier.gameVersion.contains(searchTerm, Qt::CaseInsensitive)) {
        score += 30;
    }
    
    // Description contains search term
    if (modifier.description.contains(searchTerm, Qt::CaseInsensitive)) {
        score += 20;
    }
    
    // TODO: Can add more relevance calculations, such as tokenization, synonyms, and other advanced methods
    
    return score;
}

// Save search history
void SearchManager::saveSearchHistory()
{
    const QString settingsPath = ConfigManager::getInstance().settingsFilePath();
    QSettings settings(settingsPath, QSettings::IniFormat);
    settings.beginGroup("SearchManager");
    settings.setValue("SearchHistory", m_searchHistory);
    settings.endGroup();
    settings.sync();
}

// Load search history
void SearchManager::loadSearchHistory()
{
    const QString settingsPath = ConfigManager::getInstance().settingsFilePath();
    QSettings settings(settingsPath, QSettings::IniFormat);
    settings.beginGroup("SearchManager");
    m_searchHistory = settings.value("SearchHistory").toStringList();
    settings.endGroup();
}

// Update ModifierManager's modifier list
void SearchManager::updateModifierManagerList(const QList<ModifierInfo>& modifiers)
{
    QList<ModifierInfo> modifiersCopy = modifiers;
    ModifierManager::getInstance().setModifierList(modifiersCopy);
}

// Load featured modifiers from website homepage
void SearchManager::loadFeaturedModifiers(std::function<void(const QList<ModifierInfo>&)> callback)
{
    QString url = "https://flingtrainer.com/";
    
    NetworkManager::getInstance().sendGetRequest(
        url,
        [this, callback](const QByteArray& data, bool success) {
            QList<ModifierInfo> featuredModifiers;
            
            if (success) {
                QString htmlContent = QString::fromUtf8(data);
                
                // Find latest trainers section
                QRegularExpression latestSection("<h2[^>]*>Latest Trainers|<h2[^>]*>Recent Trainers|<div[^>]*class=\"recent-posts");
                QRegularExpressionMatch match = latestSection.match(htmlContent);
                
                if (match.hasMatch()) {
                    int startPos = match.capturedStart();
                    int endPos = htmlContent.indexOf("</section>", startPos);
                    if (endPos == -1) {
                        endPos = htmlContent.indexOf("</div><!-- .site-content -->", startPos);
                    }
                    if (endPos == -1) {
                        endPos = startPos + 15000;
                    }
                    
                    QString sectionHtml = htmlContent.mid(startPos, endPos - startPos);
                    featuredModifiers = ModifierParser::parseModifierListHTML(sectionHtml.toStdString(), "");
                    
                    // Parse article entries
                    QRegularExpression articleRegex("<article[^>]*>(.+?)</article>", QRegularExpression::DotMatchesEverythingOption);
                    QRegularExpressionMatchIterator articleMatches = articleRegex.globalMatch(sectionHtml);
                    
                    int count = 0;
                    while (articleMatches.hasNext() && count < 15) {
                        QRegularExpressionMatch articleMatch = articleMatches.next();
                        QString articleHtml = articleMatch.captured(1);
                        
                        QRegularExpression titleRegex("<h2[^>]*>\\s*<a[^>]*href=\"([^\"]+)\"[^>]*>([^<]+)</a>");
                        QRegularExpressionMatch titleMatch = titleRegex.match(articleHtml);
                        
                        if (titleMatch.hasMatch()) {
                            ModifierInfo info;
                            info.url = titleMatch.captured(1);
                            info.name = ModifierInfoManager::getInstance().formatModifierName(titleMatch.captured(2).trimmed());
                            info.lastUpdate = QDate::currentDate().toString("yyyy-MM-dd");
                            info.gameVersion = "Latest";
                            info.optionsCount = 10;
                            
                            featuredModifiers.append(info);
                            count++;
                        }
                    }
                }
            } else {
                qWarning() << "SearchManager: Failed to fetch featured modifiers";
            }
            
            updateModifierManagerList(featuredModifiers);
            
            if (callback) {
                callback(featuredModifiers);
            }
        }
    );
}

// Fetch recently updated modifiers from flingtrainer.com homepage.
// Uses retries + local cache fallback to avoid empty startup list from transient network errors.
void SearchManager::fetchRecentlyUpdatedModifiers(std::function<void(const QList<ModifierInfo>&)> callback)
{
    const int maxAttempts = 3;
    fetchRecentlyUpdatedModifiersInternal(1, maxAttempts, callback);
}

void SearchManager::fetchRecentlyUpdatedModifiersInternal(
    int attempt,
    int maxAttempts,
    std::function<void(const QList<ModifierInfo>&)> callback)
{
    const QString url = "https://flingtrainer.com/";
    NetworkManager::getInstance().sendGetRequest(
        url,
        [this, callback, attempt, maxAttempts](const QByteArray& data, bool success) {
            QList<ModifierInfo> modifierList;
            bool fromNetwork = false;

            if (success) {
                modifierList = parseRecentlyUpdatedModifiersFromHtml(QString::fromUtf8(data));
                if (!modifierList.isEmpty()) {
                    fromNetwork = true;
                    saveRecentModifiersCache(modifierList);
                } else {
                    qWarning() << "SearchManager: Empty recently updated parse result";
                }
            } else {
                qWarning() << "SearchManager: Failed to fetch recently updated modifiers";
            }

            if (modifierList.isEmpty() && attempt < maxAttempts) {
                qWarning() << "SearchManager: Retrying recently updated fetch, attempt"
                           << (attempt + 1) << "of" << maxAttempts;
                fetchRecentlyUpdatedModifiersInternal(attempt + 1, maxAttempts, callback);
                return;
            }

            if (modifierList.isEmpty()) {
                modifierList = loadRecentModifiersCache();
                if (!modifierList.isEmpty()) {
                    qWarning() << "SearchManager: Using cached recently updated list";
                }
            }

            if (!fromNetwork && modifierList.isEmpty()) {
                qWarning() << "SearchManager: Recently updated list unavailable after retries and cache fallback";
            }

            updateModifierManagerList(modifierList);

            if (callback) {
                callback(modifierList);
            }
        }
    );
}

QList<ModifierInfo> SearchManager::parseRecentlyUpdatedModifiersFromHtml(const QString& html) const
{
    QList<ModifierInfo> modifierList;
    QSet<QString> seenUrls;

    QRegularExpression articleRegex(
        "<article[^>]*class=\"[^\"]*post-standard[^\"]*\"[^>]*>(.*?)</article>",
        QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator matches = articleRegex.globalMatch(html);

    while (matches.hasNext()) {
        QRegularExpressionMatch match = matches.next();
        const QString articleHtml = match.captured(1);

        QRegularExpression titleRegex(
            "<h[123][^>]*class=\"[^\"]*post-title[^\"]*\"[^>]*>\\s*<a[^>]*href=\"([^\"]*)\"[^>]*>([^<]+)</a>",
            QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch titleMatch = titleRegex.match(articleHtml);

        if (!titleMatch.hasMatch()) {
            continue;
        }

        const QString url = titleMatch.captured(1).trimmed();
        if (url.isEmpty() || seenUrls.contains(url)) {
            continue;
        }
        seenUrls.insert(url);

        ModifierInfo modifier;
        modifier.url = url;
        modifier.name = ModifierInfoManager::getInstance().formatModifierName(titleMatch.captured(2).trimmed());

        QRegularExpression dateRegex(
            "<div class=\"post-details-day\">(\\d+)</div>\\s*<div class=\"post-details-month\">([^<]+)</div>\\s*<div class=\"post-details-year\">(\\d+)</div>",
            QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch dateMatch = dateRegex.match(articleHtml);

        if (dateMatch.hasMatch()) {
            const QString day = dateMatch.captured(1).rightJustified(2, '0');
            QString month = dateMatch.captured(2).trimmed().toLower();
            if (month.length() >= 3) {
                month = month.left(3);
            }
            const QString year = dateMatch.captured(3).trimmed();

            static const QMap<QString, QString> monthMap = {
                {"jan", "01"}, {"feb", "02"}, {"mar", "03"}, {"apr", "04"},
                {"may", "05"}, {"jun", "06"}, {"jul", "07"}, {"aug", "08"},
                {"sep", "09"}, {"oct", "10"}, {"nov", "11"}, {"dec", "12"}
            };

            modifier.lastUpdate = QString("%1-%2-%3").arg(year, monthMap.value(month, "01"), day);
        }

        QRegularExpression optionsRegex("(\\d+)\\s*Options", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch optionsMatch = optionsRegex.match(articleHtml);
        modifier.optionsCount = optionsMatch.hasMatch() ? optionsMatch.captured(1).toInt() : 0;

        QRegularExpression versionRegex("Game Version:\\s*([^·<]+)", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch versionMatch = versionRegex.match(articleHtml);
        if (versionMatch.hasMatch()) {
            modifier.gameVersion = versionMatch.captured(1).trimmed();
        } else {
            versionRegex = QRegularExpression("v([0-9\\.]+)\\+?", QRegularExpression::CaseInsensitiveOption);
            versionMatch = versionRegex.match(articleHtml);
            modifier.gameVersion = versionMatch.hasMatch() ? "v" + versionMatch.captured(1) + "+" : "Latest";
        }

        modifierList.append(modifier);
    }

    // If page structure changed, fall back to generic parser to keep startup list usable.
    if (modifierList.isEmpty()) {
        QList<ModifierInfo> fallbackList = ModifierParser::parseModifierListHTML(html.toStdString(), "");
        for (ModifierInfo& modifier : fallbackList) {
            if (modifier.url.isEmpty() || seenUrls.contains(modifier.url)) {
                continue;
            }
            seenUrls.insert(modifier.url);
            modifier.name = ModifierInfoManager::getInstance().formatModifierName(modifier.name);
            modifierList.append(modifier);
            if (modifierList.size() >= 30) {
                break;
            }
        }
    }

    return modifierList;
}

QList<ModifierInfo> SearchManager::loadRecentModifiersCache() const
{
    QList<ModifierInfo> cachedList;
    const QString cachePath = FileSystem::getInstance().getDataDirectory() + "/recent_modifiers_cache.json";
    QFile file(cachePath);

    if (!file.open(QIODevice::ReadOnly)) {
        return cachedList;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isArray()) {
        return cachedList;
    }

    const QJsonArray array = doc.array();
    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject obj = value.toObject();
        ModifierInfo modifier;
        modifier.name = obj.value("name").toString();
        modifier.url = obj.value("url").toString();
        modifier.lastUpdate = obj.value("lastUpdate").toString();
        modifier.gameVersion = obj.value("gameVersion").toString();
        modifier.optionsCount = obj.value("optionsCount").toInt(0);
        modifier.screenshotUrl = obj.value("screenshotUrl").toString();

        if (!modifier.name.isEmpty() && !modifier.url.isEmpty()) {
            cachedList.append(modifier);
        }
    }

    return cachedList;
}

void SearchManager::saveRecentModifiersCache(const QList<ModifierInfo>& modifiers) const
{
    if (modifiers.isEmpty()) {
        return;
    }

    QJsonArray array;
    const int maxCachedItems = 80;
    int count = 0;

    for (const ModifierInfo& modifier : modifiers) {
        if (modifier.name.isEmpty() || modifier.url.isEmpty()) {
            continue;
        }

        QJsonObject obj;
        obj["name"] = modifier.name;
        obj["url"] = modifier.url;
        obj["lastUpdate"] = modifier.lastUpdate;
        obj["gameVersion"] = modifier.gameVersion;
        obj["optionsCount"] = modifier.optionsCount;
        obj["screenshotUrl"] = modifier.screenshotUrl;
        array.append(obj);

        count++;
        if (count >= maxCachedItems) {
            break;
        }
    }

    const QString cachePath = FileSystem::getInstance().getDataDirectory() + "/recent_modifiers_cache.json";
    QFile file(cachePath);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "SearchManager: Failed to save recent modifiers cache:" << cachePath;
        return;
    }

    file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
    file.close();
}

// Enrich search results with data from detail pages
void SearchManager::enrichSearchResultsWithDetails(QList<ModifierInfo>& modifiers,
                                                    std::function<void(const QList<ModifierInfo>&)> callback)
{
    if (modifiers.isEmpty()) {
        if (callback) {
            callback(modifiers);
        }
        return;
    }
    
    // Use shared pointer to track progress across async calls
    auto resultList = std::make_shared<QList<ModifierInfo>>(modifiers);
    auto pendingCount = std::make_shared<int>(modifiers.size());
    auto mutex = std::make_shared<QMutex>();
    
    for (int i = 0; i < modifiers.size(); ++i) {
        const ModifierInfo& mod = modifiers[i];
        
        // Skip if already has data
        if (mod.optionsCount > 0 && !mod.gameVersion.isEmpty()) {
            QMutexLocker locker(mutex.get());
            (*pendingCount)--;
            if (*pendingCount == 0) {
                if (callback) {
                    callback(*resultList);
                }
            }
            continue;
        }
        
        // Fetch detail page
        QString detailUrl = mod.url;
        if (detailUrl.isEmpty()) {
            QMutexLocker locker(mutex.get());
            (*pendingCount)--;
            if (*pendingCount == 0) {
                if (callback) {
                    callback(*resultList);
                }
            }
            continue;
        }
        
        int index = i;
        NetworkManager::getInstance().sendGetRequest(
            detailUrl,
            [resultList, pendingCount, mutex, callback, index](const QByteArray& data, bool success) {
                if (success) {
                    QString htmlContent = QString::fromUtf8(data);
                    
                    // Extract options count from detail page (format: "24 Options")
                    int optionsCount = 0;
                    QRegularExpression optionsRegex("(\\d+)\\s*Options", QRegularExpression::CaseInsensitiveOption);
                    QRegularExpressionMatch optionsMatch = optionsRegex.match(htmlContent);
                    if (optionsMatch.hasMatch()) {
                        optionsCount = optionsMatch.captured(1).toInt();
                    }
                    
                    // Extract game version from detail page (format: "Game Version: v1.02-v1.05+")
                    QString gameVersion;
                    QRegularExpression versionRegex("Game Version:\\s*([^<\\n·]+)", QRegularExpression::CaseInsensitiveOption);
                    QRegularExpressionMatch versionMatch = versionRegex.match(htmlContent);
                    if (versionMatch.hasMatch()) {
                        gameVersion = versionMatch.captured(1).trimmed();
                    }
                    
                    {
                        QMutexLocker locker(mutex.get());
                        if (index < resultList->size()) {
                            if (optionsCount > 0) {
                                (*resultList)[index].optionsCount = optionsCount;
                            }
                            if (!gameVersion.isEmpty()) {
                                (*resultList)[index].gameVersion = gameVersion;
                            }
                        }
                    }
                }
                
                {
                    QMutexLocker locker(mutex.get());
                    (*pendingCount)--;
                    if (*pendingCount == 0) {
                        qDebug() << "SearchManager: Finished enriching search results from detail pages";
                        if (callback) {
                            callback(*resultList);
                        }
                    }
                }
            }
        );
    }
}
