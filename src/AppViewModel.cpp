#include "AppViewModel.h"
#include "PlaylistStore.h"
#include "PlaylistTabsModel.h"
#include "TrackListModel.h"
#include "QueueManager.h"
#include "SessionManager.h"
#include "Settings.h"
#include "MetadataExtractor.h"
#include "audio/AudioEngine.h"
#include "library/LibraryDatabase.h"
#include "library/LibraryScanner.h"
#include "library/LibraryWatcher.h"
#include "ViewedPlaylistRouter.h"
#include "BrowseActivationService.h"
#include <QFileInfo>

static AppViewModel *s_instance = nullptr;

AppViewModel::AppViewModel(QObject *parent)
    : QObject(parent)
    , m_playlistStore(new PlaylistStore(this))
    , m_playlistTabsModel(new PlaylistTabsModel(m_playlistStore, this))
    , m_queueManager(new QueueManager(this))
    , m_audioEngine(new AudioEngine(this))
    , m_libraryDb(new LibraryDatabase(this))
    , m_libraryScanner(nullptr)
{
    s_instance = this;

    m_fileBrowserModel = new FileBrowserModel(this);
    
    m_libraryDb->open();
    m_libraryScanner = new LibraryScanner(m_libraryDb, this);
    m_libraryWatcher = new LibraryWatcher(m_libraryScanner, this);
    // Initialize watcher with current folders (settings will be applied when Settings singleton is created)
    m_libraryWatcher->setWatchFolders(m_libraryDb->watchFolders());
    
    connect(m_libraryScanner, &LibraryScanner::scanningChanged, this, &AppViewModel::libraryScanningChanged);
    connect(m_libraryScanner, &LibraryScanner::progressChanged, this, &AppViewModel::libraryScanProgressChanged);
    
    // Defer Settings connection until Settings singleton exists
    QTimer::singleShot(0, this, [this]() {
        Settings *settings = Settings::instance();
        if (!settings) {
            qWarning() << "AppViewModel: Settings not available for watcher initialization";
            return;
        }
        
        // Apply initial settings to watcher
        m_libraryWatcher->setEnabled(settings->watcherEnabled());
        m_libraryWatcher->setPeriodicRescanMinutes(settings->periodicRescanMinutes());
        
        // Connect Settings changes to watcher
        connect(settings, &Settings::watcherEnabledChanged, this, [this]() {
            if (m_libraryWatcher && Settings::instance()) {
                m_libraryWatcher->setEnabled(Settings::instance()->watcherEnabled());
            }
        });
        connect(settings, &Settings::periodicRescanMinutesChanged, this, [this]() {
            if (m_libraryWatcher && Settings::instance()) {
                m_libraryWatcher->setPeriodicRescanMinutes(Settings::instance()->periodicRescanMinutes());
            }
        });
        
        qDebug() << "LibraryWatcher: Settings applied - enabled:" << settings->watcherEnabled()
                 << ", periodic:" << settings->periodicRescanMinutes() << "min";
    });
    connect(m_libraryScanner, &LibraryScanner::scanFinished, this, [this]() {
        // Notify database changed so all models (CollectionBrowseModel, etc.) refresh
        if (m_libraryDb) {
            m_libraryDb->notifyDatabaseChanged();
        }
        emit libraryTrackCountChanged();

        // Refresh playlist tracks' metadata from the library database
        TrackListModel *playlist = m_playlistStore ? m_playlistStore->displayedPlaylist() : nullptr;
        if (playlist && m_libraryDb) {
            for (int i = 0; i < playlist->count(); ++i) {
                TrackInfo t = playlist->trackAt(i);
                if (t.filePath.isEmpty())
                    continue;

                auto libOpt = m_libraryDb->trackByPath(t.filePath);
                if (!libOpt.has_value())
                    continue;

                playlist->updateTrackMetadata(i, MetadataExtractor::toTrackInfo(*libOpt));
            }
        }
    });
    
    m_queueManager->setPlaylistModel(m_playlistStore->activePlaylist());
    
    connect(m_playlistStore, &PlaylistStore::activePlaylistChanged, this, [this](const QUuid &uuid) {
        Q_UNUSED(uuid)
        m_queueManager->setPlaylistModel(m_playlistStore->activePlaylist());
        emit activePlaylistModelChanged();
    });
    
    connect(m_playlistStore, &PlaylistStore::displayedPlaylistChanged, this, [this](const QUuid &uuid) {
        Q_UNUSED(uuid)
        emit displayedPlaylistModelChanged();
    });
    
    connect(m_queueManager, &QueueManager::currentIndexChanged, this, [this](int index) {
        m_currentIndex = index;
        emit currentIndexChanged();
    });
    
    // Ensure CoverArtProvider is created
    if (!CoverArtProvider::instance()) {
        new CoverArtProvider(this);
    }
    
    // Ensure Settings is created
    Settings *settings = Settings::instance();
    if (!settings) {
        settings = new Settings(this);
    }
    
    // Connect Settings to AudioEngine
    if (settings && m_audioEngine) {
        m_audioEngine->setPlaybackMode(static_cast<AudioEngine::PlaybackMode>(settings->playbackMode()));
        m_audioEngine->setSinkBufferMs(settings->bufferSizeMs());
        
        connect(settings, &Settings::playbackModeChanged, this, [this]() {
            Settings *s = Settings::instance();
            if (s && m_audioEngine) {
                m_audioEngine->setPlaybackMode(static_cast<AudioEngine::PlaybackMode>(s->playbackMode()));
            }
        });
        connect(settings, &Settings::bufferSizeMsChanged, this, [this]() {
            Settings *s = Settings::instance();
            if (s && m_audioEngine) {
                m_audioEngine->setSinkBufferMs(s->bufferSizeMs());
            }
        });
        connect(settings, &Settings::volumeChanged, this, [this]() {
            Settings *s = Settings::instance();
            if (s && m_audioEngine) {
                m_audioEngine->setVolume(s->volume());
            }
        });
        
        // Apply initial volume from settings
        m_audioEngine->setVolume(settings->volume());
    }
    
    // Initialize SessionManager and load session if enabled
    SessionManager *sessionMgr = SessionManager::instance();
    if (!sessionMgr) {
        sessionMgr = new SessionManager(this);
    }
    sessionMgr->initialize(m_playlistStore);
    
    // Initialize ViewedPlaylistRouter - don't create here, let QML create via create()
    // Just store the PlaylistStore pointer so the router can access it later
    // The router will get the store from AppViewModel::instance()->playlistStore() when needed
    
    // Initialize BrowseActivationService
    m_browseActivation = new BrowseActivationService(this);
    m_browseActivation->initialize(this, m_playlistStore, nullptr, m_libraryDb);
    
    // Load session if restore is enabled
    if (settings->restoreSession()) {
        sessionMgr->loadSession();
        
    }
    
    // Note: We don't connect to currentTrackChanged for now playing display
    // Now playing is updated only when AudioEngine actually starts playing a track
    // This is done in updateNowPlaying() called from play methods
    
    connect(m_audioEngine, &AudioEngine::stateChanged, this, [this](AudioEngine::State state) {
        switch (state) {
        case AudioEngine::Stopped: 
            m_playbackState = Stopped;
            // Don't clear now playing here - AudioEngine::play() calls stop() internally
            // which would clear the info before we set it for the new track.
            // Now playing is cleared explicitly in AppViewModel::stop() instead.
            break;
        case AudioEngine::Playing: m_playbackState = Playing; break;
        case AudioEngine::Paused: m_playbackState = Paused; break;
        case AudioEngine::Buffering: m_playbackState = Buffering; break;
        case AudioEngine::Error: m_playbackState = Error; break;
        }
        emit playbackStateChanged();
    });
    
    connect(m_audioEngine, &AudioEngine::positionMsChanged, this, [this](qint64 pos) {
        m_positionMs = pos;
        emit positionMsChanged();
    });
    
    connect(m_audioEngine, &AudioEngine::durationMsChanged, this, [this](qint64 dur) {
        m_durationMs = dur;
        emit durationMsChanged();
    });
    
    connect(m_audioEngine, &AudioEngine::trackAboutToFinish, this, [this](qint64 msRemaining) {
        Q_UNUSED(msRemaining)
        TrackInfo nextTrack = m_queueManager->peekNextTrack();
        if (nextTrack.isValid()) {
            m_audioEngine->prepareNext(nextTrack.filePath);
        } else {
            m_audioEngine->clearNext();
        }
    });
    
    connect(m_audioEngine, &AudioEngine::trackChanged, this, [this](const QString &filePath) {
        Q_UNUSED(filePath)
        m_queueManager->advance();
        // Update now playing to reflect the new track that just started
        TrackInfo currentTrack = m_queueManager->currentTrack();
        if (currentTrack.isValid()) {
            updateNowPlaying(currentTrack);
        }
        TrackInfo nextTrack = m_queueManager->peekNextTrack();
        if (nextTrack.isValid()) {
            m_audioEngine->prepareNext(nextTrack.filePath);
        }
    });
    
    connect(m_audioEngine, &AudioEngine::trackFinished, this, [this]() {
        if (m_queueManager->canAdvance()) {
            m_queueManager->advance();
            TrackInfo track = m_queueManager->currentTrack();
            if (track.isValid()) {
                updateNowPlaying(track);
                m_audioEngine->play(track.filePath);
            }
        } else {
            // End of queue - stop playback completely
            stop();
        }
    });
    
    connect(m_audioEngine, &AudioEngine::errorOccurred, this, [this](int code, const QString &msg) {
        Q_UNUSED(code)
        m_errorText = msg;
        m_hasError = true;
        emit errorTextChanged();
        emit hasErrorChanged();
    });
}

AppViewModel *AppViewModel::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(jsEngine)
    
    if (!s_instance) {
        s_instance = new AppViewModel(qmlEngine);
    }
    return s_instance;
}

AppViewModel *AppViewModel::instance()
{
    return s_instance;
}

void AppViewModel::play()
{
    clearError();
    if (m_playbackState == Paused) {
        m_audioEngine->resume();
    } else if (m_playbackState == Stopped) {
        // Make the displayed playlist the active playlist when starting from stopped
        if (m_playlistStore->displayedPlaylistId() != m_playlistStore->activePlaylistId()) {
            m_playlistStore->setActivePlaylist(m_playlistStore->displayedPlaylistIdString());
        }
        
        if (m_queueManager->currentIndex() < 0 && m_playlistStore->displayedPlaylist()->count() > 0) {
            m_queueManager->setCurrentIndex(0);
        }
        TrackInfo track = m_queueManager->currentTrack();
        if (track.isValid()) {
            updateNowPlaying(track);
            m_audioEngine->play(track.filePath);
        }
    }
}

void AppViewModel::pause()
{
    if (m_playbackState == Playing) {
        m_audioEngine->pause();
    }
}

void AppViewModel::stop()
{
    m_audioEngine->stop();
    
    // Reset queue position
    m_queueManager->reset();
    
    // Reset position and duration
    m_positionMs = 0;
    m_durationMs = 0;
    emit positionMsChanged();
    emit durationMsChanged();
    
    // Clear now playing when user explicitly stops playback
    m_nowPlayingTitle = QStringLiteral("No track");
    m_nowPlayingArtist = QStringLiteral("Unknown Artist");
    m_nowPlayingCoverUrl.clear();
    emit nowPlayingChanged();
}

void AppViewModel::next()
{
    if (m_queueManager->canAdvance()) {
        m_queueManager->advance();
        TrackInfo track = m_queueManager->currentTrack();
        if (track.isValid()) {
            updateNowPlaying(track);
            m_audioEngine->play(track.filePath);
        }
    } else {
        // No next track - stop playback
        stop();
    }
}

void AppViewModel::previous()
{
    Settings *settings = Settings::instance();
    
    // Check if we should restart the current track first
    if (settings && settings->previousButtonAction() == Settings::RestartThenJump) {
        // If position > 3 seconds, restart current track instead of jumping
        if (m_positionMs > 3000) {
            m_audioEngine->seek(0);
            return;
        }
    }
    
    // Jump to previous track
    if (m_queueManager->canRetreat()) {
        m_queueManager->retreat();
        TrackInfo track = m_queueManager->currentTrack();
        if (track.isValid()) {
            updateNowPlaying(track);
            m_audioEngine->play(track.filePath);
        }
    } else {
        // No previous track - restart current track or stop
        if (m_playbackState == Playing || m_playbackState == Paused) {
            m_audioEngine->seek(0);
        } else {
            stop();
        }
    }
}

void AppViewModel::seek(qint64 positionMs)
{
    if (positionMs >= 0 && positionMs <= m_durationMs) {
        m_audioEngine->seek(positionMs);
    }
}

void AppViewModel::playIndex(int row)
{
    if (row < 0 || row >= m_playlistStore->activePlaylist()->count())
        return;
    
    m_queueManager->setCurrentIndex(row);
    TrackInfo track = m_queueManager->currentTrack();
    if (track.isValid()) {
        updateNowPlaying(track);
        m_audioEngine->play(track.filePath);
    }
}

void AppViewModel::setVolume(double value)
{
    m_volume = qBound(0.0, value, 1.0);
    m_audioEngine->setVolume(m_volume);
}

TrackListModel* AppViewModel::activePlaylistModel() const
{
    return m_playlistStore->activePlaylist();
}

TrackListModel* AppViewModel::displayedPlaylistModel() const
{
    return m_playlistStore->displayedPlaylist();
}

QAbstractItemModel* AppViewModel::playlistTabsModel() const
{
    return m_playlistTabsModel;
}

PlaylistStore *AppViewModel::playlistStore() const
{
    return m_playlistStore;
}

void AppViewModel::addFilesToPlaylist(const QList<QUrl> &urls)
{
    TrackListModel *model = m_playlistStore->displayedPlaylist();
    if (!model) return;
    
    for (const QUrl &url : urls) {
        QString filePath = url.toLocalFile();
        if (!filePath.isEmpty()) {
            TrackInfo track = MetadataExtractor::extractTrackInfo(filePath);
            if (track.isValid())
                model->addTrack(track);
        }
    }
}

void AppViewModel::removeFromPlaylist(int index, int count)
{
    TrackListModel *model = m_playlistStore->displayedPlaylist();
    if (!model) return;
    model->removeRows(index, count);
}

void AppViewModel::movePlaylistRow(int from, int to)
{
    TrackListModel *model = m_playlistStore->displayedPlaylist();
    if (!model) return;
    model->moveRow(from, to);
}

void AppViewModel::clearPlaylist()
{
    TrackListModel *model = m_playlistStore->displayedPlaylist();
    if (model) model->clear();
    // Do NOT stop playback - user may be playing from a different playlist
}

FileBrowserModel *AppViewModel::fileBrowserModel() const
{
    return m_fileBrowserModel;
}

bool AppViewModel::libraryScanning() const
{
    return m_libraryScanner ? m_libraryScanner->isScanning() : false;
}

int AppViewModel::libraryScanProgress() const
{
    return m_libraryScanner ? m_libraryScanner->progress() : 0;
}

void AppViewModel::addLibraryFolder(const QString &path)
{
    qDebug() << "AppViewModel::addLibraryFolder:" << path;
    m_libraryDb->addWatchFolder(path);
    m_libraryWatcher->setWatchFolders(m_libraryDb->watchFolders());
    m_libraryScanner->scanFolder(path);
    emit libraryFoldersChanged();
}

void AppViewModel::removeLibraryFolder(const QString &path)
{
    m_libraryDb->removeWatchFolder(path);
    m_libraryDb->removeTracksInFolder(path);
    m_libraryWatcher->setWatchFolders(m_libraryDb->watchFolders());
    m_libraryDb->notifyDatabaseChanged();
    emit libraryFoldersChanged();
    emit libraryTrackCountChanged();
}

void AppViewModel::rescanLibrary()
{
    m_libraryScanner->rescanAll();
}

QStringList AppViewModel::libraryFolders() const
{
    return m_libraryDb->watchFolders();
}

QStringList AppViewModel::watchFolders() const
{
    return m_libraryDb->watchFolders();
}

void AppViewModel::addWatchFolder(const QString &path)
{
    m_libraryDb->addWatchFolder(path);
    m_libraryWatcher->setWatchFolders(m_libraryDb->watchFolders());
    m_libraryScanner->scanFolder(path);
    emit libraryFoldersChanged();
}

void AppViewModel::removeWatchFolder(const QString &path)
{
    m_libraryDb->removeWatchFolder(path);
    m_libraryDb->removeTracksInFolder(path);
    m_libraryWatcher->setWatchFolders(m_libraryDb->watchFolders());
    m_libraryDb->notifyDatabaseChanged();
    emit libraryFoldersChanged();
    emit libraryTrackCountChanged();
}

int AppViewModel::libraryTrackCount() const
{
    return m_libraryDb ? m_libraryDb->trackCount() : 0;
}

qint64 AppViewModel::lastScanTime() const
{
    // This would need to be stored in the database
    // For now, return 0 to indicate "never"
    return 0;
}

BrowseActivationService *AppViewModel::browseActivation() const
{
    return m_browseActivation;
}

// NOTE: ALL THIS IS GONNA HAVE TO BE REMADE, SEE THE UI V2 DOCUMENTATION
void AppViewModel::addLibraryTracksToPlaylist(const QVariantList &tracks)
{
    for (const QVariant &v : tracks) {
        QVariantMap map = v.toMap();
        QString filePath = map.value("filePath").toString();
        if (filePath.isEmpty())
            continue;
        
        // Try to get from library database first (has all metadata)
        if (m_libraryDb) {
            auto libOpt = m_libraryDb->trackByPath(filePath);
            if (libOpt.has_value()) {
                m_playlistStore->displayedPlaylist()->addTrack(MetadataExtractor::toTrackInfo(*libOpt));
                continue;
            }
        }
        
        // Fallback: extract metadata from file
        TrackInfo track = MetadataExtractor::extractTrackInfo(filePath);
        if (track.isValid()) {
            m_playlistStore->displayedPlaylist()->addTrack(track);
        }
    }
}

void AppViewModel::setPlaybackState(PlaybackState state)
{
    if (m_playbackState != state) {
        m_playbackState = state;
        emit playbackStateChanged();
    }
}

void AppViewModel::setError(const QString &text)
{
    m_errorText = text;
    m_hasError = !text.isEmpty();
    emit errorTextChanged();
    emit hasErrorChanged();
}

void AppViewModel::clearError()
{
    if (m_hasError) {
        m_errorText.clear();
        m_hasError = false;
        emit errorTextChanged();
        emit hasErrorChanged();
    }
}

void AppViewModel::updateNowPlaying(const TrackInfo &track)
{
    if (track.isValid()) {
        m_nowPlayingTitle = track.title.isEmpty() ? QFileInfo(track.filePath).fileName() : track.title;
        m_nowPlayingArtist = track.artist;
        
        // Use async image provider to avoid blocking GUI thread with TagLib/QImage work
        m_nowPlayingCoverUrl = QStringLiteral("image://cover/") + track.filePath;
        
        emit nowPlayingChanged();
    }
}
