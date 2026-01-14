#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include "CoverImageProvider.h"
#include "AppViewModel.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    
    QCoreApplication::setOrganizationName("MusicPlayer");
    QCoreApplication::setApplicationName("MusicPlayer-");
    
    QQuickStyle::setStyle("Fusion");
    
    QQmlApplicationEngine engine;
    
    // Register cover image provider (with null db initially, will be set later)
    auto *coverProvider = new CoverImageProvider(nullptr);
    CoverImageProvider::setInstance(coverProvider);
    engine.addImageProvider("cover", coverProvider);
    
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    
    // After QML loads, the AppViewModel singleton will be created
    // We can then update the cover provider with the database
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, [coverProvider]() {
        if (AppViewModel::instance()) {
            coverProvider->setDatabase(AppViewModel::instance()->libraryDatabase());
        }
    });
    
    engine.loadFromModule("MusicPlayer", "Main");
    
    return app.exec();
}
