#ifndef PLAYLISTSTORE_H
#define PLAYLISTSTORE_H

#include <QObject>
#include <QUuid>
#include <QUrl>
#include <QVector>
#include <QFuture>

class TrackListModel;

/**
 * @brief PlaylistStore owns all playlist data and implements operations.
 * 
 * This is the single authority for playlist state. It owns TrackListModel instances,
 * manages tab metadata (uuid, name), and handles active/displayed playlist routing.
 * 
 * PlaylistTabsModel is a separate UI model that reads from this store.
 */
class PlaylistStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int tabCount READ tabCount NOTIFY tabCountChanged)
    Q_PROPERTY(QString displayedPlaylistId READ displayedPlaylistIdString WRITE setDisplayedPlaylistIdString NOTIFY displayedPlaylistChanged)
    Q_PROPERTY(QString activePlaylistId READ activePlaylistIdString NOTIFY activePlaylistChanged)
    Q_PROPERTY(int displayedIndex READ displayedIndex NOTIFY displayedPlaylistChanged)

public:
    struct Tab {
        QUuid uuid;
        QString name;
        TrackListModel *model = nullptr;
        bool isUserCreated = true;
    };

    explicit PlaylistStore(QObject *parent = nullptr);
    ~PlaylistStore();

    // Tab management
    Q_INVOKABLE QString createNewTab(const QString &name = QString(), bool isUserCreated = true);
    Q_INVOKABLE bool closeTab(const QString &uuid);
    Q_INVOKABLE bool renameTab(const QString &uuid, const QString &newName);
    Q_INVOKABLE bool moveTab(int fromIndex, int toIndex);

    // Active/displayed playlist
    int tabCount() const { return m_tabs.size(); }
    QUuid activePlaylistId() const { return m_activeId; }
    QUuid displayedPlaylistId() const { return m_displayedId; }
    QString activePlaylistIdString() const { return m_activeId.toString(); }
    QString displayedPlaylistIdString() const { return m_displayedId.toString(); }
    void setDisplayedPlaylistIdString(const QString &uuid);
    int displayedIndex() const;

    Q_INVOKABLE void setActivePlaylist(const QString &uuid);
    Q_INVOKABLE void setDisplayedPlaylist(const QString &uuid);

    // Playlist access
    TrackListModel *activePlaylist() const;
    TrackListModel *displayedPlaylist() const;
    TrackListModel *getPlaylistModel(const QString &uuid) const;
    TrackListModel *playlistModel(int index) const;
    QUuid tabUuid(int index) const;
    Q_INVOKABLE QString tabName(int index) const;
    Q_INVOKABLE bool tabIsUserCreated(int index) const;
    
    // Generated playlist management
    Q_INVOKABLE QString findGeneratedPlaylistByName(const QString &name) const;
    Q_INVOKABLE QString getOrCreateGeneratedPlaylist(const QString &name = QString());
    Q_INVOKABLE int generatedPlaylistCount() const;
    Q_INVOKABLE bool setPlaylistUserCreated(const QString &uuid, bool isUserCreated);
    
public slots:
    void enforceGeneratedPlaylistCount();

    // Tab data access (for PlaylistTabsModel)
    const QVector<Tab> &tabs() const { return m_tabs; }
    int indexOfUuid(const QUuid &uuid) const;
    Q_INVOKABLE int indexOfUuid(const QString &uuid) const { return indexOfUuid(QUuid(uuid)); }

    // Import/export
    Q_INVOKABLE QString importPlaylist(const QString &filePath);
    Q_INVOKABLE QString importPlaylistAsync(const QString &filePath);
    Q_INVOKABLE bool exportPlaylist(const QString &uuid, const QString &filePath);

signals:
    void tabCountChanged();
    void activePlaylistChanged(const QUuid &uuid);
    void displayedPlaylistChanged(const QUuid &uuid);
    void playlistChanged();
    
    // Signals for PlaylistTabsModel to observe
    void tabInserted(int index);
    void tabRemoved(int index);
    void tabMoved(int from, int to);
    void tabDataChanged(int index);
    
    // Import progress signals
    void importProgress(const QString &uuid, int imported, int total);
    void importFinished(const QString &uuid, bool success);

private:
    bool importM3U(TrackListModel *model, const QString &filePath, bool utf8);
    bool exportM3U(TrackListModel *model, const QString &filePath, bool utf8);
    void importM3UAsync(const QString &uuid, const QString &filePath, bool utf8);

    QVector<Tab> m_tabs;
    QFuture<void> m_importFuture;
    QUuid m_activeId;
    QUuid m_displayedId;
};

#endif // PLAYLISTSTORE_H
