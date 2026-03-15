#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include "CoverImageProvider.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    
    // Disable debug logging in release builds
    #ifndef QT_DEBUG
    QLoggingCategory::setFilterRules("*.debug=false\n*.info=false\nmusicplayer.*.warning=true\nmusicplayer.*.critical=true");
    #endif
    
    QCoreApplication::setOrganizationName("MusicPlayer");
    QCoreApplication::setApplicationName("MusicPlayer");
    QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    
    QQuickStyle::setStyle("Fusion");
    app.setWindowIcon(QIcon(QStringLiteral(":/qt/qml/MusicPlayer/logo-icon-256.png")));
    
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("AppVersion", QCoreApplication::applicationVersion());
    
    // Register cover image provider
    auto *coverProvider = new CoverImageProvider();
    CoverImageProvider::setInstance(coverProvider);
    engine.addImageProvider("cover", coverProvider);
    
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    
    engine.loadFromModule("MusicPlayer", "Main");
    
    return app.exec();
}
