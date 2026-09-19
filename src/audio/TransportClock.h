#pragma once

#include <atomic>
#include <cstdint>

namespace rsd {

enum class TransportState { Stopped, Playing, Recording };

class TransportClock {
public:
    TransportState state() const { return m_state.load(std::memory_order_acquire); }
    void setState(TransportState s) { m_state.store(s, std::memory_order_release); }

    int64_t positionSamples() const { return m_position.load(std::memory_order_acquire); }
    void setPositionSamples(int64_t pos) { m_position.store(pos, std::memory_order_release); }
    void advance(int64_t frames) { m_position.fetch_add(frames, std::memory_order_acq_rel); }

private:
    std::atomic<TransportState> m_state{TransportState::Stopped};
    std::atomic<int64_t> m_position{0};
};

} // namespace rsd
