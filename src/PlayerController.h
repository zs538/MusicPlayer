#pragma once

#include <QObject>
#include <QUrl>
#include <QStringList>
#include <QVector>
#include <QMediaDevices>
#include <QAudioDevice>

#include "PlaybackEngine.h"
#include "PlaybackQueue.h"
#include "TrackMetadata.h"

class PlaylistModel;

/**
 * @brief Orchestrates playback between PlaybackEngine and PlaybackQueue.
 * 
 * PlayerController is the QML-facing facade that:
 * - Forwards UI commands to PlaybackQueue and PlaybackEngine
 * - Reacts to engine signals (trackAboutToFinish, trackFinished, etc.)
 * - Manages metadata and audio device selection
 * - Provides a stable API for QML bindings
 */
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
    Q_PROPERTY(int repeatMode READ repeatMode WRITE setRepeatMode NOTIFY repeatModeChanged)
    Q_PROPERTY(bool shuffle READ shuffle WRITE setShuffle NOTIFY shuffleChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(int bufferProfile READ bufferProfile WRITE setBufferProfile NOTIFY bufferProfileChanged)

public:
    explicit PlayerController(QObject* parent = nullptr);
    ~PlayerController() override;

    // Playback control (QML-invokable)
    Q_INVOKABLE void openFile(const QUrl& url);
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void seek(qint64 posMs);
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    
    // Play specific index in playlist
    Q_INVOKABLE void playIndex(int index);
    
    // Audio device selection
    Q_INVOKABLE void selectOutputByIndex(int index);
    Q_INVOKABLE void refreshAudioDevices();
    
    // Playlist integration
    void setPlaylistModel(PlaylistModel* model);
    PlaybackQueue* queue() { return m_queue; }

    // Property getters
    bool playing() const;
    qint64 position() const;
    qint64 duration() const;
    float volume() const;
    void setVolume(float v);
    QUrl currentSource() const;
    TrackMetadata* currentMetadata() const { return m_currentMetadata; }
    
    // Repeat/shuffle
    int repeatMode() const;
    void setRepeatMode(int mode);
    bool shuffle() const;
    void setShuffle(bool enabled);
    
    // Current index in playlist
    int currentIndex() const;

    // Audio outputs
    QStringList audioOutputs() const;
    QString currentOutput() const;

    // Buffer profile
    int bufferProfile() const;
    void setBufferProfile(int profile);

signals:
    void playingChanged();
    void positionChanged();
    void durationChanged();
    void volumeChanged();
    void currentSourceChanged();
    void audioOutputsChanged();
    void currentMetadataChanged();
    void repeatModeChanged();
    void shuffleChanged();
    void currentIndexChanged();
    void bufferProfileChanged();

private slots:
    // Engine signal handlers
    void onEngineStateChanged(PlaybackEngine::State state);
    void onEngineTrackChanged(const TrackRef& track);
    void onEngineTrackAboutToFinish(qint64 msRemaining);
    void onEngineTrackFinished();
    void onEnginePositionChanged(qint64 positionMs);
    void onEngineDurationChanged(qint64 durationMs);
    void onEngineError(const QString& message);
    
    // Queue signal handlers
    void onQueueModified();
    
    // Device changes
    void onAudioOutputsChanged();

private:
    void refreshOutputs();
    void updateCurrentMetadataFromSource();
    void clearCurrent();
    void armNextTrack();

    PlaybackEngine* m_engine {nullptr};
    PlaybackQueue* m_queue {nullptr};
    
    float m_targetVolume {0.8f};
    QMediaDevices m_devices;
    QVector<QAudioDevice> m_outputDevices;
    TrackMetadata* m_currentMetadata {nullptr};
    PlaylistModel* m_playlistModel {nullptr};
};
