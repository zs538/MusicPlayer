#ifndef BUFFERIODEVICE_H
#define BUFFERIODEVICE_H

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
    explicit BufferIODevice(SPSCRingBuffer *buffer, QObject *parent = nullptr);
    
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;
    qint64 bytesAvailable() const override;
    bool isSequential() const override { return true; }
    
    void setVolume(float volume) { m_volume = qBound(0.0f, volume, 1.0f); }
    float volume() const { return m_volume; }

private:
    SPSCRingBuffer *m_buffer;
    float m_volume = 1.0f;
};

#endif // BUFFERIODEVICE_H
