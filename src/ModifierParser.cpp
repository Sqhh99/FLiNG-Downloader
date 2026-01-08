#include "ModifierParser.h"
#include <QDebug>
#include <QRegularExpression>

// Helper function: check if word is a number or sequence word
bool isNumberOrSequenceWord(const QString& word) {
    QString wordLower = word.toLower();
    
    // Check if it's a pure number
    bool isNumber;
    wordLower.toInt(&isNumber);
    if (isNumber) return true;
    
    // Check if it's a Roman numeral or sequence word
    QStringList sequenceWords = {
        "i", "ii", "iii", "iv", "v", "vi", "vii", "viii", "ix", "x",
        "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten",
        "age", "episode", "part", "vol", "volume", "season", "chapter"
    };
    
    return sequenceWords.contains(wordLower);
}

// Helper function: check if title contains equivalent number expression
bool containsEquivalentNumber(const QString& title, const QString& searchWord) {
    QString titleLower = title.toLower();
    QString wordLower = searchWord.toLower();
    
    // Number equivalence mapping
    QMap<QString, QStringList> numberEquivalents = {
        {"1", {"i", "one", "first"}},
        {"2", {"ii", "two", "second", "age"}},  // "age" often indicates sequel
        {"3", {"iii", "three", "third"}},
        {"4", {"iv", "four", "fourth"}},
        {"5", {"v", "five", "fifth"}},
        {"6", {"vi", "six", "sixth"}},
        {"age", {"2", "ii", "two", "second"}},  // "age" usually refers to sequel
        {"episode", {"part", "vol", "volume"}},
        {"part", {"episode", "vol", "volume"}}
    };
    
    // Check for equivalent expressions
    for (auto it = numberEquivalents.begin(); it != numberEquivalents.end(); ++it) {
        QString key = it.key();
        QStringList values = it.value();
        
        // If search word is key, check if title contains any value
        if (wordLower == key) {
            for (const QString& value : values) {
                if (titleLower.contains(value)) {
                    return true;
                }
            }
        }
        
        // If search word is one of values, check if title contains key
        if (values.contains(wordLower) && titleLower.contains(key)) {
            return true;
        }
    }
    
    return false;
}

// HTML entity decode function
QString decodeHtmlEntities(const QString& text) {
    QString result = text;
    
    // Common HTML entity replacements
    result.replace("&amp;", "&");
    result.replace("&lt;", "<");
    result.replace("&gt;", ">");
    result.replace("&quot;", "\"");
    result.replace("&#39;", "'");
    result.replace("&apos;", "'");
    result.replace("&#8217;", "'");  // Right single quote
    result.replace("&#8216;", "'");  // Left single quote
    result.replace("&#8220;", "\""); // Left double quote
    result.replace("&#8221;", "\""); // Right double quote
    result.replace("&#8211;", "-");  // en dash
    result.replace("&#8212;", "—");  // em dash
    
    return result;
}

ModifierParser::ModifierParser()
{
}

ModifierParser::~ModifierParser()
{
}

// Parse modifier list HTML
QList<ModifierInfo> ModifierParser::parseModifierListHTML(const std::string& html, const QString& searchTerm)
{
    QList<ModifierInfo> result;
    
    try {
        // Return empty for invalid HTML
        if (html.size() < 10) {
            qDebug() << "HTML data too short to parse";
            return result;
        }
        
        QString htmlQt = QString::fromStdString(html);
        
        // Check article count
        int articleCount = htmlQt.count("<article", Qt::CaseInsensitive);
        
        // Check if page is a search results page
        bool isSearchPage = htmlQt.contains("SEARCH RESULTS", Qt::CaseInsensitive) || 
                           !searchTerm.isEmpty();
        
        // Process search term - split into words for flexible matching
        QStringList searchWords;
        if (!searchTerm.isEmpty()) {
            // Decode HTML entities
            QString decodedSearchTerm = decodeHtmlEntities(searchTerm);
            
            // Split search term into words, keeping important short words (numbers, sequences)
            QStringList words = decodedSearchTerm.split(" ", Qt::SkipEmptyParts);
            for (const QString& word : words) {
                QString trimmedWord = word.trimmed();
                // Keep numbers, sequence words, or words longer than 2 chars
                if (trimmedWord.length() >= 1 && 
                    (isNumberOrSequenceWord(trimmedWord) || trimmedWord.length() > 2) &&
                    trimmedWord.compare("the", Qt::CaseInsensitive) != 0 && 
                    trimmedWord.compare("and", Qt::CaseInsensitive) != 0 && 
                    trimmedWord.compare("for", Qt::CaseInsensitive) != 0 && 
                    trimmedWord.compare("of", Qt::CaseInsensitive) != 0 &&
                    trimmedWord.compare("in", Qt::CaseInsensitive) != 0) {
                    searchWords << trimmedWord;
                }
            }
        }
        
        // Use regex to find all article entries
        QRegularExpression articleRegex;
        
        if (isSearchPage) {
            // Search results page article pattern
            articleRegex = QRegularExpression("<article[^>]*class=\"[^\"]*post[^\"]*\"[^>]*>(.*?)</article>", 
                                             QRegularExpression::DotMatchesEverythingOption);
        } else {
            // Homepage article pattern (usually in "RECENTLY UPDATED" section)
            articleRegex = QRegularExpression("<article[^>]*>(.*?)</article>", 
                                             QRegularExpression::DotMatchesEverythingOption);
        }
        
        // Find all matching articles
        QRegularExpressionMatchIterator matches = articleRegex.globalMatch(htmlQt);
        
        // Process each article entry
        int matchCount = 0;
        while (matches.hasNext()) {
            QRegularExpressionMatch match = matches.next();
            QString articleHtml = match.captured(1);
            matchCount++;
            
            // Extract modifier name and URL from post-title
            QRegularExpression titleRegex("<h[123][^>]*class=\"[^\"]*post-title[^\"]*\"[^>]*>\\s*<a[^>]*href=\"([^\"]*)\"[^>]*>([^<]+)</a>", 
                                         QRegularExpression::DotMatchesEverythingOption);
            QRegularExpressionMatch titleMatch = titleRegex.match(articleHtml);
            
            if (titleMatch.hasMatch()) {
                QString url = titleMatch.captured(1);
                QString title = titleMatch.captured(2).trimmed();
                
                // If title contains "Trainer" or URL contains trainer
                if (title.contains("Trainer", Qt::CaseInsensitive) || 
                    url.contains("trainer", Qt::CaseInsensitive)) {
                    ModifierInfo modifier;
                    
                    // Extract modifier name (remove trailing "Trainer" and decode HTML entities)
                    modifier.name = decodeHtmlEntities(title);
                    modifier.name.replace(QRegularExpression("\\s+Trainer\\s*$", QRegularExpression::CaseInsensitiveOption), "");
                    modifier.url = url;
                    // Initialize optionsCount to 0
                    modifier.optionsCount = 0;
                    
                    // Extract screenshot URL for cover extraction
                    QRegularExpression screenshotRegex("<img[^>]*src=\"([^\"]*\\.(?:jpg|png|gif))\"[^>]*>", 
                                                      QRegularExpression::CaseInsensitiveOption);
                    QRegularExpressionMatch screenshotMatch = screenshotRegex.match(articleHtml);
                    if (screenshotMatch.hasMatch()) {
                        modifier.screenshotUrl = screenshotMatch.captured(1);
                    }
                    
                    // Decode title for matching
                    QString decodedTitle = decodeHtmlEntities(title);
                    
                    // Special handling for Red Dead Redemption 2 search
                    bool isRDR2Search = searchTerm.contains("Red Dead Redemption 2", Qt::CaseInsensitive);
                    bool isRDRMatch = decodedTitle.contains("Red Dead Redemption", Qt::CaseInsensitive);
                    
                    // Improved search term matching logic
                    bool matchesSearch = false;
                    
                    // Priority: direct containment match (using decoded title)
                    if (searchTerm.isEmpty() || 
                        decodedTitle.contains(searchTerm, Qt::CaseInsensitive) || 
                        modifier.name.contains(searchTerm, Qt::CaseInsensitive)) {
                        matchesSearch = true;
                    } 
                    // If not exact match, check keyword matching
                    else if (!searchWords.isEmpty()) {
                        int matchedWords = 0;
                        QString titleLower = decodedTitle.toLower();
                        
                        // Check all keywords in title with smart number/sequence matching
                        for (const QString& word : searchWords) {
                            QString wordLower = word.toLower();
                            if (titleLower.contains(wordLower)) {
                                matchedWords++;
                            }
                            // Smart matching for numbers and sequence words
                            else if (isNumberOrSequenceWord(word) && containsEquivalentNumber(titleLower, wordLower)) {
                                matchedWords++;
                            }
                        }
                        
                        // Improved match threshold for flexible matching
                        double matchRatio = static_cast<double>(matchedWords) / searchWords.size();
                        if ((matchRatio >= 0.6) ||                              // 60%+ match
                            (matchedWords >= 1 && searchWords.size() <= 2) ||   // 2 words or less, 1 match is enough
                            (matchedWords >= 2 && matchRatio >= 0.4)) {         // Multiple words, at least 2 and 40%+
                            matchesSearch = true;
                        }
                    }
                    
                    // Special handling for Red Dead Redemption series
                    if (!matchesSearch && (searchTerm.contains("Red Dead", Qt::CaseInsensitive) && isRDRMatch)) {
                        matchesSearch = true;
                    }
                    
                    // Skip non-matching entries
                    if (!matchesSearch && !searchTerm.isEmpty()) {
                        continue;
                    }
                    
                    // Extract update date from post-details format
                    QRegularExpression postDateRegex("<div class=\"post-details-day\">(\\d+)</div>\\s*<div class=\"post-details-month\">([^<]+)</div>\\s*<div class=\"post-details-year\">(\\d+)</div>", 
                                                    QRegularExpression::DotMatchesEverythingOption);
                    QRegularExpressionMatch postDateMatch = postDateRegex.match(articleHtml);
                    if (postDateMatch.hasMatch()) {
                        QString day = postDateMatch.captured(1);
                        QString month = postDateMatch.captured(2);
                        QString year = postDateMatch.captured(3);
                        
                        static QMap<QString, QString> monthMap = {
                            {"Jan", "01"}, {"Feb", "02"}, {"Mar", "03"}, {"Apr", "04"},
                            {"May", "05"}, {"Jun", "06"}, {"Jul", "07"}, {"Aug", "08"},
                            {"Sep", "09"}, {"Oct", "10"}, {"Nov", "11"}, {"Dec", "12"}
                        };
                        
                        QString monthNum = monthMap.value(month, "01");
                        modifier.lastUpdate = QString("%1-%2-%3").arg(year, monthNum, day.rightJustified(2, '0'));
                    }
                    
                    // Note: Search results page does NOT contain options count or game version
                    // These will be fetched from detail pages by SearchManager::enrichSearchResultsWithDetails
                    
                    // Create ModifierInfo and add to result
                    modifier.name = title;
                    modifier.url = url;
                    result.append(modifier);
                }
            }
        }
        
        qDebug() << "Parsed" << result.size() << "modifiers from HTML";
        
        // If no results found, return empty list
        // Do not try alternative search methods
    }
    catch (const std::exception& e) {
        qDebug() << "HTML parsing exception:" << e.what();
    }
    
    return result;
}

// Parse modifier detail HTML
ModifierInfo* ModifierParser::parseModifierDetailHTML(const std::string& html, const QString& modifierName)
{
    ModifierInfo* modifier = new ModifierInfo();
    modifier->name = modifierName;
    modifier->optionsCount = 0; // Initialize to 0
    
    try {
        QString htmlQt = QString::fromStdString(html);
        
        qDebug() << "Parsing modifier detail:" << modifierName;
        
        // Extract game version - try multiple patterns
        QRegularExpression versionRegex("Game Version:\\s*([^<·]+)", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch versionMatch = versionRegex.match(htmlQt);
        if (versionMatch.hasMatch()) {
            modifier->gameVersion = versionMatch.captured(1).trimmed();
        } else {
            // Try alternative version patterns
            QRegularExpression altVersionRegex("Version\\s*:\\s*([^<\\n]+)", QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch altVersionMatch = altVersionRegex.match(htmlQt);
            if (altVersionMatch.hasMatch()) {
                modifier->gameVersion = altVersionMatch.captured(1).trimmed();
            } else {
                // Try from HTML title
                QRegularExpression titleVersionRegex("<title>.*?v([\\d\\.]+).*?</title>", 
                                                QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
                QRegularExpressionMatch titleVersionMatch = titleVersionRegex.match(htmlQt);
                if (titleVersionMatch.hasMatch()) {
                    modifier->gameVersion = "v" + titleVersionMatch.captured(1).trimmed();
                } else {
                    // Try from meta description
                    QRegularExpression metaVersionRegex("<meta[^>]*content\\s*=\\s*[\"'].*?version\\s*[:\\s]\\s*([^,\"'<>]+)[\"']", 
                                                   QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
                    QRegularExpressionMatch metaVersionMatch = metaVersionRegex.match(htmlQt);
                    if (metaVersionMatch.hasMatch()) {
                        modifier->gameVersion = metaVersionMatch.captured(1).trimmed();
                    } else {
                        // Try from post-meta element (FLiNG website specific)
                        QRegularExpression postMetaRegex("<div class=\"post-meta[^\"]*\"[^>]*>(.*?)</div>", 
                                                       QRegularExpression::DotMatchesEverythingOption);
                        QRegularExpressionMatch postMetaMatch = postMetaRegex.match(htmlQt);
                        if (postMetaMatch.hasMatch()) {
                            QString metaContent = postMetaMatch.captured(1);
                            
                            // Find Game Version marker
                            QRegularExpression gameVersionRegex("Game Version:\\s*([^<]+)", QRegularExpression::CaseInsensitiveOption);
                            QRegularExpressionMatch gameVersionMatch = gameVersionRegex.match(metaContent);
                            if (gameVersionMatch.hasMatch()) {
                                modifier->gameVersion = gameVersionMatch.captured(1).trimmed();
                            } else if (metaContent.contains("Early Access", Qt::CaseInsensitive)) {
                                modifier->gameVersion = "Early Access";
                            }
                        } else {
                            // Try from flex-content area
                            QRegularExpression flexContentRegex("<div class=\"flex-content[^\"]*\"[^>]*>(.*?)</div>", 
                                                             QRegularExpression::DotMatchesEverythingOption);
                            QRegularExpressionMatch flexContentMatch = flexContentRegex.match(htmlQt);
                            if (flexContentMatch.hasMatch()) {
                                QString flexContent = flexContentMatch.captured(1);
                                if (flexContent.contains("Early Access", Qt::CaseInsensitive)) {
                                    modifier->gameVersion = "Early Access";
                                }
                            } else {
                                // Use default value
                                modifier->gameVersion = "Latest";
                            }
                        }
                    }
                }
            }
        }
        
        // Special handling for Early Access+
        if (htmlQt.contains("Early Access+")) {
            modifier->gameVersion = "Early Access+";
        }
        
        // Extract last update date
        QRegularExpression lastUpdateRegex("Last Updated:\\s*([^<\\n]+)", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch lastUpdateMatch = lastUpdateRegex.match(htmlQt);
        if (lastUpdateMatch.hasMatch()) {
            modifier->lastUpdate = lastUpdateMatch.captured(1).trimmed();
        } else {
            // Try from post-meta or flex-content
            QRegularExpression dateRegex("<div[^>]*>.*?Last Updated:\\s*([\\d\\.]+).*?</div>", 
                                     QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch dateMatch = dateRegex.match(htmlQt);
            if (dateMatch.hasMatch()) {
                modifier->lastUpdate = dateMatch.captured(1).trimmed();
            } else {
                // Try from attachment-date class
                QRegularExpression attachmentDateRegex("<td class=\"attachment-date\"[^>]*>([^<]+)</td>", 
                                                    QRegularExpression::CaseInsensitiveOption);
                QRegularExpressionMatch attachmentDateMatch = attachmentDateRegex.match(htmlQt);
                if (attachmentDateMatch.hasMatch()) {
                    modifier->lastUpdate = attachmentDateMatch.captured(1).trimmed();
                }
            }
        }
        
        // Extract options count
        QRegularExpression optionsCountRegex("Options:\\s*(\\d+)", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch optionsCountMatch = optionsCountRegex.match(htmlQt);
        if (optionsCountMatch.hasMatch()) {
            modifier->optionsCount = optionsCountMatch.captured(1).toInt();
        }
        
        // Find tables that may contain version info
        QRegularExpression tableRegex("<table[^>]*>.*?</table>", 
                                  QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatchIterator tableMatches = tableRegex.globalMatch(htmlQt);
        
        bool foundDownloadTable = false;
        
        while (tableMatches.hasNext() && !foundDownloadTable) {
            QRegularExpressionMatch tableMatch = tableMatches.next();
            QString tableHtml = tableMatch.captured(0);
            
            // Check if table contains download-related content
            if (tableHtml.contains("download", Qt::CaseInsensitive) || 
                tableHtml.contains("attachment", Qt::CaseInsensitive) || 
                tableHtml.contains("file", Qt::CaseInsensitive)) {
                
                foundDownloadTable = true;
                
                // Extract links from the table
                QRegularExpression linkRegex("<a[^>]*href=\"([^\"]+)\"[^>]*>(.*?)</a>", 
                                       QRegularExpression::DotMatchesEverythingOption);
                QRegularExpressionMatchIterator linkMatches = linkRegex.globalMatch(tableHtml);
                
                while (linkMatches.hasNext()) {
                    QRegularExpressionMatch linkMatch = linkMatches.next();
                    QString downloadLink = linkMatch.captured(1);
                    QString linkText = linkMatch.captured(2).trimmed();
                    
                    // Filter HTML tags to get plain text
                    linkText.replace(QRegularExpression("<[^>]*>"), "");
                    
                    // Check if this is a download link
                    if ((downloadLink.contains("download", Qt::CaseInsensitive) || 
                         downloadLink.contains("trainer", Qt::CaseInsensitive) || 
                         downloadLink.contains("file", Qt::CaseInsensitive) || 
                         linkText.contains("download", Qt::CaseInsensitive)) &&
                        !downloadLink.contains("javascript:", Qt::CaseInsensitive)) { // Exclude JavaScript pseudo-links
                        
                        // Determine version name
                        QString versionName = linkText;
                        if (versionName.isEmpty() || versionName.contains("img", Qt::CaseInsensitive)) {
                            // If link text is empty or only contains image tags, use more descriptive name
                            if (tableHtml.contains("Early Access", Qt::CaseInsensitive)) {
                                versionName = "Early Access";
                            } else if (tableHtml.contains("Auto", Qt::CaseInsensitive)) {
                                versionName = "Auto Update";
                            } else {
                                versionName = "Download #" + QString::number(modifier->versions.size() + 1);
                            }
                        }
                        
                        // Add version
                        modifier->versions.append(qMakePair(versionName, downloadLink));
                    }
                }
            }
        }
        
        // If not found in table, search download section
        if (modifier->versions.isEmpty()) {
            QRegularExpression downloadSectionRegex("<div[^>]*class=\"[^\"]*download[^\"]*\"[^>]*>(.*?)</div>", 
                                                 QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch downloadMatch = downloadSectionRegex.match(htmlQt);
            
            if (downloadMatch.hasMatch()) {
                QString downloadSection = downloadMatch.captured(1);
                
                // Extract links
                QRegularExpression linkRegex("<a[^>]*href=\"([^\"]+)\"[^>]*>(.*?)</a>", 
                                       QRegularExpression::DotMatchesEverythingOption);
                QRegularExpressionMatchIterator linkMatches = linkRegex.globalMatch(downloadSection);
                
                while (linkMatches.hasNext()) {
                    QRegularExpressionMatch linkMatch = linkMatches.next();
                    QString downloadLink = linkMatch.captured(1);
                    QString linkText = linkMatch.captured(2).trimmed();
                    
                    // Filter HTML tags to get plain text
                    linkText.replace(QRegularExpression("<[^>]*>"), "");
                    
                    // If link text is empty, use more descriptive name
                    if (linkText.isEmpty() || linkText.contains("img", Qt::CaseInsensitive)) {
                        linkText = "Download #" + QString::number(modifier->versions.size() + 1);
                    }
                    
                    // Add version
                    modifier->versions.append(qMakePair(linkText, downloadLink));
                }
            }
        }

        // Find attachment-link class links, a common download link format on FLiNG website
        QRegularExpression attachmentLinkRegex("<a[^>]*class=\"[^\"]*attachment-link[^\"]*\"[^>]*href=\"([^\"]+)\"[^>]*>([^<]*)</a>", 
                                             QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatchIterator attachmentMatches = attachmentLinkRegex.globalMatch(htmlQt);
        
        while (attachmentMatches.hasNext()) {
            QRegularExpressionMatch attachmentMatch = attachmentMatches.next();
            QString downloadLink = attachmentMatch.captured(1);
            QString linkText = attachmentMatch.captured(2).trimmed();
            
            // If link text is empty, search surrounding content for more info
            if (linkText.isEmpty()) {
                // Find the title or description of the link's section
                int linkPos = htmlQt.indexOf(attachmentMatch.captured(0));
                int contextStart = qMax(0, htmlQt.lastIndexOf("<div", linkPos));
                int contextEnd = qMin(htmlQt.length(), htmlQt.indexOf("</div>", linkPos) + 6);
                
                QString context = htmlQt.mid(contextStart, contextEnd - contextStart);
                
                if (context.contains("Early Access", Qt::CaseInsensitive)) {
                    linkText = "Early Access";
                } else if (context.contains("FLiNG", Qt::CaseInsensitive)) {
                    linkText = "FLiNG Version";
                } else {
                    linkText = "Download #" + QString::number(modifier->versions.size() + 1);
                }
            }
            
            // Add version
            modifier->versions.append(qMakePair(linkText, downloadLink));
        }
        
        // Extract options list
        // Use parseOptionsFromHTML method to extract options
        modifier->options = parseOptionsFromHTML(htmlQt);
        
        // If no options extracted, leave options empty
        // Do not provide fake generic options
        
        // If options is empty but options count exists, update options count
        if (modifier->optionsCount <= 0) {
            modifier->optionsCount = modifier->options.size();
        }
        
        // Extract modifier screenshot URL for game cover extraction
        QRegularExpression screenshotRegex("<img[^>]*src\\s*=\\s*[\"']([^\"']*\\.(jpg|jpeg|png|gif))[\"'][^>]*>", 
                                          QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch screenshotMatch = screenshotRegex.match(htmlQt);
        if (screenshotMatch.hasMatch()) {
            modifier->screenshotUrl = screenshotMatch.captured(1);
        } else {
            // Try to find more general image patterns
            QRegularExpression imgRegex("<img[^>]+src\\s*=\\s*[\"']([^\"']+)[\"'][^>]*>", 
                                       QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatchIterator imgMatches = imgRegex.globalMatch(htmlQt);
            while (imgMatches.hasNext()) {
                QRegularExpressionMatch imgMatch = imgMatches.next();
                QString imgUrl = imgMatch.captured(1);
                // Exclude small icons, select large images that may be game screenshots
                if (!imgUrl.contains("icon", Qt::CaseInsensitive) && 
                    !imgUrl.contains("logo", Qt::CaseInsensitive) && 
                    (imgUrl.contains("screenshot", Qt::CaseInsensitive) || 
                     imgUrl.contains("image", Qt::CaseInsensitive) ||
                     imgUrl.endsWith(".jpg", Qt::CaseInsensitive) ||
                     imgUrl.endsWith(".png", Qt::CaseInsensitive))) {
                    modifier->screenshotUrl = imgUrl;
                    break;
                }
            }
        }

    } catch (const std::exception& e) {
        qDebug() << "Exception during HTML parsing:" << e.what();
    }
    
    // If name was passed in but internal name is empty, use the passed name
    if (modifier->name.isEmpty() && !modifierName.isEmpty()) {
        modifier->name = modifierName;
    }
    qDebug() << "Modifier details parsed:" << modifier->name << ", options:" << modifier->options.size();
    
    return modifier;
}

// Parse game options directly from HTML content
QStringList ModifierParser::parseOptionsFromHTML(const QString& html) {
    QStringList options;
    
    if (html.isEmpty()) {
        return options;
    }
    
    try {
        // Unified options parsing: use generic parsing rules for all games
        QMap<QString, QString> cleanOptions;
        
        // Handle complex HTML structures (e.g., FLiNG trainer website format)
        // This format contains tooltips, JavaScript and other complex elements
        QRegularExpression complexHtmlRegex(
            "((?:Num|Ctrl\\+Num|Alt\\+Num|Shift\\+(?:Num|F\\d))\\s*[\\d\\+\\-\\.]+|(?:Ctrl|Alt|Shift)\\+[\\d\\+\\-\\.]+)\\s*(?:&#8211;|[-–])\\s*([^<]+?)(?=\\s*<(?:span|script|br))",
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption
        );
        QRegularExpressionMatchIterator complexMatches = complexHtmlRegex.globalMatch(html);
        
        while (complexMatches.hasNext()) {
            QRegularExpressionMatch match = complexMatches.next();
            QString hotkey = match.captured(1).trimmed();
            QString description = match.captured(2).trimmed();
            
            // Clean hotkey format
            hotkey.replace(QRegularExpression("\\s+"), " ");
            
            // Clean description text
            description.replace("&amp;", "&");
            description.replace("&#8211;", "-");
            description.replace("&#046;", ".");
            description.replace("&#8217;", "'");
            description = description.trimmed();
            
            // Validate this is a valid option
            if (!description.isEmpty() && 
                !description.contains("jQuery", Qt::CaseInsensitive) && 
                !description.contains("function", Qt::CaseInsensitive) &&
                !description.contains("script", Qt::CaseInsensitive) &&
                !description.contains("tooltip", Qt::CaseInsensitive) &&
                description.length() > 2) {
                
                cleanOptions[hotkey] = description;
            }
        }
        
        // Handle complex options separated by <br> tags
        QRegularExpression brComplexRegex(
            "((?:Num|Ctrl\\+Num|Alt\\+Num|Shift\\+(?:Num|F\\d))\\s*[\\d\\+\\-\\.]+|(?:Ctrl|Alt|Shift)\\+[\\d\\+\\-\\.]+)\\s*(?:&#8211;|[-–])\\s*([^<]+?)\\s*<br",
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption
        );
        QRegularExpressionMatchIterator brComplexMatches = brComplexRegex.globalMatch(html);
        
        while (brComplexMatches.hasNext()) {
            QRegularExpressionMatch match = brComplexMatches.next();
            QString hotkey = match.captured(1).trimmed();
            QString description = match.captured(2).trimmed();
            
            // Clean hotkey format
            hotkey.replace(QRegularExpression("\\s+"), " ");
            
            // Clean description text
            description.replace("&amp;", "&");
            description.replace("&#8211;", "-");
            description.replace("&#046;", ".");
            description.replace("&#8217;", "'");
            description = description.trimmed();
            
            // Validate this is a valid option
            if (!description.isEmpty() && 
                !description.contains("jQuery", Qt::CaseInsensitive) && 
                !description.contains("function", Qt::CaseInsensitive) &&
                !description.contains("script", Qt::CaseInsensitive) &&
                description.length() > 2) {
                
                // Only replace if current option is more complete
                if (!cleanOptions.contains(hotkey) || cleanOptions[hotkey].length() < description.length()) {
                    cleanOptions[hotkey] = description;
                }
            }
        }
        
        // Organize parsed options by category
        if (!cleanOptions.isEmpty()) {
            QStringList basicOptions;
            QStringList ctrlOptions;
            QStringList altOptions;
            QStringList shiftOptions;            
            QMapIterator<QString, QString> it(cleanOptions);
            while (it.hasNext()) {
                it.next();
                QString hotkey = it.key();
                QString description = it.value();
                QString formattedOption = "• " + hotkey + " – " + description;
                
                if (hotkey.startsWith("Ctrl+", Qt::CaseInsensitive)) {
                    ctrlOptions.append(formattedOption);
                } else if (hotkey.startsWith("Alt+", Qt::CaseInsensitive)) {
                    altOptions.append(formattedOption);
                } else if (hotkey.startsWith("Shift+", Qt::CaseInsensitive)) {
                    shiftOptions.append(formattedOption);
                } else {
                    basicOptions.append(formattedOption);
                }
            }
            
            // Add options by category
            if (!basicOptions.isEmpty()) {
                options.append("● Basic Options");
                options.append(basicOptions);
            }
            
            if (!ctrlOptions.isEmpty()) {
                options.append("● Ctrl Hotkey Options");
                options.append(ctrlOptions);
            }
            
            if (!altOptions.isEmpty()) {
                options.append("● Alt Hotkey Options");
                options.append(altOptions);
            }
            
            if (!shiftOptions.isEmpty()) {
                options.append("● Shift Hotkey Options");
                options.append(shiftOptions);
            }
        }
        
        // If options couldn't be extracted, return empty list
        // Do not provide fake generic options
    } catch (const std::exception& e) {
        qDebug() << "Exception during HTML content parsing:" << e.what();
    }
    
    return options;
}

// Detect game name from HTML content
QString ModifierParser::detectGameNameFromHTML(const QString& html) {
    QString gameName = "";
    
    if (html.isEmpty()) {
        return gameName;
    }
    
    try {
        // First try to extract from title tag
        QRegularExpression titleRegex("<title[^>]*>(.*?)</title>", 
                                QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatch titleMatch = titleRegex.match(html);
        
        if (titleMatch.hasMatch()) {
            QString title = titleMatch.captured(1).trimmed();
            
            // Clean title
            title.replace(" Trainer", "", Qt::CaseInsensitive);
            title.replace(" Cheat", "", Qt::CaseInsensitive);
            title.replace(" / ", " ");
            title.replace(" | ", " ");
            title.replace(" - ", " ");
            
            // Extract game name (usually the part before colon, if there is one)
            if (title.contains(":")) {
                gameName = title.section(':', 0, 0).trimmed();
            } else {
                gameName = title;
            }
        }
        
        // If unable to extract from title, try other methods
        if (gameName.isEmpty() || gameName.contains("FLiNG", Qt::CaseInsensitive)) {
            // Find heading tags that may contain game name
            QRegularExpression headingRegex("<h[1-6][^>]*>(.*?)</h[1-6]>", 
                                      QRegularExpression::DotMatchesEverythingOption);
            QRegularExpressionMatchIterator headingMatches = headingRegex.globalMatch(html);
            
            while (headingMatches.hasNext() && (gameName.isEmpty() || gameName.contains("FLiNG"))) {
                QRegularExpressionMatch headingMatch = headingMatches.next();
                QString heading = headingMatch.captured(1).trimmed();
                
                // Clean HTML tags
                heading.replace(QRegularExpression("<[^>]*>"), "");
                
                // If contains "Trainer" but not "FLiNG" or other generic words, may be game name
                if (heading.contains("Trainer", Qt::CaseInsensitive) && 
                    !heading.contains("FLiNG", Qt::CaseInsensitive) && 
                    !heading.contains("Download", Qt::CaseInsensitive)) {
                    
                    // Extract game name
                    heading.replace(" Trainer", "", Qt::CaseInsensitive);
                    heading.replace(" Cheat", "", Qt::CaseInsensitive);
                    
                    gameName = heading.trimmed();
                }
            }
        }
        
        // If still unable to extract, try from URL or other content
        if (gameName.isEmpty()) {
            // Find links containing "trainer"
            QRegularExpression linkRegex("<a[^>]*href=\"([^\"]*trainer[^\"]*)\"[^>]*>(.*?)</a>", 
                                   QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
            QRegularExpressionMatch linkMatch = linkRegex.match(html);
            
            if (linkMatch.hasMatch()) {
                QString url = linkMatch.captured(1);
                
                // Extract game name from URL
                QRegularExpression urlGameNameRegex("/trainer/([^/]+)", QRegularExpression::CaseInsensitiveOption);
                QRegularExpressionMatch urlGameNameMatch = urlGameNameRegex.match(url);
                
                if (urlGameNameMatch.hasMatch()) {
                    QString urlGameName = urlGameNameMatch.captured(1).trimmed();
                    
                    // Replace URL separators with spaces
                    urlGameName.replace("-", " ");
                    urlGameName.replace("_", " ");
                    urlGameName.replace("trainer", "", Qt::CaseInsensitive);
                    
                    gameName = urlGameName.trimmed();
                }
            }
        }
        
        // Do not hardcode specific game names
        
    } catch (const std::exception& e) {
        qDebug() << "Exception during game name detection:" << e.what();
    }
    return gameName;
}

