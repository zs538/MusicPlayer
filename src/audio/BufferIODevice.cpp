#include "BufferIODevice.h"
#include "SPSCRingBuffer.h"
#include <cstring>

BufferIODevice::BufferIODevice(SPSCRingBuffer *buffer, QObject *parent)
    : QIODevice(parent)
    , m_buffer(buffer)
{
    open(QIODevice::ReadOnly);
}

qint64 BufferIODevice::readData(char *data, qint64 maxSize)
{
    // RingBuffer::read() now handles silence filling internally
    // It always returns maxSize bytes (real data + silence padding)
    qint64 bytesRead = static_cast<qint64>(m_buffer->read(reinterpret_cast<uint8_t*>(data), 
                                                          static_cast<size_t>(maxSize)));
    
    // Apply software volume control (S16 samples)
    // This avoids PipeWire/WirePlumber volume stacking issues
    if (m_volume < 0.999f && bytesRead > 0) {
        int16_t *samples = reinterpret_cast<int16_t*>(data);
        qint64 sampleCount = bytesRead / sizeof(int16_t);
        for (qint64 i = 0; i < sampleCount; ++i) {
            samples[i] = static_cast<int16_t>(samples[i] * m_volume);
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
