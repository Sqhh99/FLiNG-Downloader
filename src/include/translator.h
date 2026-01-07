#ifndef TRANSLATOR_H
#define TRANSLATOR_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <ctime>

// 前向声明以避免循环包含
#ifdef QT_CORE_LIB
#include <QString>
#include <QStringList>
#endif

// 翻译结果结构体
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

// 基础翻译器接口
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

// AppWorlds翻译器
class AppWorldsTranslator : public ITranslator {
private:
    std::string baseUrl;
    std::map<std::string, std::string> headers;
    double requestInterval;
    std::time_t lastRequestTime;  // 使用std::time_t替代time_t
    
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

// SuApi翻译器
class SuApiTranslator : public ITranslator {
private:
    std::string baseUrl;
    std::map<std::string, std::string> headers;
    double requestInterval;
    std::time_t lastRequestTime;  // 使用std::time_t替代time_t
    
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

// 翻译器工厂
class TranslatorFactory {
public:
    enum class TranslatorType {
        APP_WORLDS,
        SU_API
    };
    
    static std::unique_ptr<ITranslator> createTranslator(TranslatorType type);
};

#endif // TRANSLATOR_H
