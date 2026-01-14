#ifndef SPSCRINGBUFFER_H
#define SPSCRINGBUFFER_H

#include <atomic>
#include <cstdint>
#include <cstring>
#include <vector>

/**
 * @brief Lock-free Single-Producer Single-Consumer ring buffer.
 * 
 * - Producer (decode thread) calls write()
 * - Consumer (audio callback) calls read()
 * - No mutex in the hot path; uses acquire/release atomics.
 * - Blocking write with a semaphore for backpressure (producer waits when full).
 * - Non-blocking read that pads with silence when empty (critical for gapless).
 */
class SPSCRingBuffer
{
public:
    explicit SPSCRingBuffer(size_t capacity);
    
    // Producer: write data, blocks if buffer is full until space available or aborted.
    // Returns bytes written (0 if aborted).
    size_t write(const uint8_t *data, size_t size);
    
    // Consumer: read data, never blocks. Pads with silence if not enough data.
    // Always returns `size` bytes.
    size_t read(uint8_t *data, size_t size);
    
    // Lock-free queries (approximate, safe for cross-thread)
    size_t availableToRead() const;
    size_t availableToWrite() const;
    size_t capacity() const { return m_capacity; }
    
    // State management (called from main thread, not hot path)
    void clear();
    void setFinished(bool finished);
    bool isFinished() const;
    void abort();   // Unblock waiting producer
    void reset();   // Clear abort state

private:
    void waitForSpace();
    void signalSpace();
    
    std::vector<uint8_t> m_buffer;
    size_t m_capacity;
    
    // Atomic indices for lock-free SPSC
    alignas(64) std::atomic<size_t> m_writePos{0};
    alignas(64) std::atomic<size_t> m_readPos{0};
    
    // Blocking/signaling for producer backpressure (not in consumer hot path)
    std::atomic<bool> m_finished{false};
    std::atomic<bool> m_aborted{false};
    
    // Simple futex-like signaling for producer wait
    std::atomic<bool> m_producerWaiting{false};
};

#endif // SPSCRINGBUFFER_H
