#include "SPSCRingBuffer.h"
#include <algorithm>
#include <thread>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#endif

SPSCRingBuffer::SPSCRingBuffer(size_t capacity)
    : m_buffer(capacity)
    , m_capacity(capacity)
{
}

size_t SPSCRingBuffer::availableToRead() const
{
    size_t w = m_writePos.load(std::memory_order_acquire);
    size_t r = m_readPos.load(std::memory_order_relaxed);
    return (w >= r) ? (w - r) : (m_capacity - r + w);
}

size_t SPSCRingBuffer::availableToWrite() const
{
    size_t w = m_writePos.load(std::memory_order_relaxed);
    size_t r = m_readPos.load(std::memory_order_acquire);
    size_t used = (w >= r) ? (w - r) : (m_capacity - r + w);
    return m_capacity - used - 1;  // -1 to distinguish full from empty
}

// Producer: blocking write
size_t SPSCRingBuffer::write(const uint8_t *data, size_t size)
{
    if (m_aborted.load(std::memory_order_relaxed))
        return 0;
    
    size_t written = 0;
    while (written < size) {
        if (m_aborted.load(std::memory_order_relaxed))
            return written;
        
        size_t space = availableToWrite();
        if (space == 0) {
            waitForSpace();
            continue;
        }
        
        size_t toWrite = std::min(size - written, space);
        size_t w = m_writePos.load(std::memory_order_relaxed);
        
        // Handle wrap-around
        size_t firstPart = std::min(toWrite, m_capacity - w);
        std::memcpy(m_buffer.data() + w, data + written, firstPart);
        
        if (toWrite > firstPart) {
            std::memcpy(m_buffer.data(), data + written + firstPart, toWrite - firstPart);
        }
        
        // Update write position with release semantics (makes data visible to consumer)
        size_t newW = (w + toWrite) % m_capacity;
        m_writePos.store(newW, std::memory_order_release);
        written += toWrite;
    }
    
    return written;
}

// Consumer: non-blocking read, pads with silence
size_t SPSCRingBuffer::read(uint8_t *data, size_t size)
{
    size_t avail = availableToRead();
    
    if (avail == 0) {
        // No data, return silence
        std::memset(data, 0, size);
        return size;
    }
    
    size_t toRead = std::min(size, avail);
    size_t r = m_readPos.load(std::memory_order_relaxed);
    
    // Handle wrap-around
    size_t firstPart = std::min(toRead, m_capacity - r);
    std::memcpy(data, m_buffer.data() + r, firstPart);
    
    if (toRead > firstPart) {
        std::memcpy(data + firstPart, m_buffer.data(), toRead - firstPart);
    }
    
    // Update read position with release semantics
    size_t newR = (r + toRead) % m_capacity;
    m_readPos.store(newR, std::memory_order_release);
    
    // Pad remainder with silence
    if (toRead < size) {
        std::memset(data + toRead, 0, size - toRead);
    }
    
    // Signal producer that space is available
    signalSpace();
    
    return size;
}

void SPSCRingBuffer::waitForSpace()
{
    // Simple spin-wait with backoff, then yield
    // This is only called when buffer is full (rare in normal playback)
    m_producerWaiting.store(true, std::memory_order_relaxed);
    
    for (int i = 0; i < 100; ++i) {
        if (m_aborted.load(std::memory_order_relaxed))
            break;
        if (availableToWrite() > 0)
            break;
        // Brief pause
        std::this_thread::yield();
    }
    
    // If still no space, sleep briefly
    while (!m_aborted.load(std::memory_order_relaxed) && availableToWrite() == 0) {
#ifdef _WIN32
        Sleep(1);
#else
        std::this_thread::sleep_for(std::chrono::microseconds(500));
#endif
    }
    
    m_producerWaiting.store(false, std::memory_order_relaxed);
}

void SPSCRingBuffer::signalSpace()
{
    // No-op for now; producer will wake on next spin iteration
    // Could use futex/condition_variable for lower latency wakeup if needed
}

void SPSCRingBuffer::clear()
{
    m_readPos.store(0, std::memory_order_relaxed);
    m_writePos.store(0, std::memory_order_relaxed);
    m_finished.store(false, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
}

void SPSCRingBuffer::setFinished(bool finished)
{
    m_finished.store(finished, std::memory_order_release);
}

bool SPSCRingBuffer::isFinished() const
{
    return m_finished.load(std::memory_order_acquire);
}

void SPSCRingBuffer::abort()
{
    m_aborted.store(true, std::memory_order_release);
}

void SPSCRingBuffer::reset()
{
    m_aborted.store(false, std::memory_order_relaxed);
}
