#include "PlayerController.h"
#include "MetadataReader.h"
#include "TrackMetadata.h"
#include "PlaylistModel.h"
#include "PlaylistManager.h"

#include <QDebug>
#include <QTimer>

PlayerController::PlayerController(QObject* parent)
    : QObject(parent)
    , m_currentMetadata(nullptr)
{
    // Create engine and queue
    m_engine = new PlaybackEngine(this);
    m_queue = new PlaybackQueue(this);
    
    // Connect engine signals
    connect(m_engine, &PlaybackEngine::stateChanged,
            this, &PlayerController::onEngineStateChanged);
    connect(m_engine, &PlaybackEngine::trackChanged,
            this, &PlayerController::onEngineTrackChanged);
    connect(m_engine, &PlaybackEngine::trackAboutToFinish,
            this, &PlayerController::onEngineTrackAboutToFinish);
    connect(m_engine, &PlaybackEngine::trackFinished,
            this, &PlayerController::onEngineTrackFinished);
    connect(m_engine, &PlaybackEngine::positionChanged,
            this, &PlayerController::onEnginePositionChanged);
    connect(m_engine, &PlaybackEngine::durationChanged,
            this, &PlayerController::onEngineDurationChanged);
    connect(m_engine, &PlaybackEngine::errorOccurred,
            this, &PlayerController::onEngineError);
    
    // Connect queue signals
    connect(m_queue, &PlaybackQueue::queueModified,
            this, &PlayerController::onQueueModified);
    connect(m_queue, &PlaybackQueue::currentIndexChanged,
            this, &PlayerController::currentIndexChanged);
    
    // Audio device monitoring
    connect(&m_devices, &QMediaDevices::audioOutputsChanged,
            this, &PlayerController::onAudioOutputsChanged);
    
    refreshOutputs();
    emit audioOutputsChanged();
}

PlayerController::~PlayerController()
{
    if (m_currentMetadata) {
        m_currentMetadata->deleteLater();
    }
}

void PlayerController::setPlaylistModel(PlaylistModel* model)
{
    if (m_playlistModel == model) return;
    
    m_playlistModel = model;
    m_queue->setActivePlaylist(model);
}

void PlayerController::setPlaylistManager(PlaylistManager* manager)
{
    if (m_playlistManager == manager) return;
    
    m_playlistManager = manager;
    
    if (m_playlistManager) {
        // Connect to active playlist changes
        connect(m_playlistManager, &PlaylistManager::activePlaylistChanged,
                this, &PlayerController::connectToActivePlaylist);
        
        // Set initial active playlist
        connectToActivePlaylist();
    }
}

void PlayerController::connectToActivePlaylist()
{
    if (!m_playlistManager) return;
    
    PlaylistModel* activePlaylist = m_playlistManager->activePlaylist();
    setPlaylistModel(activePlaylist);
}

void PlayerController::openFile(const QUrl& url)
{
    // This is a legacy API - prefer playIndex() for playlist items
    // openFile is for playing a file that may or may not be in the playlist
    QUrl validUrl = url;
    if (validUrl.scheme().isEmpty()) {
        validUrl = QUrl::fromLocalFile(validUrl.path());
    }
    
    // Create TrackRef without playlist index (standalone file)
    TrackRef track;
    track.url = validUrl;
    track.displayName = validUrl.fileName();
    track.playlistIndex = -1;
    
    m_engine->play(track);
    
    updateCurrentMetadataFromSource();
    emit currentSourceChanged();
}

void PlayerController::playIndex(int index)
{
    if (!m_playlistModel || index < 0 || index >= m_playlistModel->rowCount()) {
        return;
    }
    
    m_queue->setCurrentIndex(index);
    
    TrackRef track = m_queue->current();
    if (!track.isValid()) return;
    
    m_engine->play(track);
    armNextTrack();
    
    updateCurrentMetadataFromSource();
    emit currentSourceChanged();
}

void PlayerController::play()
{
    if (m_engine->state() == PlaybackEngine::State::Paused) {
        m_engine->resume();
    } else if (m_engine->state() == PlaybackEngine::State::Stopped) {
        // If stopped, try to play current track from queue
        TrackRef track = m_queue->current();
        if (track.isValid()) {
            m_engine->play(track);
            armNextTrack();
        } else if (!m_queue->isEmpty()) {
            // No current track, start from beginning
            m_queue->setCurrentIndex(0);
            track = m_queue->current();
            if (track.isValid()) {
                m_engine->play(track);
                armNextTrack();
            }
        }
    }
    emit playingChanged();
}

void PlayerController::pause()
{
    m_engine->pause();
    emit playingChanged();
}

void PlayerController::stop()
{
    m_engine->stop();
    emit playingChanged();
}

void PlayerController::seek(qint64 posMs)
{
    m_engine->seek(posMs);
    // Re-arm next track after seek
    armNextTrack();
}

void PlayerController::next()
{
    if (!m_queue->hasNext()) {
        // End of playlist
        return;
    }
    
    m_queue->advance();
    TrackRef track = m_queue->current();
    
    if (track.isValid()) {
        m_engine->play(track);
        armNextTrack();
        updateCurrentMetadataFromSource();
        emit currentSourceChanged();
    }
}

void PlayerController::previous()
{
    if (!m_queue->hasPrevious()) {
        // Beginning of playlist - seek to start of current track
        m_engine->seek(0);
        return;
    }
    
    m_queue->goBack();
    TrackRef track = m_queue->current();
    
    if (track.isValid()) {
        m_engine->play(track);
        armNextTrack();
        updateCurrentMetadataFromSource();
        emit currentSourceChanged();
    }
}

bool PlayerController::playing() const
{
    return m_engine->state() == PlaybackEngine::State::Playing;
}

qint64 PlayerController::position() const
{
    return m_engine->position();
}

qint64 PlayerController::duration() const
{
    return m_engine->duration();
}

float PlayerController::volume() const
{
    return m_targetVolume;
}

void PlayerController::setVolume(float v)
{
    m_targetVolume = v;
    m_engine->setVolume(v);
    emit volumeChanged();
}

QUrl PlayerController::currentSource() const
{
    return m_engine->currentTrack().url;
}

int PlayerController::repeatMode() const
{
    return static_cast<int>(m_queue->repeatMode());
}

void PlayerController::setRepeatMode(int mode)
{
    m_queue->setRepeatMode(static_cast<RepeatMode>(mode));
    emit repeatModeChanged();
}

bool PlayerController::shuffle() const
{
    return m_queue->shuffle();
}

void PlayerController::setShuffle(bool enabled)
{
    m_queue->setShuffle(enabled);
    emit shuffleChanged();
}

QStringList PlayerController::audioOutputs() const
{
    QStringList names;
    for (const auto& d : m_outputDevices) {
        names << d.description();
    }
    return names;
}

QString PlayerController::currentOutput() const
{
    return m_engine->outputDevice().description();
}

int PlayerController::currentIndex() const
{
    return m_queue->currentIndex();
}

int PlayerController::bufferProfile() const
{
    return (m_engine->bufferProfile() == BufferProfile::Short) ? 0 : 1;
}

void PlayerController::setBufferProfile(int profile)
{
    BufferProfile newProfile = (profile == 1) ? BufferProfile::Long : BufferProfile::Short;
    
    if (newProfile == m_engine->bufferProfile())
        return;
    
    m_engine->setBufferProfile(newProfile);
    emit bufferProfileChanged();
}

void PlayerController::selectOutputByIndex(int index)
{
    if (index < 0 || index >= m_outputDevices.size()) return;
    
    m_engine->setOutputDevice(m_outputDevices.at(index));
    emit audioOutputsChanged();
}

void PlayerController::refreshAudioDevices()
{
    refreshOutputs();
    emit audioOutputsChanged();
}

void PlayerController::refreshOutputs()
{
    m_outputDevices = m_devices.audioOutputs();
}

void PlayerController::onEngineStateChanged(PlaybackEngine::State state)
{
    Q_UNUSED(state)
    emit playingChanged();
}

void PlayerController::onEngineTrackChanged(const TrackRef& track)
{
    // Update queue index to match engine's current track using playlistIndex
    // (This handles gapless transitions where engine advances automatically)
    if (track.playlistIndex >= 0 && m_queue->currentIndex() != track.playlistIndex) {
        m_queue->setCurrentIndex(track.playlistIndex);
    }
    
    // Arm the next track for gapless playback
    armNextTrack();
    
    updateCurrentMetadataFromSource();
    emit currentSourceChanged();
    emit durationChanged();
}

void PlayerController::onEngineTrackAboutToFinish(qint64 msRemaining)
{
    Q_UNUSED(msRemaining)
    
    // Ensure next track is armed
    // The engine will use whatever is prepared via prepareNext()
    armNextTrack();
}

void PlayerController::onEngineTrackFinished()
{
    // This is called when playback actually stops (no gapless transition occurred)
    // For gapless transitions, trackChanged is emitted instead
    
    // If engine stopped and there's a next track, start it manually
    if (m_engine->state() == PlaybackEngine::State::Stopped) {
        if (m_queue->hasNext()) {
            m_queue->advance();
            TrackRef track = m_queue->current();
            if (track.isValid()) {
                m_engine->play(track);
                armNextTrack();
            }
        } else {
            // End of playlist
            clearCurrent();
        }
    }
}

void PlayerController::onEnginePositionChanged(qint64 positionMs)
{
    Q_UNUSED(positionMs)
    emit positionChanged();
}

void PlayerController::onEngineDurationChanged(qint64 durationMs)
{
    Q_UNUSED(durationMs)
    emit durationChanged();
}

void PlayerController::onEngineError(const QString& message)
{
    qWarning() << "Playback engine error:" << message;
}

void PlayerController::onQueueModified()
{
    // Playlist changed - update the armed next track
    armNextTrack();
}

void PlayerController::onAudioOutputsChanged()
{
    refreshOutputs();
    emit audioOutputsChanged();
}

void PlayerController::updateCurrentMetadataFromSource()
{
    QUrl source = m_engine->currentTrack().url;
    if (source.isEmpty()) return;
    
    TrackMetadata* newMetadata = MetadataReader::readMetadataStandalone(source, this);
    
    if (newMetadata) {
        if (m_currentMetadata) {
            m_currentMetadata->deleteLater();
        }
        m_currentMetadata = newMetadata;
        emit currentMetadataChanged();
    }
}

void PlayerController::clearCurrent()
{
    m_engine->stop();
    
    if (m_currentMetadata) {
        m_currentMetadata->deleteLater();
        m_currentMetadata = nullptr;
        emit currentMetadataChanged();
    }
    
    emit playingChanged();
    emit currentSourceChanged();
    emit positionChanged();
    emit durationChanged();
}

void PlayerController::armNextTrack()
{
    TrackRef next = m_queue->peekNext();
    
    if (next.isValid()) {
        m_engine->prepareNext(next);
    } else {
        m_engine->clearNext();
    }
}
