#include "BufferIODevice.h"
#include "SPSCRingBuffer.h"
#include <cstring>

BufferIODevice::BufferIODevice(SPSCRingBuffer *buffer, float initialVolume, QObject *parent)
    : QIODevice(parent)
    , m_buffer(buffer)
    , m_volume(qBound(0.0f, initialVolume, 1.0f))
{
    open(QIODevice::ReadOnly);
}

void BufferIODevice::setArmed(bool armed)
{
    m_armed.store(armed, std::memory_order_release);
}

bool BufferIODevice::isArmed() const
{
    return m_armed.load(std::memory_order_acquire);
}

void BufferIODevice::setVolume(float volume)
{
    m_volume.store(qBound(0.0f, volume, 1.0f), std::memory_order_relaxed);
}

float BufferIODevice::volume() const
{
    return m_volume.load(std::memory_order_relaxed);
}

qint64 BufferIODevice::readData(char *data, qint64 maxSize)
{
    if (!m_armed.load(std::memory_order_acquire)) {
        std::memset(data, 0, static_cast<size_t>(maxSize));
        return maxSize;
    }

    // RingBuffer::read() now handles silence filling internally
    // It always returns maxSize bytes (real data + silence padding)
    qint64 bytesRead = static_cast<qint64>(m_buffer->read(reinterpret_cast<uint8_t*>(data), 
                                                          static_cast<size_t>(maxSize)));
    
    // Apply software volume control (S16 samples)
    // This avoids PipeWire/WirePlumber volume stacking issues
    const float volume = m_volume.load(std::memory_order_relaxed);
    if (bytesRead > 0 && volume <= 0.0001f) {
        std::memset(data, 0, static_cast<size_t>(bytesRead));
    } else if (volume < 0.999f && bytesRead > 0) {
        int16_t *samples = reinterpret_cast<int16_t*>(data);
        qint64 sampleCount = bytesRead / sizeof(int16_t);
        for (qint64 i = 0; i < sampleCount; ++i) {
            samples[i] = static_cast<int16_t>(samples[i] * volume);
        }
    }
    
    return bytesRead;
}

qint64 BufferIODevice::writeData(const char *, qint64)
{
    return -1;
}

qint64 BufferIODevice::bytesAvailable() const
{
    return static_cast<qint64>(m_buffer->availableToRead()) + QIODevice::bytesAvailable();
}
