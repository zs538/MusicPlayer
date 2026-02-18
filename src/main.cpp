#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include "CoverImageProvider.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    
    QCoreApplication::setOrganizationName("MusicPlayer");
    QCoreApplication::setApplicationName("MusicPlayer-");
    
    QQuickStyle::setStyle("Fusion");
    
    QQmlApplicationEngine engine;
    
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
