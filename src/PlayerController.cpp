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
}

void PlayerController::setupDeck(Deck& deck)
{
    deck.audio = new QAudioOutput(this);
    deck.player = new QMediaPlayer(this);
    deck.player->setAudioOutput(deck.audio);

    connect(deck.player, &QMediaPlayer::positionChanged, this, &PlayerController::onPositionChanged);
    connect(deck.player, &QMediaPlayer::durationChanged, this, &PlayerController::onDurationChanged);
    connect(deck.player, &QMediaPlayer::mediaStatusChanged, this, &PlayerController::onMediaStatusChanged);
}

void PlayerController::openFile(const QUrl& url)
{
    m_current->player->setSource(url);
    m_current->player->play();
    m_gaplessArmed = false;
    emit playingChanged();
    emit currentSourceChanged();
}

void PlayerController::setNextFile(const QUrl& url)
{
    m_next->source = url;
    m_next->player->setSource(url);
    m_next->player->pause();
    m_gaplessArmed = false;
}

void PlayerController::play()
{
    m_current->player->play();
    m_gaplessArmed = false;
    emit playingChanged();
}

void PlayerController::pause()
{
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
    return m_current->audio->volume();
}

void PlayerController::setVolume(float v)
{
    m_current->audio->setVolume(v);
    m_next->audio->setVolume(v);
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
