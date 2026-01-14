#ifndef AUDIOENGINE_H
#define AUDIOENGINE_H

#include <QObject>
#include <QAudioSink>
#include <QAudioFormat>
#include <QIODevice>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QTimer>
#include <QMediaDevices>
#include <atomic>
#include <memory>

struct AVFormatContext;
struct AVCodecContext;
struct SwrContext;
struct AVFrame;
struct AVPacket;

class SPSCRingBuffer;
class BufferIODevice;

class AudioEngine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int state READ state NOTIFY stateChanged)
    Q_PROPERTY(qint64 positionMs READ positionMs NOTIFY positionMsChanged)
    Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY durationMsChanged)

public:
    enum State {
        Stopped = 0,
        Playing,
        Paused,
        Buffering,
        Error
    };
    Q_ENUM(State)
    
    enum PlaybackMode {
        GaplessSession = 0,
        BitPerfectSameRate
    };
    Q_ENUM(PlaybackMode)
    
    // Audio sink buffer size - affects latency and overlap at track transitions
    // Short = 100ms (low latency, minimal overlap)
    // Long = 250ms (safer on slow systems)
    Q_PROPERTY(int sinkBufferMs READ sinkBufferMs WRITE setSinkBufferMs NOTIFY sinkBufferMsChanged)
    
    explicit AudioEngine(QObject *parent = nullptr);
    ~AudioEngine();
    
    State state() const { return m_state; }
    qint64 positionMs() const;
    qint64 durationMs() const { return m_durationMs; }
    QString currentFilePath() const;
    PlaybackMode playbackMode() const { return m_playbackMode; }
    void setPlaybackMode(PlaybackMode mode);
    
    int sinkBufferMs() const { return m_sinkBufferMs; }
    void setSinkBufferMs(int ms);

signals:
    void sinkBufferMsChanged(int ms);

public slots:
    bool play(const QString &filePath);
    void prepareNext(const QString &filePath);
    void clearNext();
    void pause();
    void resume();
    void stop();
    void seek(qint64 positionMs);
    void setVolume(double volume);

signals:
    void stateChanged(AudioEngine::State state);
    void positionMsChanged(qint64 positionMs);
    void durationMsChanged(qint64 durationMs);
    void trackAboutToFinish(qint64 msRemaining);
    void trackChanged(const QString &filePath);
    void trackFinished();
    void errorOccurred(int code, const QString &message);

private slots:
    void onAudioStateChanged(QAudio::State state);
    void updatePosition();

private:
    void setState(State state);
    bool openFile(const QString &filePath, qint64 *outDurationMs = nullptr);
    bool preopenNextFile(const QString &filePath, qint64 *outDurationMs);
    void closeFile();
    void closeNextFile();
    void startDecodeThread();
    void stopDecodeThread();
    void decodeLoop();
    void checkGaplessTransition();
    bool initAudioOutput(int sampleRate, int channels);
    
    State m_state = Stopped;
    QString m_currentFilePath;
    qint64 m_durationMs = 0;
    double m_volume = 1.0;
    
    AVFormatContext *m_formatCtx = nullptr;
    AVCodecContext *m_codecCtx = nullptr;
    SwrContext *m_swrCtx = nullptr;
    int m_audioStreamIndex = -1;

    AVFormatContext *m_nextFormatCtx = nullptr;
    AVCodecContext *m_nextCodecCtx = nullptr;
    SwrContext *m_nextSwrCtx = nullptr;
    int m_nextAudioStreamIndex = -1;
    QString m_nextPreopenedPath;
    
    std::unique_ptr<QAudioSink> m_audioSink;
    QAudioFormat m_audioFormat;
    
    std::unique_ptr<SPSCRingBuffer> m_ringBuffer;
    std::unique_ptr<BufferIODevice> m_bufferDevice;
    std::unique_ptr<QThread> m_decodeThread;
    std::atomic<bool> m_stopDecoding{false};
    
    std::atomic<qint64> m_samplesWritten{0};  // Total samples written to buffer
    int m_outputSampleRate = 44100;
    qint64 m_trackStartSample = 0;  // Sample offset where current track starts in output stream
    qint64 m_seekOffsetMs = 0;      // Offset added to position after seek
    qint64 m_currentTrackSamples = 0;  // Total samples in current track (for end detection)
    
    QTimer *m_positionTimer = nullptr;
    
    // Accessed from both UI thread (prepareNext/clearNext/updatePosition) and decode thread.
    // Must be protected to avoid undefined behavior and intermittent clicks/glitches.
    mutable QMutex m_nextMutex;
    QString m_nextFilePath;
    bool m_trackAboutToFinishEmitted = false;
    bool m_nextLocked = false;      // True when next track is locked for gapless transition
    bool m_hasNextPrepared = false; // True when next track path is set
    qint64 m_nextTrackDurationMs = 0;
    static constexpr qint64 GAPLESS_LEAD_IN_MS = 2000;
    
    PlaybackMode m_playbackMode = GaplessSession;
    int m_sinkBufferMs = 100;  // Default: 100ms (low latency)
};

#endif // AUDIOENGINE_H
