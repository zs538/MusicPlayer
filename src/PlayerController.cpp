#include "PlayerController.h"

#include <QFileInfo>
#include <QDebug>

PlayerController::PlayerController(QObject* parent)
    : QObject(parent)
{
    setupDeck(m_a);
    setupDeck(m_b);

    m_current = &m_a;
    m_next = &m_b;

    connect(&m_devices, &QMediaDevices::audioOutputsChanged,
            this, &PlayerController::onAudioOutputsChanged);

    const auto outs = m_devices.audioOutputs();
    qDebug() << "Audio outputs available:" << outs.size();
    for (const auto &d : outs) {
        qDebug() << " -" << d.description();
    }
    refreshOutputs();
    emit audioOutputsChanged();

    // Apply default device to both decks after creation
    selectDefaultOutputDevice();
}

void PlayerController::setupDeck(Deck& deck)
{
    deck.audio = new QAudioOutput(this);
    deck.audio->setVolume(0.8f);
    deck.audio->setMuted(false);

    deck.player = new QMediaPlayer(this);
    deck.player->setAudioOutput(deck.audio);

    connect(deck.player, &QMediaPlayer::positionChanged, this, &PlayerController::onPositionChanged);
    connect(deck.player, &QMediaPlayer::durationChanged, this, &PlayerController::onDurationChanged);
    connect(deck.player, &QMediaPlayer::mediaStatusChanged, this, &PlayerController::onMediaStatusChanged);
    connect(deck.player, &QMediaPlayer::errorOccurred, this, &PlayerController::onPlayerErrorOccurred);
    connect(deck.player, &QMediaPlayer::playbackStateChanged, this, [&deck](QMediaPlayer::PlaybackState s){
        qDebug() << "playbackStateChanged:" << s << "device:" << (deck.audio ? deck.audio->device().description() : QString("<null>")) << "vol:" << (deck.audio ? deck.audio->volume() : -1);
    });
    if (deck.audio) {
        connect(deck.audio, &QAudioOutput::deviceChanged, this, [&deck]{
            qDebug() << "audioOutput deviceChanged:" << deck.audio->device().description();
        });
        connect(deck.audio, &QAudioOutput::volumeChanged, this, [&deck](float v){
            qDebug() << "audioOutput volumeChanged:" << v;
        });
        connect(deck.audio, &QAudioOutput::mutedChanged, this, [&deck](bool m){
            qDebug() << "audioOutput mutedChanged:" << m;
        });
    }
}

void PlayerController::openFile(const QUrl& url)
{
    qDebug() << "openFile:" << url;
    if (m_current->audio) m_current->audio->setMuted(false);
    if (m_current->player && m_current->audio)
        m_current->player->setAudioOutput(m_current->audio);
    m_current->player->setSource(url);
    m_current->player->play();
    qDebug() << "after play, state:" << m_current->player->playbackState() << "status:" << m_current->player->mediaStatus();
    m_gaplessArmed = false;
    emit playingChanged();
    emit currentSourceChanged();
}

void PlayerController::setNextFile(const QUrl& url)
{
    qDebug() << "setNextFile:" << url;
    m_next->source = url;
    m_next->player->setSource(url);
    m_next->player->pause();
    m_gaplessArmed = false;
}

void PlayerController::play()
{
    qDebug() << "play()";
    if (m_current->audio) {
        m_current->audio->setMuted(false);
        m_current->player->setAudioOutput(m_current->audio);
    }
    m_current->player->play();
    qDebug() << "after play(), state:" << m_current->player->playbackState() << "status:" << m_current->player->mediaStatus();
    m_gaplessArmed = false;
    emit playingChanged();
}

void PlayerController::pause()
{
    qDebug() << "pause()";
    m_current->player->pause();
    emit playingChanged();
}

void PlayerController::seek(qint64 posMs)
{
    m_current->player->setPosition(posMs);
}

bool PlayerController::playing() const
{
    return m_current->player->playbackState() == QMediaPlayer::PlayingState;
}

qint64 PlayerController::position() const
{
    return m_current->player->position();
}

qint64 PlayerController::duration() const
{
    return m_current->player->duration();
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
        switchToNext();
    }
    qDebug() << "mediaStatusChanged:" << status << "pos/dur" << m_current->player->position() << "/" << m_current->player->duration();
}

void PlayerController::switchToNext()
{
    if (!m_next->player->source().isEmpty()) {
        // swap decks
        std::swap(m_current, m_next);
        m_current->player->play();
        m_gaplessArmed = false;
        emit playingChanged();
        emit currentSourceChanged();
    }
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
        qDebug() << "No audio output devices detected";
        return;
    }
    auto dev = m_devices.defaultAudioOutput();
    if (m_a.audio) m_a.audio->setDevice(dev);
    if (m_b.audio) m_b.audio->setDevice(dev);
    if (m_a.player && m_a.audio) m_a.player->setAudioOutput(m_a.audio);
    if (m_b.player && m_b.audio) m_b.player->setAudioOutput(m_b.audio);
    qDebug() << "Using audio device:" << dev.description();
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
    qDebug() << "Switched audio device to:" << dev.description();
    emit audioOutputsChanged();
}

void PlayerController::refreshOutputs()
{
    m_outputDevices = m_devices.audioOutputs();
    qDebug() << "Refreshed audio outputs:" << m_outputDevices.size();
    for (const auto &d : m_outputDevices)
        qDebug() << " *" << d.description();
}

void PlayerController::onPlayerErrorOccurred(QMediaPlayer::Error error, const QString &errorString)
{
    qDebug() << "Player error:" << error << errorString;
}

void PlayerController::refreshAudioDevices()
{
    refreshOutputs();
    emit audioOutputsChanged();
}
