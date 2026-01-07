#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QDebug>
#include <QDir>
#include <QTranslator>

#include "Backend.h"
#include "ModifierListModel.h"
#include "DownloadedModifierModel.h"
#include "ThemeManager.h"
#include "LanguageManager.h"
#include "GameMappingManager.h"
#include "ConfigManager.h"

int main(int argc, char *argv[])
{
    // Use QGuiApplication (pure QML app)
    QGuiApplication app(argc, argv);
    
    try {
        qDebug() << "Application initializing...";
        
        // Initialize resource system
        Q_INIT_RESOURCE(resources);
        Q_INIT_RESOURCE(qml);
        qDebug() << "Resources initialized";
        
        // Verify resource system
        QDir resourceRoot(":/");
        if (resourceRoot.exists()) {
            QStringList entries = resourceRoot.entryList();
            qDebug() << "Available resource dirs:" << entries;
        } else {
            qDebug() << "Resource system initialization failed";
        }
        
        // Apply current language
        LanguageManager::getInstance().applyCurrentLanguage(app);
        
        // Set QuickControls2 style
        QQuickStyle::setStyle("Basic");
        
        // Initialize game mapping manager
        qDebug() << "Initializing game mapping manager...";
        if (GameMappingManager::getInstance().initialize()) {
            qDebug() << "Game mapping manager initialized";
        } else {
            qDebug() << "Game mapping manager failed, Chinese search may be limited";
        }
        
        // Register QML types
        qmlRegisterType<ModifierListModel>("DownloadIntegrator", 1, 0, "ModifierListModel");
        qmlRegisterType<DownloadedModifierModel>("DownloadIntegrator", 1, 0, "DownloadedModifierModel");
        
        // Create Backend instance
        Backend* backend = new Backend(&app);
        backend->setApplication(&app);
        
        // Create QML engine
        QQmlApplicationEngine engine;
        
        // Set QQmlEngine reference (for language switch refresh)
        backend->setQmlEngine(&engine);
        
        // Add QML import path
        engine.addImportPath("qrc:/qml");
        
        // Expose theme index to QML (for ThemeProvider initialization)
        int currentTheme = static_cast<int>(ConfigManager::getInstance().getCurrentTheme());
        engine.rootContext()->setContextProperty("initialTheme", currentTheme);
        
        // Load main QML file with setInitialProperties for required property
        const QUrl url(QStringLiteral("qrc:/qml/Main.qml"));
        
        // Set required property initial values
        engine.setInitialProperties({{"backend", QVariant::fromValue(backend)}});
        
        QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                         &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl) {
                qDebug() << "QML load failed";
                QCoreApplication::exit(-1);
            }
        }, Qt::QueuedConnection);
        
        engine.load(url);
        
        if (engine.rootObjects().isEmpty()) {
            qDebug() << "No QML objects loaded";
            return -1;
        }
        
        qDebug() << "Application initialized, starting event loop";
        
        // Run application
        return app.exec();
    } 
    catch (const std::exception& e) {
        qDebug() << "Exception:" << e.what();
        return 1;
    } 
    catch (...) {
        qDebug() << "Unknown exception";
        return 1;
    }
}
