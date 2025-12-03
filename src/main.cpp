#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQml>
#include <QQuickWindow>
#include <QSGRendererInterface>

#include "PlayerController.h"
#include "PlaylistModel.h"
#include "PlaylistManager.h"
#include "TrackMetadata.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    
    // Set application info for QStandardPaths
    app.setOrganizationName("MusicPlayer");
    app.setApplicationName("MusicPlayer");
    
    // Force software rendering backend to avoid driver/OpenGL issues
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    qputenv("QSG_RHI_BACKEND", QByteArray("software"));
    qputenv("QT_QUICK_BACKEND", QByteArray("software"));
    
    // Register TrackMetadata for QML
    qmlRegisterType<TrackMetadata>("MusicPlayer", 1, 0, "TrackMetadata");
    qmlRegisterUncreatableType<PlaylistModel>("MusicPlayer", 1, 0, "PlaylistModel",
        "PlaylistModel is created by PlaylistManager");

    // Create playlist manager and player controller
    PlaylistManager playlistManager;
    playlistManager.loadSession();
    
    PlayerController controller;
    controller.setPlaylistManager(&playlistManager);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("player", &controller);
    engine.rootContext()->setContextProperty("playlistManager", &playlistManager);
    // Expose the displayed playlist for backward compatibility
    engine.rootContext()->setContextProperty("playlist", playlistManager.displayedPlaylist());
    
    // Update playlist context property when displayed playlist changes
    QObject::connect(&playlistManager, &PlaylistManager::displayedPlaylistChanged,
                     [&engine, &playlistManager](const QString&) {
        engine.rootContext()->setContextProperty("playlist", playlistManager.displayedPlaylist());
    });
    
    // Save session on quit
    QObject::connect(&app, &QGuiApplication::aboutToQuit, [&playlistManager]() {
        playlistManager.saveSession();
    });

    const QUrl url(QStringLiteral("qrc:/qt/qml/MusicPlayer/qml/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
