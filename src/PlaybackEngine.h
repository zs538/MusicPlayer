#pragma once

#include <QObject>
#include <QUrl>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QAudioSink>
#include <QAudioFormat>
#include <QAudioDevice>
#include <QIODevice>
#include <QTimer>
#include <atomic>
#include <vector>

// Forward declarations for FFmpeg types
struct AVFormatContext;
struct AVCodecContext;
struct SwrContext;
struct AVPacket;
struct AVFrame;

/**
 * @brief Reference to a track for playback.
 * 
 * Lightweight struct containing the URL, playlist index, and optional display name.
 * The playlistIndex is used to uniquely identify tracks (handles duplicate files).
 * Used by PlaybackEngine to identify tracks without owning playlist logic.
 */
struct TrackRef {
    QUrl url;
    QString displayName;  // Optional, for debug/logging
    int playlistIndex {-1};  // Index in the playlist (-1 if not from playlist)
    
    bool isValid() const { return !url.isEmpty(); }
    // Compare by index if both have valid indices, otherwise by URL
    bool operator==(const TrackRef& other) const { 
        if (playlistIndex >= 0 && other.playlistIndex >= 0) {
            return playlistIndex == other.playlistIndex;
        }
        return url == other.url; 
    }
    bool operator!=(const TrackRef& other) const { return !(*this == other); }
};
Q_DECLARE_METATYPE(TrackRef)

/**
 * @brief Playback mode controlling resampling and gapless behavior.
 */
enum class PlaybackMode {
    GaplessSession,      ///< Always gapless; resample/convert to session format as needed.
    BitPerfectSameRate   ///< Bit-perfect and gapless within same-format runs; no resampling.
};
Q_DECLARE_METATYPE(PlaybackMode)

/**
 * @brief Audio buffer profile controlling latency vs stability.
 */
enum class BufferProfile {
    Short,  ///< Low latency (~100ms), minimal audio tail
    Long    ///< Safer on slow systems (~250ms)
};
Q_DECLARE_METATYPE(BufferProfile)

/**
 * @brief Audio format information for a track.
 */
struct AudioFormat {
    int sampleRate {0};
    int channels {0};
    int bitsPerSample {0};
    
    bool isValid() const { return sampleRate > 0 && channels > 0 && bitsPerSample > 0; }
    bool isCompatibleWith(const AudioFormat& other) const {
        return sampleRate == other.sampleRate && 
               channels == other.channels && 
               bitsPerSample == other.bitsPerSample;
    }
};

// Forward declaration
class PlaybackEngine;

/**
 * @brief SPSC ring buffer for audio data.
 */
class AudioBuffer {
public:
    explicit AudioBuffer(size_t capacityBytes = 2 * 1024 * 1024);
    
    size_t write(const char* data, size_t bytes);
    size_t read(char* data, size_t bytes);
    size_t available() const;
    size_t freeSpace() const;
    void clear();
    void abort();
    void reset();  // Clear abort flag
    
private:
    std::vector<char> m_buffer;
    size_t m_readPos {0};
    size_t m_writePos {0};
    mutable QMutex m_mutex;
    QWaitCondition m_spaceAvailable;
    std::atomic<bool> m_aborted {false};
};

/**
 * @brief QIODevice adapter for QAudioSink to read from AudioBuffer.
 */
class BufferIODevice : public QIODevice {
    Q_OBJECT
public:
    explicit BufferIODevice(AudioBuffer* buffer, QObject* parent = nullptr);
    qint64 readData(char* data, qint64 maxSize) override;
    qint64 writeData(const char* data, qint64 maxSize) override;
    qint64 bytesAvailable() const override;
    bool isSequential() const override { return true; }
    
private:
    AudioBuffer* m_buffer;
};

/**
 * @brief Decoder worker running in a background thread.
 */
class DecoderWorker : public QObject {
    Q_OBJECT
public:
    explicit DecoderWorker(PlaybackEngine* engine);
    
public slots:
    void startDecoding(const TrackRef& track, bool isNext, qint64 seekPositionMs = 0);
    void stopDecoding();
    void abortDecoding();
    
signals:
    void decodeStarted(const TrackRef& track, qint64 durationMs, const AudioFormat& format);
    void decodeFinished(const TrackRef& track, qint64 samplesDecoded);
    void decodeError(const QString& message);
    
private:
    bool decodeTrack(const TrackRef& track, bool isNext, qint64 seekPositionMs, qint64& samplesDecoded);
    
    PlaybackEngine* m_engine;
    std::atomic<bool> m_stopRequested {false};
    std::atomic<bool> m_abortRequested {false};
};

/**
 * @brief New PlaybackEngine using FFmpeg for decoding and QAudioSink for output.
 * 
 * This engine follows the SimplifiedAudioEnginePlan.md design:
 * - Knows only about current track and optional next track for gapless
 * - Supports two playback modes: GaplessSession and BitPerfectSameRate
 * - Does not own playlist/queue logic (that's PlaybackQueue's job)
 * - Emits trackAboutToFinish for gapless preparation
 */
class PlaybackEngine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(PlaybackMode mode READ mode WRITE setMode NOTIFY modeChanged)

public:
    enum class State { Stopped, Playing, Paused };
    Q_ENUM(State)

    explicit PlaybackEngine(QObject* parent = nullptr);
    ~PlaybackEngine() override;

    // Configuration
    void setMode(PlaybackMode mode);
    PlaybackMode mode() const { return m_mode; }

    void setBufferProfile(BufferProfile profile);
    BufferProfile bufferProfile() const { return m_bufferProfile; }

    void setOutputDevice(const QAudioDevice& device);
    QAudioDevice outputDevice() const { return m_outputDevice; }

    // Core control
    void play(const TrackRef& track);
    void prepareNext(const TrackRef& track);
    void clearNext();

    void pause();
    void resume();
    void stop();
    void seek(qint64 positionMs);

    // Queries
    State state() const { return m_state; }
    qint64 position() const;
    qint64 duration() const { return m_currentDurationMs; }
    TrackRef currentTrack() const { return m_currentTrack; }
    
    // Volume (only effective in GaplessSession mode)
    float volume() const { return m_volume; }
    void setVolume(float v);

signals:
    void stateChanged(PlaybackEngine::State state);
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);

    void trackChanged(const TrackRef& track);
    void trackAboutToFinish(qint64 msRemaining);
    void trackFinished();

    void modeChanged(PlaybackMode mode);
    void formatMismatchForNext(const AudioFormat& currentFormat, const AudioFormat& nextFormat);
    void errorOccurred(const QString& message);

private:
    friend class DecoderWorker;  // Allow DecoderWorker to access private members

    // Configuration
    PlaybackMode m_mode {PlaybackMode::GaplessSession};
    BufferProfile m_bufferProfile {BufferProfile::Short};
    State m_state {State::Stopped};
    float m_volume {0.8f};

    // Output device
    QAudioDevice m_outputDevice;
    QAudioSink* m_audioSink {nullptr};
    QAudioFormat m_sessionFormat;
    AudioFormat m_currentTrackFormat;

    // Buffer and I/O
    AudioBuffer m_buffer;
    BufferIODevice* m_bufferDevice {nullptr};

    // Decoder thread
    QThread* m_decodeThread {nullptr};
    DecoderWorker* m_decoder {nullptr};

    // Track state
    TrackRef m_currentTrack;
    TrackRef m_nextTrack;
    bool m_hasNextPrepared {false};
    bool m_nextLocked {false};  // True after lead-in, next track is locked for this transition
    
    qint64 m_currentDurationMs {0};
    qint64 m_currentTrackSamples {0};  // Total samples in current track
    std::atomic<qint64> m_samplesWritten {0};  // Samples written to buffer for current track
    qint64 m_trackStartSample {0};  // Sample offset where current track started in the session
    qint64 m_seekOffsetMs {0};  // Offset added to position after seek
    
    // Next track info (stored during gapless transition)
    qint64 m_nextTrackDurationMs {0};
    AudioFormat m_nextTrackFormat;
    
    // Position tracking
    QTimer* m_positionTimer {nullptr};
    static constexpr int POSITION_UPDATE_INTERVAL_MS = 50;
    static constexpr int GAPLESS_LEAD_IN_MS = 300;  // Start decoding next track this early

    // =========================================================================
    // Private methods
    // =========================================================================

    void setupAudioOutput();
    void startDecoderThread();
    void stopDecoderThread();
    void updatePosition();
    void checkTrackTransition();
    void performTrackTransition();
    
    QAudioFormat determineSessionFormat();
    
    // Helpers
    qint64 samplesToMs(qint64 samples) const;
    qint64 msToSamples(qint64 ms) const;
    int bytesPerFrame() const;

private slots:
    void onDecodeStarted(const TrackRef& track, qint64 durationMs, const AudioFormat& format);
    void onDecodeFinished(const TrackRef& track, qint64 samplesDecoded);
    void onDecodeError(const QString& message);
    void onAudioStateChanged(QAudio::State state);
};
