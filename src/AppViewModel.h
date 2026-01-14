#ifndef APPVIEWMODEL_H
#define APPVIEWMODEL_H

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QUrl>
#include "TrackListModel.h"
#include "PlaylistStore.h"
#include "PlaylistTabsModel.h"
#include "FileBrowserModel.h"
#include "library/LibraryTreeModel.h"
#include "CoverArtProvider.h"
#include "BrowseActivationService.h"

class QueueManager;
class AudioEngine;
class LibraryDatabase;
class LibraryScanner;

class AppViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    
    Q_PROPERTY(int playbackState READ playbackState NOTIFY playbackStateChanged)
    Q_PROPERTY(qint64 positionMs READ positionMs NOTIFY positionMsChanged)
    Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY durationMsChanged)
    Q_PROPERTY(QString nowPlayingTitle READ nowPlayingTitle NOTIFY nowPlayingChanged)
    Q_PROPERTY(QString nowPlayingArtist READ nowPlayingArtist NOTIFY nowPlayingChanged)
    Q_PROPERTY(bool hasError READ hasError NOTIFY hasErrorChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(QAbstractItemModel* activePlaylistModel READ activePlaylistModel NOTIFY activePlaylistModelChanged)
    Q_PROPERTY(QAbstractItemModel* displayedPlaylistModel READ displayedPlaylistModel NOTIFY displayedPlaylistModelChanged)
    Q_PROPERTY(QAbstractItemModel* playlistTabsModel READ playlistTabsModel CONSTANT)
    Q_PROPERTY(PlaylistStore* playlistStore READ playlistStore CONSTANT)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(LibraryTreeModel* libraryTreeModel READ libraryTreeModel CONSTANT)
    Q_PROPERTY(FileBrowserModel* fileBrowserModel READ fileBrowserModel CONSTANT)
    Q_PROPERTY(bool libraryScanning READ libraryScanning NOTIFY libraryScanningChanged)
    Q_PROPERTY(int libraryScanProgress READ libraryScanProgress NOTIFY libraryScanProgressChanged)
    Q_PROPERTY(QStringList watchFolders READ watchFolders NOTIFY libraryFoldersChanged)
    Q_PROPERTY(int libraryTrackCount READ libraryTrackCount NOTIFY libraryTrackCountChanged)
    Q_PROPERTY(qint64 lastScanTime READ lastScanTime NOTIFY lastScanTimeChanged)
    Q_PROPERTY(QString nowPlayingCoverUrl READ nowPlayingCoverUrl NOTIFY nowPlayingChanged)
    Q_PROPERTY(BrowseActivationService* browseActivation READ browseActivation CONSTANT)
    Q_PROPERTY(LibraryDatabase* libraryDatabase READ libraryDatabase CONSTANT)

public:
    enum PlaybackState {
        Stopped = 0,
        Playing,
        Paused,
        Buffering,
        Error
    };
    Q_ENUM(PlaybackState)

    explicit AppViewModel(QObject *parent = nullptr);
    
    static AppViewModel *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
    
    int playbackState() const { return m_playbackState; }
    qint64 positionMs() const { return m_positionMs; }
    qint64 durationMs() const { return m_durationMs; }
    QString nowPlayingTitle() const { return m_nowPlayingTitle; }
    QString nowPlayingArtist() const { return m_nowPlayingArtist; }
    QString nowPlayingCoverUrl() const { return m_nowPlayingCoverUrl; }
    bool hasError() const { return m_hasError; }
    QString errorText() const { return m_errorText; }
    TrackListModel *activePlaylistModel() const;
    TrackListModel *displayedPlaylistModel() const;
    QAbstractItemModel *playlistTabsModel() const;
    PlaylistStore *playlistStore() const;
    int currentIndex() const { return m_currentIndex; }
    LibraryTreeModel *libraryTreeModel() const;
    FileBrowserModel *fileBrowserModel() const;
    bool libraryScanning() const;
    int libraryScanProgress() const;
    QStringList watchFolders() const;
    int libraryTrackCount() const;
    qint64 lastScanTime() const;
    BrowseActivationService *browseActivation() const;
    LibraryDatabase *libraryDatabase() const { return m_libraryDb; }
    
    static AppViewModel *instance();

public slots:
    void play();
    void pause();
    void stop();
    void next();
    void previous();
    void seek(qint64 positionMs);
    void playIndex(int row);
    void setVolume(double value);
    void addFilesToPlaylist(const QList<QUrl> &urls);
    void removeFromPlaylist(int index, int count = 1);
    void movePlaylistRow(int from, int to);
    void clearPlaylist();
    void addLibraryFolder(const QString &path);
    void removeLibraryFolder(const QString &path);
    void rescanLibrary();
    void addLibraryTracksToPlaylist(const QVariantList &tracks);
    QStringList libraryFolders() const;
    void addWatchFolder(const QString &path);
    void removeWatchFolder(const QString &path);

signals:
    void playbackStateChanged();
    void positionMsChanged();
    void durationMsChanged();
    void nowPlayingChanged();
    void hasErrorChanged();
    void errorTextChanged();
    void currentIndexChanged();
    void libraryScanningChanged();
    void libraryScanProgressChanged();
    void activePlaylistModelChanged();
    void displayedPlaylistModelChanged();
    void libraryFoldersChanged();
    void libraryTrackCountChanged();
    void lastScanTimeChanged();


private:
    void setPlaybackState(PlaybackState state);
    void setError(const QString &text);
    void clearError();
    void updateNowPlaying(const TrackInfo &track);
    
    PlaybackState m_playbackState = Stopped;
    qint64 m_positionMs = 0;
    qint64 m_durationMs = 180000;
    QString m_nowPlayingTitle = "No track";
    QString m_nowPlayingArtist = "";
    QString m_nowPlayingCoverUrl;
    bool m_hasError = false;
    QString m_errorText;
    double m_volume = 1.0;
    int m_currentIndex = -1;
    PlaylistStore *m_playlistStore = nullptr;
    PlaylistTabsModel *m_playlistTabsModel = nullptr;
    QueueManager *m_queueManager = nullptr;
    AudioEngine *m_audioEngine = nullptr;
    LibraryDatabase *m_libraryDb = nullptr;
    LibraryScanner *m_libraryScanner = nullptr;
    LibraryTreeModel *m_libraryTreeModel = nullptr;
    FileBrowserModel *m_fileBrowserModel = nullptr;
    BrowseActivationService *m_browseActivation = nullptr;
};

#endif // APPVIEWMODEL_H
