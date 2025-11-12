#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQml>
#include <QQuickWindow>
#include <QSGRendererInterface>

#include "PlayerController.h"
#include "PlaylistModel.h"
#include "TrackMetadata.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    // Force software rendering backend to avoid driver/OpenGL issues
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    qputenv("QSG_RHI_BACKEND", QByteArray("software"));
    qputenv("QT_QUICK_BACKEND", QByteArray("software"));
    
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
