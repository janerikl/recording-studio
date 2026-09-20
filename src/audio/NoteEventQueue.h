#pragma once

#include <array>
#include <atomic>

namespace rsd {

struct NoteEvent {
    int pitch = 0;
    float velocity = 1.0f;
    bool noteOn = true;
};

// Single-producer (GUI thread, from the on-screen keyboard), single-consumer
// (RT audio thread) lock-free ring buffer for live note-on/off events. Only
// used for live audition — recorded note capture is separate GUI-thread-only
// bookkeeping (see MainWindow), so this never needs to carry timing info.
class NoteEventQueue {
public:
    static constexpr int kCapacity = 64;

    bool push(const NoteEvent& ev) {
        size_t head = m_head.load(std::memory_order_relaxed);
        size_t next = (head + 1) % kCapacity;
        if (next == m_tail.load(std::memory_order_acquire)) return false; // full, drop
        m_buffer[head] = ev;
        m_head.store(next, std::memory_order_release);
        return true;
    }

    bool pop(NoteEvent& out) {
        size_t tail = m_tail.load(std::memory_order_relaxed);
        if (tail == m_head.load(std::memory_order_acquire)) return false; // empty
        out = m_buffer[tail];
        m_tail.store((tail + 1) % kCapacity, std::memory_order_release);
        return true;
    }

private:
    std::array<NoteEvent, kCapacity> m_buffer;
    std::atomic<size_t> m_head{0};
    std::atomic<size_t> m_tail{0};
};

} // namespace rsd
