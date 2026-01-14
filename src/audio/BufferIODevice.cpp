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
    return static_cast<qint64>(m_buffer->read(reinterpret_cast<uint8_t*>(data), 
                                               static_cast<size_t>(maxSize)));
}

qint64 BufferIODevice::writeData(const char *, qint64)
{
    return -1;
}

qint64 BufferIODevice::bytesAvailable() const
{
    return static_cast<qint64>(m_buffer->availableToRead()) + QIODevice::bytesAvailable();
}
