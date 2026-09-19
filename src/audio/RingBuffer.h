#pragma once

#include <atomic>
#include <cstddef>
#include <vector>

namespace rsd {

// Lock-free single-producer/single-consumer ring buffer. The audio callback
// (producer) writes captured samples; the GUI thread (consumer) drains them
// on a timer. Never locks, never allocates after construction.
template <typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity) : m_buffer(capacity + 1) {}

    size_t write(const T* data, size_t count) {
        size_t written = 0;
        size_t w = m_writeIdx.load(std::memory_order_relaxed);
        const size_t r = m_readIdx.load(std::memory_order_acquire);

        while (written < count) {
            size_t next = (w + 1) % m_buffer.size();
            if (next == r) break; // full
            m_buffer[w] = data[written];
            w = next;
            ++written;
        }
        m_writeIdx.store(w, std::memory_order_release);
        return written;
    }

    size_t read(T* out, size_t maxCount) {
        size_t readCount = 0;
        size_t r = m_readIdx.load(std::memory_order_relaxed);
        const size_t w = m_writeIdx.load(std::memory_order_acquire);

        while (readCount < maxCount && r != w) {
            out[readCount] = m_buffer[r];
            r = (r + 1) % m_buffer.size();
            ++readCount;
        }
        m_readIdx.store(r, std::memory_order_release);
        return readCount;
    }

private:
    std::vector<T> m_buffer;
    std::atomic<size_t> m_writeIdx{0};
    std::atomic<size_t> m_readIdx{0};
};

} // namespace rsd
