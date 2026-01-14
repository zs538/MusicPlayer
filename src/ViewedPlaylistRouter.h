#ifndef VIEWEDPLAYLISTROUTER_H
#define VIEWEDPLAYLISTROUTER_H

#include <QObject>
#include <QQmlEngine>

class PlaylistStore;
class AppViewModel;
class QAbstractItemModel;

/**
 * @brief ViewedPlaylistRouter is the single authority for "where UI playlist edits go".
 * 
 * It acts as a compatibility layer around the existing PlaylistStore.displayedPlaylistId.
 * QML should never directly write PlaylistStore.displayedPlaylistId - it should use this router.
 */
class ViewedPlaylistRouter : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString viewedPlaylistId READ viewedPlaylistId WRITE setViewedPlaylistId NOTIFY viewedPlaylistIdChanged)
    Q_PROPERTY(QAbstractItemModel* viewedPlaylistModel READ viewedPlaylistModel NOTIFY viewedPlaylistModelChanged)
    Q_PROPERTY(QString activePlaylistId READ activePlaylistId NOTIFY activePlaylistIdChanged)
    Q_PROPERTY(QAbstractItemModel* activePlaylistModel READ activePlaylistModel NOTIFY activePlaylistModelChanged)

public:
    explicit ViewedPlaylistRouter(QObject *parent = nullptr);

    static ViewedPlaylistRouter *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
    static ViewedPlaylistRouter *instance();

    void initialize(PlaylistStore *store, AppViewModel *app);

    QString viewedPlaylistId() const;
    void setViewedPlaylistId(const QString &id);
    QAbstractItemModel *viewedPlaylistModel() const;

    QString activePlaylistId() const;
    QAbstractItemModel *activePlaylistModel() const;

    Q_INVOKABLE void setActiveToViewed();
    Q_INVOKABLE bool hasViewedPlaylist() const;

signals:
    void viewedPlaylistIdChanged();
    void viewedPlaylistModelChanged();
    void activePlaylistIdChanged();
    void activePlaylistModelChanged();

private:
    PlaylistStore *store() const;
    void connectToStore(PlaylistStore *s);
    
    static ViewedPlaylistRouter *s_instance;
    PlaylistStore *m_store = nullptr;
    AppViewModel *m_app = nullptr;
};

#endif // VIEWEDPLAYLISTROUTER_H
