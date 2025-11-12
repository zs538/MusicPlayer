#include "PlayerController.h"
#include "MetadataReader.h"
#include "TrackMetadata.h"
#include "PlaylistModel.h"

#include <QFileInfo>
#include <QDebug>

PlayerController::PlayerController(QObject* parent)
    : QObject(parent)
    , m_currentMetadata(nullptr)
{
    setupDeck(m_a);
    setupDeck(m_b);

    m_current = &m_a;
    m_next = &m_b;

    connect(&m_devices, &QMediaDevices::audioOutputsChanged,
            this, &PlayerController::onAudioOutputsChanged);

    refreshOutputs();
    emit audioOutputsChanged();

    selectDefaultOutputDevice();
}

void PlayerController::setupDeck(Deck& deck)
{
    deck.audio = new QAudioOutput(this);
    deck.audio->setVolume(0.8f);

    deck.player = new QMediaPlayer(this);
    deck.player->setAudioOutput(deck.audio);

    connect(deck.player, &QMediaPlayer::positionChanged, this, &PlayerController::onPositionChanged);
    connect(deck.player, &QMediaPlayer::durationChanged, this, &PlayerController::onDurationChanged);
    connect(deck.player, &QMediaPlayer::mediaStatusChanged, this, &PlayerController::onMediaStatusChanged);
    connect(deck.player, &QMediaPlayer::errorOccurred, this, &PlayerController::onPlayerErrorOccurred);
    connect(deck.player, &QMediaPlayer::metaDataChanged, this, &PlayerController::onMetaDataChanged);
}

void PlayerController::openFile(const QUrl& url)
{
    if (!m_current->player || !m_current->audio) return;
    
    // Clear current metadata
    if (m_currentMetadata) {
        m_currentMetadata->deleteLater();
        m_currentMetadata = nullptr;
    }
    
    m_current->audio->setMuted(false);
    m_current->player->setAudioOutput(m_current->audio);
    m_current->player->setSource(url);
    
    // Use TagLib to read metadata immediately
    updateCurrentMetadataFromSource();
    
    m_current->player->play();
    m_gaplessArmed = false;
    emit playingChanged();
    emit currentSourceChanged();
}

void PlayerController::setNextFile(const QUrl& url)
{
    if (!m_next->player) return;
    
    m_next->source = url;
    m_next->player->setSource(url);
    m_next->player->pause();
    m_gaplessArmed = false;
}

void PlayerController::play()
{
    if (!m_current->player || !m_current->audio) return;
    
    m_current->audio->setMuted(false);
    m_current->player->setAudioOutput(m_current->audio);
    m_current->player->play();
    m_gaplessArmed = false;
    emit playingChanged();
}

void PlayerController::pause()
{
    if (!m_current->player) return;
    m_current->player->pause();
    emit playingChanged();
}

void PlayerController::seek(qint64 posMs)
{
    if (!m_current->player) return;
    m_current->player->setPosition(posMs);
}

bool PlayerController::playing() const
{
    return m_current->player ? m_current->player->playbackState() == QMediaPlayer::PlayingState : false;
}

qint64 PlayerController::position() const
{
    return m_current->player ? m_current->player->position() : 0;
}

qint64 PlayerController::duration() const
{
    return m_current->player ? m_current->player->duration() : 0;
}

float PlayerController::volume() const
{
    return m_current->audio ? m_current->audio->volume() : 0.0f;
}

void PlayerController::setVolume(float v)
{
    if (m_a.audio) m_a.audio->setVolume(v);
    if (m_b.audio) m_b.audio->setVolume(v);
    emit volumeChanged();
}

void PlayerController::onPositionChanged()
{
    // Pre-switch just before end for tighter gapless behavior
    const qint64 dur = m_current->player->duration();
    const qint64 pos = m_current->player->position();
    if (dur > 0 && m_next->player->source().isValid() && !m_gaplessArmed) {
        const qint64 remaining = dur - pos;
        if (remaining <= 30) { // ~30ms window
            m_gaplessArmed = true;
            switchToNext();
            return;
        }
    }
    emit positionChanged();
}

void PlayerController::onDurationChanged()
{
    emit durationChanged();
}

void PlayerController::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (sender() != m_current->player)
        return;

    if (status == QMediaPlayer::EndOfMedia) {
        // If no next track is armed, clear to empty state
        if (!m_next->player || m_next->player->source().isEmpty()) {
            clearCurrent();
        } else {
            switchToNext();
        }
    }
}

void PlayerController::switchToNext()
{
    if (!m_next->player || m_next->player->source().isEmpty()) {
        // No next track available, transition to empty state
        clearCurrent();
        return;
    }
    
    std::swap(m_current, m_next);
    
    // Clear metadata for clean state
    if (m_currentMetadata) {
        m_currentMetadata->clear();
    }
    
    m_current->player->play();
    m_gaplessArmed = false;
    emit playingChanged();
    emit currentSourceChanged();
}

void PlayerController::clearCurrent()
{
    // Stop playback and clear sources for both decks to ensure an empty state
    if (m_a.player) m_a.player->stop();
    if (m_b.player) m_b.player->stop();
    if (m_a.player) m_a.player->setSource(QUrl());
    if (m_b.player) m_b.player->setSource(QUrl());
    if (m_a.audio) m_a.player->setAudioOutput(m_a.audio);
    if (m_b.audio) m_b.player->setAudioOutput(m_b.audio);

    // Reset next deck bookkeeping
    if (m_next) m_next->source = QUrl();

    // Clear metadata object so QML shows "No track playing"
    if (m_currentMetadata) {
        m_currentMetadata->deleteLater();
        m_currentMetadata = nullptr;
        emit currentMetadataChanged();
    }

    m_gaplessArmed = false;

    // Notify QML bindings to reset UI state
    emit playingChanged();
    emit currentSourceChanged();
    emit positionChanged();
    emit durationChanged();
}

void PlayerController::onAudioOutputsChanged()
{
    refreshOutputs();
    selectDefaultOutputDevice();
    emit audioOutputsChanged();
}

void PlayerController::selectDefaultOutputDevice()
{
    const auto outs = m_devices.audioOutputs();
    if (outs.isEmpty()) {
        return;
    }
    
    auto dev = m_devices.defaultAudioOutput();
    if (m_a.audio) m_a.audio->setDevice(dev);
    if (m_b.audio) m_b.audio->setDevice(dev);
    if (m_a.player && m_a.audio) m_a.player->setAudioOutput(m_a.audio);
    if (m_b.player && m_b.audio) m_b.player->setAudioOutput(m_b.audio);
    emit audioOutputsChanged();
}

QStringList PlayerController::audioOutputs() const
{
    QStringList names;
    for (const auto &d : m_outputDevices)
        names << d.description();
    return names;
}

QString PlayerController::currentOutput() const
{
    if (!m_current->audio)
        return {};
    return m_current->audio->device().description();
}

void PlayerController::selectOutputByIndex(int index)
{
    if (index < 0 || index >= m_outputDevices.size())
        return;
    
    const auto dev = m_outputDevices.at(index);
    if (m_a.audio) m_a.audio->setDevice(dev);
    if (m_b.audio) m_b.audio->setDevice(dev);
    if (m_a.player && m_a.audio) m_a.player->setAudioOutput(m_a.audio);
    if (m_b.player && m_b.audio) m_b.player->setAudioOutput(m_b.audio);
    emit audioOutputsChanged();
}

void PlayerController::refreshOutputs()
{
    m_outputDevices = m_devices.audioOutputs();
}

void PlayerController::onPlayerErrorOccurred(QMediaPlayer::Error error, const QString &errorString)
{
    Q_UNUSED(error)
    Q_UNUSED(errorString)
}

void PlayerController::refreshAudioDevices()
{
    refreshOutputs();
    emit audioOutputsChanged();
}

void PlayerController::onMetaDataChanged()
{
    // Don't use Qt metadata - use TagLib instead
    // This method is now handled by updateCurrentMetadataFromSource()
}

void PlayerController::updateCurrentMetadataFromSource()
{
    if (!m_current->player || m_current->player->source().isEmpty()) {
        return;
    }
    
    // Use TagLib to read metadata from the current source
    TrackMetadata* newMetadata = MetadataReader::readMetadataStandalone(m_current->player->source(), this);
    
    if (newMetadata) {
        if (m_currentMetadata) {
            m_currentMetadata->deleteLater();
        }
        m_currentMetadata = newMetadata;
        emit currentMetadataChanged();
    }
}
