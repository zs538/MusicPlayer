#pragma once

#include <QObject>
#include <QUrl>
#include <QTimer>
#include <QMediaPlayer>
#include <QAudioOutput>

class PlayerController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(float volume READ volume WRITE setVolume NOTIFY volumeChanged)
public:
    explicit PlayerController(QObject* parent = nullptr);

    Q_INVOKABLE void openFile(const QUrl& url);
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void seek(qint64 posMs);
    Q_INVOKABLE void setNextFile(const QUrl& url);

    bool playing() const;
    qint64 position() const;
    qint64 duration() const;
    float volume() const;
    void setVolume(float v);

signals:
    void playingChanged();
    void positionChanged();
    void durationChanged();
    void volumeChanged();
    void currentSourceChanged();

private slots:
    void onPositionChanged();
    void onDurationChanged();
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);

private:
    struct Deck {
        QMediaPlayer* player {nullptr};
        QAudioOutput* audio {nullptr};
        QUrl source;
    };

    Deck m_a;
    Deck m_b;
    Deck* m_current {nullptr};
    Deck* m_next {nullptr};
    bool m_gaplessArmed {false};

    void setupDeck(Deck& deck);
    void switchToNext();
};
