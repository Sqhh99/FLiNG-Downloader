#include "ConfigManager.h"
#include <QDir>
#include <QDebug>

namespace {
constexpr const char* kDefaultUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";
}

ConfigManager::ConfigManager()
    : m_settings(nullptr)
{
    const QString path = settingsFilePath();
    m_settings = new QSettings(path, QSettings::IniFormat);
    qDebug() << "Config file path:" << m_settings->fileName();
}

ConfigManager::~ConfigManager()
{
    delete m_settings;
    m_settings = nullptr;
}

QString ConfigManager::settingsFilePath() const
{
    const QString configDir = FileSystem::getInstance().getConfigDirectory();
    return QDir(configDir).filePath("settings.ini");
}

QString ConfigManager::getDownloadDirectory() const
{
    return m_settings->value("downloadDirectory",
                             FileSystem::getInstance().getDownloadDirectory()).toString();
}

void ConfigManager::setDownloadDirectory(const QString& directory)
{
    if (FileSystem::getInstance().ensureDirectoryExists(directory)) {
        m_settings->setValue("downloadDirectory", directory);
        m_settings->sync();
        qDebug() << "Download directory set to:" << directory;
    } else {
        qDebug() << "Cannot set download directory, path invalid:" << directory;
    }
}

bool ConfigManager::getUseCustomDownloadPath() const
{
    return m_settings->value("useCustomDownloadPath", false).toBool();
}

void ConfigManager::setUseCustomDownloadPath(bool useCustomPath)
{
    m_settings->setValue("useCustomDownloadPath", useCustomPath);
    m_settings->sync();
}

QString ConfigManager::getCustomDownloadPath() const
{
    return m_settings->value("customDownloadPath",
                             FileSystem::getInstance().getDownloadDirectory()).toString();
}

void ConfigManager::setCustomDownloadPath(const QString& path)
{
    m_settings->setValue("customDownloadPath", path);
    m_settings->sync();
}

bool ConfigManager::getUseMockDownload() const
{
    return m_settings->value("useMockDownload", false).toBool();
}

void ConfigManager::setUseMockDownload(bool useMock)
{
    m_settings->setValue("useMockDownload", useMock);
    m_settings->sync();
}

QString ConfigManager::getUserAgent() const
{
    return m_settings->value("userAgent", kDefaultUserAgent).toString();
}

void ConfigManager::setUserAgent(const QString& userAgent)
{
    m_settings->setValue("userAgent", userAgent);
    m_settings->sync();
}

int ConfigManager::getNetworkTimeout() const
{
    return m_settings->value("networkTimeout", 30000).toInt();
}

void ConfigManager::setNetworkTimeout(int timeout)
{
    if (timeout >= 1000) {
        m_settings->setValue("networkTimeout", timeout);
        m_settings->sync();
    }
}

bool ConfigManager::getAutoCheckUpdates() const
{
    return m_settings->value("autoCheckUpdates", true).toBool();
}

void ConfigManager::setAutoCheckUpdates(bool autoCheck)
{
    m_settings->setValue("autoCheckUpdates", autoCheck);
    m_settings->sync();
}

ConfigManager::Theme ConfigManager::getCurrentTheme() const
{
    const int themeValue = m_settings->value(
        "currentTheme", static_cast<int>(Theme::Default)).toInt();
    return static_cast<Theme>(themeValue);
}

void ConfigManager::setCurrentTheme(Theme theme)
{
    m_settings->setValue("currentTheme", static_cast<int>(theme));
    m_settings->sync();
    qDebug() << "Theme set to:" << static_cast<int>(theme);
}

ConfigManager::Language ConfigManager::getCurrentLanguage() const
{
    const int langValue = m_settings->value(
        "currentLanguage", static_cast<int>(Language::Default)).toInt();
    return static_cast<Language>(langValue);
}

void ConfigManager::setCurrentLanguage(Language language)
{
    m_settings->setValue("currentLanguage", static_cast<int>(language));
    m_settings->sync();
    qDebug() << "Language set to:" << static_cast<int>(language);
}

void ConfigManager::resetToDefaults()
{
    m_settings->clear();
    m_settings->sync();
    qDebug() << "Settings reset to defaults";
}
