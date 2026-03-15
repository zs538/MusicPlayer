#ifndef BUFFERIODEVICE_H
#define BUFFERIODEVICE_H

#include <atomic>
#include <QIODevice>

class SPSCRingBuffer;

/**
 * @brief QIODevice adapter for QAudioSink to read from RingBuffer.
 * 
 * Critical for gapless playback: when buffer is empty, returns silence
 * instead of 0 bytes. This prevents clicks/gaps during track transitions.
 */
class BufferIODevice : public QIODevice
{
    Q_OBJECT
public:
    explicit BufferIODevice(SPSCRingBuffer *buffer, float initialVolume = 0.0f, QObject *parent = nullptr);
    
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;
    qint64 bytesAvailable() const override;
    bool isSequential() const override { return true; }
    
    void setArmed(bool armed);
    bool isArmed() const;
    void setVolume(float volume);
    float volume() const;

private:
    SPSCRingBuffer *m_buffer;
    std::atomic<bool> m_armed{false};
    std::atomic<float> m_volume{0.0f};
};

#endif // BUFFERIODEVICE_H
