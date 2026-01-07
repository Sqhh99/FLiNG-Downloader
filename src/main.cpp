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
    // 使用 QGuiApplication (纯 QML 应用)
    QGuiApplication app(argc, argv);
    
    try {
        qDebug() << "应用程序开始初始化...";
        
        // 初始化资源系统
        Q_INIT_RESOURCE(resources);
        Q_INIT_RESOURCE(qml);
        qDebug() << "✅ 资源系统已初始化";
        
        // 验证资源系统
        QDir resourceRoot(":/");
        if (resourceRoot.exists()) {
            QStringList entries = resourceRoot.entryList();
            qDebug() << "📁 可用资源目录:" << entries;
        } else {
            qDebug() << "❌ 资源系统初始化失败!";
        }
        
        // 应用当前语言
        LanguageManager::getInstance().applyCurrentLanguage(app);
        
        // 设置 QuickControls2 样式
        QQuickStyle::setStyle("Basic");
        
        // 初始化游戏名映射管理器
        qDebug() << "正在初始化游戏名映射管理器...";
        if (GameMappingManager::getInstance().initialize()) {
            qDebug() << "✅ 游戏名映射管理器初始化成功";
        } else {
            qDebug() << "⚠️ 游戏名映射管理器初始化失败，中文搜索功能可能受限";
        }
        
        // 注册 QML 类型
        qmlRegisterType<ModifierListModel>("DownloadIntegrator", 1, 0, "ModifierListModel");
        qmlRegisterType<DownloadedModifierModel>("DownloadIntegrator", 1, 0, "DownloadedModifierModel");
        
        // 创建 Backend 实例
        Backend* backend = new Backend(&app);
        backend->setApplication(&app);
        
        // 创建 QML 引擎
        QQmlApplicationEngine engine;
        
        // 设置 QQmlEngine 引用（用于语言切换时刷新 QML）
        backend->setQmlEngine(&engine);
        
        // 添加 QML 导入路径
        engine.addImportPath("qrc:/qml");
        
        // 将主题索引暴露到 QML (用于初始化 ThemeProvider)
        int currentTheme = static_cast<int>(ConfigManager::getInstance().getCurrentTheme());
        engine.rootContext()->setContextProperty("initialTheme", currentTheme);
        
        // 加载主 QML 文件，使用 setInitialProperties 绑定 required property
        const QUrl url(QStringLiteral("qrc:/qml/Main.qml"));
        
        // 设置 required property 的初始值
        engine.setInitialProperties({{"backend", QVariant::fromValue(backend)}});
        
        QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                         &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl) {
                qDebug() << "❌ QML 加载失败!";
                QCoreApplication::exit(-1);
            }
        }, Qt::QueuedConnection);
        
        engine.load(url);
        
        if (engine.rootObjects().isEmpty()) {
            qDebug() << "❌ 没有 QML 对象被加载";
            return -1;
        }
        
        qDebug() << "✅ 应用程序初始化完成，启动事件循环";
        
        // 运行应用程序
        return app.exec();
    } 
    catch (const std::exception& e) {
        qDebug() << "发生异常:" << e.what();
        return 1;
    } 
    catch (...) {
        qDebug() << "发生未知异常";
        return 1;
    }
}
