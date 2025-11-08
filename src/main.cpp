#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQml>

#include "PlayerController.h"
#include "PlaylistModel.h"
#include "TrackMetadata.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    
    // Register TrackMetadata for QML
    qmlRegisterType<TrackMetadata>("MusicPlayer", 1, 0, "TrackMetadata");

    PlayerController controller;
    PlaylistModel playlist;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("player", &controller);
    engine.rootContext()->setContextProperty("playlist", &playlist);

    const QUrl url(QStringLiteral("qrc:/qt/qml/MusicPlayer/qml/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
