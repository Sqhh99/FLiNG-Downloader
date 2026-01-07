#include "translator.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <ctime>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// HTTP response callback function
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// URL encode function (global)
std::string globalUrlEncode(const std::string& str) {
    CURL* curl = curl_easy_init();
    if (!curl) return str;
    
    char* encoded = curl_easy_escape(curl, str.c_str(), str.length());
    std::string result(encoded);
    curl_free(encoded);
    curl_easy_cleanup(curl);
    return result;
}

// AppWorlds translator implementation
AppWorldsTranslator::AppWorldsTranslator() 
    : baseUrl("https://translate.appworlds.cn")
    , requestInterval(2.1)
    , lastRequestTime(0) {
    
    headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";
    headers["Accept"] = "application/json, text/plain, */*";
    headers["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8";
    headers["Content-Type"] = "application/x-www-form-urlencoded";
}

void AppWorldsTranslator::waitForRateLimit() {
    auto now = std::chrono::system_clock::now();
    auto nowTime = std::chrono::system_clock::to_time_t(now);
    
    double timeSinceLast = difftime(nowTime, lastRequestTime);
    if (timeSinceLast < requestInterval) {
        double waitTime = requestInterval - timeSinceLast;
        std::this_thread::sleep_for(std::chrono::milliseconds((int)(waitTime * 1000)));
    }
    lastRequestTime = nowTime;
}

std::string AppWorldsTranslator::urlEncode(const std::string& str) {
    return globalUrlEncode(str);
}

std::string AppWorldsTranslator::httpPost(const std::string& url, const std::string& data) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;
    
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        
        // Set request headers
        struct curl_slist* headerList = nullptr;
        for (const auto& header : headers) {
            std::string headerStr = header.first + ": " + header.second;
            headerList = curl_slist_append(headerList, headerStr.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
        
        res = curl_easy_perform(curl);
        curl_slist_free_all(headerList);
        curl_easy_cleanup(curl);
    }
    
    return readBuffer;
}

TranslationResult AppWorldsTranslator::translate(const std::string& text, 
                                               const std::string& fromLang, 
                                               const std::string& toLang) {
    TranslationResult result;
    result.original = text;
    result.fromLang = fromLang;
    result.toLang = toLang;
    
    if (text.empty()) {
        result.error = "Text cannot be empty";
        return result;
    }
    
    if (text.length() > 255) {
        result.error = "Text exceeds 255 character limit, length: " + std::to_string(text.length());
        return result;
    }
    
    waitForRateLimit();
    
    try {
        // Build POST data
        std::string postData = "text=" + urlEncode(text) + 
                              "&from=" + urlEncode(fromLang) + 
                              "&to=" + urlEncode(toLang);
        
        std::string response = httpPost(baseUrl, postData);
        
        if (!response.empty()) {
            try {
                json jsonResponse = json::parse(response);
                result.apiResponse = response;
                
                if (jsonResponse.contains("code") && jsonResponse["code"] == 200) {
                    if (jsonResponse.contains("data")) {
                        result.translated = jsonResponse["data"].get<std::string>();
                        result.success = true;
                    }
                } else {
                    std::string errorMsg = jsonResponse.contains("msg") ? 
                                         jsonResponse["msg"].get<std::string>() : "Unknown error";
                    result.error = "API error: " + errorMsg;
                }
            } catch (const json::exception& e) {
                result.error = "JSON parse failed: " + std::string(e.what());
            }
        } else {
            result.error = "HTTP request failed or empty response";
        }
    } catch (const std::exception& e) {
        result.error = "Request exception: " + std::string(e.what());
    }
    
    return result;
}

std::vector<TranslationResult> AppWorldsTranslator::batchTranslate(
    const std::vector<std::string>& texts,
    const std::string& fromLang,
    const std::string& toLang) {
    
    std::vector<TranslationResult> results;
    
    for (size_t i = 0; i < texts.size(); ++i) {
        TranslationResult result = translate(texts[i], fromLang, toLang);
        results.push_back(result);
    }
    
    return results;
}

bool AppWorldsTranslator::testApi() {
    return true;
}

// SuApi translator implementation
SuApiTranslator::SuApiTranslator() 
    : baseUrl("https://suapi.net/api/text/translate")
    , requestInterval(1.0)
    , lastRequestTime(0) {
    
    headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";
    headers["Accept"] = "application/json, text/plain, */*";
    headers["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8";
}

void SuApiTranslator::waitForRateLimit() {
    auto now = std::chrono::system_clock::now();
    auto nowTime = std::chrono::system_clock::to_time_t(now);
    
    double timeSinceLast = difftime(nowTime, lastRequestTime);
    if (timeSinceLast < requestInterval) {
        double waitTime = requestInterval - timeSinceLast;
        std::this_thread::sleep_for(std::chrono::milliseconds((int)(waitTime * 1000)));
    }
    lastRequestTime = nowTime;
}

std::string SuApiTranslator::urlEncode(const std::string& str) {
    return globalUrlEncode(str);
}

std::string SuApiTranslator::httpGet(const std::string& url) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;
    
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
        
        // Set request headers
        struct curl_slist* headerList = nullptr;
        for (const auto& header : headers) {
            std::string headerStr = header.first + ": " + header.second;
            headerList = curl_slist_append(headerList, headerStr.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
        
        res = curl_easy_perform(curl);
        curl_slist_free_all(headerList);
        curl_easy_cleanup(curl);
    }
    
    return readBuffer;
}

TranslationResult SuApiTranslator::translate(const std::string& text, 
                                           const std::string& fromLang, 
                                           const std::string& toLang) {
    TranslationResult result;
    result.original = text;
    result.fromLang = fromLang;
    result.toLang = toLang;
    
    if (text.empty()) {
        result.error = "Text cannot be empty";
        return result;
    }
    
    waitForRateLimit();
    
    try {
        // Build GET URL
        std::string url = baseUrl + "?to=" + urlEncode(toLang) + "&text[]=" + urlEncode(text);
        
        std::string response = httpGet(url);
        
        if (!response.empty()) {
            try {
                json jsonResponse = json::parse(response);
                result.apiResponse = response;
                
                // Check if array format
                if (jsonResponse.is_array() && !jsonResponse.empty()) {
                    result.translated = jsonResponse[0].get<std::string>();
                    result.success = true;
                }  // Check if object format
                else if (jsonResponse.is_object()) {
                    // Handle SuApi nested JSON format
                    if (jsonResponse.contains("code") && jsonResponse["code"] == 200 && 
                        jsonResponse.contains("data") && jsonResponse["data"].is_array() &&
                        !jsonResponse["data"].empty()) {
                        
                        auto dataArray = jsonResponse["data"];
                        if (dataArray[0].contains("translations") && dataArray[0]["translations"].is_array() &&
                            !dataArray[0]["translations"].empty()) {
                            
                            auto translations = dataArray[0]["translations"];
                            if (translations[0].contains("text")) {
                                result.translated = translations[0]["text"].get<std::string>();
                                result.success = true;
                            } else {
                                result.error = "Missing text field in translation result";
                            }
                        } else {
                            result.error = "Invalid format: missing translations field";
                        }
                    }
                    // Other formats
                    else if (jsonResponse.contains("result")) {
                        result.translated = jsonResponse["result"].get<std::string>();
                        result.success = true;
                    } else if (jsonResponse.contains("data") && jsonResponse["data"].is_string()) {
                        result.translated = jsonResponse["data"].get<std::string>();
                        result.success = true;
                    } else if (jsonResponse.contains("translation")) {
                        result.translated = jsonResponse["translation"].get<std::string>();
                        result.success = true;
                    } else {
                        result.error = "Cannot extract translation from response";
                    }
                } else {
                    result.error = "Unexpected response format";
                }
            } catch (const json::exception& e) {
                // If not JSON, try using text content directly
                if (!response.empty()) {
                    result.translated = response;
                    result.success = true;
                } else {
                    result.error = "JSON parse failed and empty response: " + std::string(e.what());
                }
            }
        } else {
            result.error = "HTTP request failed or empty response";
        }
    } catch (const std::exception& e) {
        result.error = "Request exception: " + std::string(e.what());
    }
    
    return result;
}

std::vector<TranslationResult> SuApiTranslator::batchTranslate(
    const std::vector<std::string>& texts,
    const std::string& fromLang,
    const std::string& toLang) {
    
    std::vector<TranslationResult> results;
    
    // Try batch request
    waitForRateLimit();
    
    try {
        std::string url = baseUrl + "?to=" + urlEncode(toLang);
        for (const auto& text : texts) {
            url += "&text[]=" + urlEncode(text);
        }
        
        std::string response = httpGet(url);
        
        if (!response.empty()) {
            try {
                json jsonResponse = json::parse(response);
                
                if (jsonResponse.is_array()) {
                    // If returns array, match in order
                    for (size_t i = 0; i < texts.size(); ++i) {
                        TranslationResult result;
                        result.original = texts[i];
                        result.fromLang = fromLang;
                        result.toLang = toLang;
                        
                        if (i < jsonResponse.size()) {
                            result.translated = jsonResponse[i].get<std::string>();
                            result.success = true;
                        } else {
                            result.error = "Result count mismatch";
                        }
                        results.push_back(result);
                    }
                    return results;
                }
            } catch (const json::exception& e) {
                // Fall back to individual translation
            }
        }
    } catch (const std::exception& e) {
        // Fall back to individual translation
    }
    
    // Fall back to individual translation
    for (const auto& text : texts) {
        results.push_back(translate(text, fromLang, toLang));
    }
    
    return results;
}

bool SuApiTranslator::testApi() {
    return true;
}

// Translator factory implementation
std::unique_ptr<ITranslator> TranslatorFactory::createTranslator(TranslatorType type) {
    switch (type) {
        case TranslatorType::APP_WORLDS:
            return std::make_unique<AppWorldsTranslator>();
        case TranslatorType::SU_API:
            return std::make_unique<SuApiTranslator>();
        default:
            return nullptr;
    }
}
