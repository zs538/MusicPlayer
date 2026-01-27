#ifndef AUDIOOUTPUTWORKER_H
#define AUDIOOUTPUTWORKER_H

#include <QObject>
#include <QAudioSink>
#include <QAudioFormat>
#include <QTimer>
#include <memory>

class SPSCRingBuffer;
class BufferIODevice;

/**
 * @brief Audio output worker that runs QAudioSink in a dedicated thread.
 * 
 * This class owns the QAudioSink and BufferIODevice, running them in a separate
 * high-priority thread to ensure audio output is completely decoupled from the
 * GUI thread. This prevents UI stalls (compositor sync, layout, rendering) from
 * causing audio glitches.
 * 
 * Communication with AudioEngine is via queued signals/slots (thread-safe).
 */
class AudioOutputWorker : public QObject
{
    Q_OBJECT

public:
    explicit AudioOutputWorker(SPSCRingBuffer *ringBuffer, QObject *parent = nullptr);
    ~AudioOutputWorker();

public slots:
    // Called from AudioEngine (queued connection from GUI thread)
    void initialize(int sampleRate, int channels, int bufferMs);
    void start();
    void suspend();
    void resume();
    void stop();

signals:
    // Emitted to AudioEngine (queued connection to GUI thread)
    void initialized(bool success);
    void stateChanged(QAudio::State state);
    void positionUpdated(qint64 processedUSecs);
    void errorOccurred(const QString &message);

private slots:
    void onAudioStateChanged(QAudio::State state);
    void pushPosition();

private:
    SPSCRingBuffer *m_ringBuffer = nullptr;
    std::unique_ptr<QAudioSink> m_audioSink;
    std::unique_ptr<BufferIODevice> m_bufferDevice;
    QAudioFormat m_audioFormat;
    QTimer *m_positionTimer = nullptr;
    int m_bufferMs = 100;
    bool m_running = false;  // Only accessed from audio thread
};

#endif // AUDIOOUTPUTWORKER_H
