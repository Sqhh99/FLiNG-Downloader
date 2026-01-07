#ifndef TRANSLATOR_H
#define TRANSLATOR_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <ctime>

// Forward declaration to avoid circular inclusion
#ifdef QT_CORE_LIB
#include <QString>
#include <QStringList>
#endif

// Translation result structure
struct TranslationResult {
    bool success;
    std::string original;
    std::string translated;
    std::string fromLang;
    std::string toLang;
    std::string error;
    std::string apiResponse;
    
    TranslationResult() : success(false) {}
};

// Base translator interface
class ITranslator {
public:
    virtual ~ITranslator() = default;
    virtual TranslationResult translate(const std::string& text, 
                                      const std::string& fromLang = "auto", 
                                      const std::string& toLang = "en") = 0;
    virtual std::vector<TranslationResult> batchTranslate(const std::vector<std::string>& texts,
                                                        const std::string& fromLang = "auto",
                                                        const std::string& toLang = "en") = 0;
    virtual bool testApi() = 0;
};

// AppWorlds translator
class AppWorldsTranslator : public ITranslator {
private:
    std::string baseUrl;
    std::map<std::string, std::string> headers;
    double requestInterval;
    std::time_t lastRequestTime;  // Use std::time_t instead of time_t
    
    void waitForRateLimit();
    std::string urlEncode(const std::string& str);
    std::string httpPost(const std::string& url, const std::string& data);
    
public:
    AppWorldsTranslator();
    ~AppWorldsTranslator() override = default;
    
    TranslationResult translate(const std::string& text, 
                              const std::string& fromLang = "zh-CN", 
                              const std::string& toLang = "en") override;
    
    std::vector<TranslationResult> batchTranslate(const std::vector<std::string>& texts,
                                                const std::string& fromLang = "zh-CN",
                                                const std::string& toLang = "en") override;
    
    bool testApi() override;
};

// SuApi translator
class SuApiTranslator : public ITranslator {
private:
    std::string baseUrl;
    std::map<std::string, std::string> headers;
    double requestInterval;
    std::time_t lastRequestTime;  // Use std::time_t instead of time_t
    
    void waitForRateLimit();
    std::string urlEncode(const std::string& str);
    std::string httpGet(const std::string& url);
    
public:
    SuApiTranslator();
    ~SuApiTranslator() override = default;
    
    TranslationResult translate(const std::string& text, 
                              const std::string& fromLang = "auto", 
                              const std::string& toLang = "en") override;
    
    std::vector<TranslationResult> batchTranslate(const std::vector<std::string>& texts,
                                                const std::string& fromLang = "auto",
                                                const std::string& toLang = "en") override;
    
    bool testApi() override;
};

// Translator factory
class TranslatorFactory {
public:
    enum class TranslatorType {
        APP_WORLDS,
        SU_API
    };
    
    static std::unique_ptr<ITranslator> createTranslator(TranslatorType type);
};

#endif // TRANSLATOR_H
