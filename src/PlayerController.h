#pragma once

#include <QObject>
#include <QUrl>
#include <QTimer>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QStringList>
#include <QVector>
#include <QMediaMetaData>

#include "TrackMetadata.h"

class PlaylistModel;

class PlayerController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(float volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(QStringList audioOutputs READ audioOutputs NOTIFY audioOutputsChanged)
    Q_PROPERTY(QString currentOutput READ currentOutput NOTIFY audioOutputsChanged)
    Q_PROPERTY(QUrl currentSource READ currentSource NOTIFY currentSourceChanged)
    Q_PROPERTY(TrackMetadata* currentMetadata READ currentMetadata NOTIFY currentMetadataChanged)
public:
    explicit PlayerController(QObject* parent = nullptr);

    Q_INVOKABLE void openFile(const QUrl& url);
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void seek(qint64 posMs);
    Q_INVOKABLE void setNextFile(const QUrl& url);
    Q_INVOKABLE void selectOutputByIndex(int index);
    Q_INVOKABLE void refreshAudioDevices();
    Q_INVOKABLE QUrl currentSource() const { return m_current && m_current->player ? m_current->player->source() : QUrl(); }
    Q_INVOKABLE TrackMetadata* currentMetadata() const { return m_currentMetadata; }

    bool playing() const;
    qint64 position() const;
    qint64 duration() const;
    float volume() const;
    void setVolume(float v);

    QStringList audioOutputs() const;
    QString currentOutput() const;

signals:
    void playingChanged();
    void positionChanged();
    void durationChanged();
    void volumeChanged();
    void currentSourceChanged();
    void audioOutputsChanged();
    void currentMetadataChanged();

private slots:
    void onPositionChanged();
    void onDurationChanged();
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onAudioOutputsChanged();
    void onPlayerErrorOccurred(QMediaPlayer::Error error, const QString &errorString);
    void onMetaDataChanged();

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
    QMediaDevices m_devices;
    QVector<QAudioDevice> m_outputDevices;
    TrackMetadata* m_currentMetadata {nullptr};

    void setupDeck(Deck& deck);
    void switchToNext();
    void selectDefaultOutputDevice();
    void refreshOutputs();
    void updateCurrentMetadataFromSource();
};
