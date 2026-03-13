#include "AppViewModel.h"
#include "PlaylistStore.h"
#include "PlaylistTabsModel.h"
#include "TrackListModel.h"
#include "QueueManager.h"
#include "SessionManager.h"
#include "Settings.h"
#include "MetadataExtractor.h"
#include "CoverImageProvider.h"
#include "TrackFilter.h"
#include "audio/AudioEngine.h"
#include "library/LibraryController.h"
#include "library/LibraryDatabase.h"
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
    , m_libraryController(new LibraryController(this))
{
    s_instance = this;
    
    m_libraryController->initialize();
    
    connect(m_libraryController, &LibraryController::scanningChanged, this, &AppViewModel::libraryScanningChanged);
    connect(m_libraryController, &LibraryController::scanProgressChanged, this, &AppViewModel::libraryScanProgressChanged);
    connect(m_libraryController, &LibraryController::libraryFoldersChanged, this, &AppViewModel::libraryFoldersChanged);
    connect(m_libraryController, &LibraryController::trackCountChanged, this, &AppViewModel::libraryTrackCountChanged);
    connect(m_libraryController, &LibraryController::scanFinished, this, [this]() {
        refreshPlaylistMetadataFromLibrary();
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
    
    // Ensure Settings is created
    Settings *settings = Settings::instance();
    if (!settings) {
        settings = new Settings(this);
    }
    
    // Connect Settings to AudioEngine
    if (settings && m_audioEngine) {
        m_audioEngine->setSinkBufferMs(settings->bufferSizeMs());
        m_audioEngine->setGaplessLeadInMs(settings->gaplessLeadInMs());
        
        connect(settings, &Settings::bufferSizeMsChanged, this, [this]() {
            Settings *s = Settings::instance();
            if (s && m_audioEngine) {
                m_audioEngine->setSinkBufferMs(s->bufferSizeMs());
            }
        });
        connect(settings, &Settings::gaplessLeadInMsChanged, this, [this]() {
            Settings *s = Settings::instance();
            if (s && m_audioEngine) {
                m_audioEngine->setGaplessLeadInMs(s->gaplessLeadInMs());
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
    SessionManager *sessionMgr = SessionManager::ensureInstance(this);
    sessionMgr->initialize(m_playlistStore);
    
    // Initialize ViewedPlaylistRouter - don't create here, let QML create via create()
    // Just store the PlaylistStore pointer so the router can access it later
    // The router will get the store from AppViewModel::instance()->playlistStore() when needed
    
    // Initialize BrowseActivationService
    m_browseActivation = new BrowseActivationService(this);
    m_browseActivation->initialize(this, m_playlistStore, nullptr, m_libraryController->database());
    
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
        
        // Persist playback position on track change (position resets to 0 for new track)
        SessionManager *sessionMgr = SessionManager::instance();
        if (sessionMgr && m_playlistStore) {
            int playlistIdx = m_playlistStore->indexOfUuid(m_playlistStore->activePlaylistId());
            sessionMgr->setPlaybackState(playlistIdx, 0);
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

QString AppViewModel::coverImageSourceForFile(const QString &filePath) const
{
    return CoverImageProvider::sourceForFilePath(filePath);
}

QString AppViewModel::coverImageSourceForFiles(const QStringList &filePaths) const
{
    return CoverImageProvider::sourceForFilePaths(filePaths);
}

QString AppViewModel::localFileUrlForPath(const QString &filePath) const
{
    if (filePath.isEmpty()) {
        return QString();
    }

    return QUrl::fromLocalFile(filePath).toString();
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
        
        // Persist playback position on pause
        SessionManager *sessionMgr = SessionManager::instance();
        if (sessionMgr && m_playlistStore) {
            int playlistIdx = m_playlistStore->indexOfUuid(m_playlistStore->activePlaylistId());
            sessionMgr->setPlaybackState(playlistIdx, m_positionMs);
        }
    }
}

void AppViewModel::stop()
{
    // Persist playback position before stopping
    SessionManager *sessionMgr = SessionManager::instance();
    if (sessionMgr && m_playlistStore) {
        int playlistIdx = m_playlistStore->indexOfUuid(m_playlistStore->activePlaylistId());
        sessionMgr->setPlaybackState(playlistIdx, m_positionMs);
    }
    
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

bool AppViewModel::libraryScanning() const
{
    return m_libraryController ? m_libraryController->isScanning() : false;
}

int AppViewModel::libraryScanProgress() const
{
    return m_libraryController ? m_libraryController->scanProgress() : 0;
}

void AppViewModel::addLibraryFolder(const QString &path)
{
    m_libraryController->addLibraryFolder(path);
}

void AppViewModel::removeLibraryFolder(const QString &path)
{
    m_libraryController->removeLibraryFolder(path);
}

void AppViewModel::rescanLibrary()
{
    m_libraryController->rescanLibrary();
}

void AppViewModel::rescanCollectionEntry(const QVariantList &filter, const QString &entryType,
                                         const QString &groupType, const QVariant &groupValue,
                                         const QString &filePath)
{
    m_libraryController->rescanCollectionEntry(filter, entryType, groupType, groupValue, filePath);
}

void AppViewModel::rescanPlaylistSelection(const QString &playlistId, const QVariantList &rows)
{
    if (!m_playlistStore)
        return;

    TrackListModel *playlist = m_playlistStore->getPlaylistModel(playlistId);
    if (!playlist)
        return;

    QStringList filePaths;
    for (const QVariant &rowValue : rows) {
        const int row = rowValue.toInt();
        const TrackInfo track = playlist->trackAt(row);
        if (!track.filePath.isEmpty())
            filePaths.append(track.filePath);
    }

    m_libraryController->rescanFiles(filePaths);
}

QStringList AppViewModel::watchFolders() const
{
    return m_libraryController->libraryFolders();
}

void AppViewModel::refreshPlaylistMetadataFromLibrary()
{
    LibraryDatabase *db = m_libraryController ? m_libraryController->database() : nullptr;
    if (!m_playlistStore || !db)
        return;

    const QString currentTrackPath = m_queueManager ? m_queueManager->currentTrack().filePath : QString();
    TrackInfo refreshedCurrentTrack;

    for (const PlaylistStore::Tab &tab : m_playlistStore->tabs()) {
        TrackListModel *playlist = tab.model;
        if (!playlist)
            continue;

        for (int i = 0; i < playlist->count(); ++i) {
            const TrackInfo existingTrack = playlist->trackAt(i);
            if (existingTrack.filePath.isEmpty())
                continue;

            auto libraryTrack = db->trackByPath(existingTrack.filePath);
            if (!libraryTrack.has_value())
                continue;

            const TrackInfo refreshedTrack = MetadataExtractor::toTrackInfo(*libraryTrack);
            playlist->updateTrackMetadata(i, refreshedTrack);

            if (!currentTrackPath.isEmpty() && refreshedTrack.filePath == currentTrackPath)
                refreshedCurrentTrack = refreshedTrack;
        }
    }

    if (refreshedCurrentTrack.isValid())
        updateNowPlaying(refreshedCurrentTrack);
}

int AppViewModel::libraryTrackCount() const
{
    return m_libraryController ? m_libraryController->trackCount() : 0;
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

LibraryDatabase *AppViewModel::libraryDatabase() const
{
    return m_libraryController->database();
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
        m_nowPlayingCoverUrl = coverImageSourceForFile(track.filePath);
        
        emit nowPlayingChanged();
    }
}
