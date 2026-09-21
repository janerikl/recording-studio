#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>

#include "PunchRegion.h"

namespace rsd {

enum class TransportState { Stopped, Playing, Recording };

class TransportClock {
public:
    TransportState state() const { return m_state.load(std::memory_order_acquire); }
    void setState(TransportState s) { m_state.store(s, std::memory_order_release); }

    int64_t positionSamples() const { return m_position.load(std::memory_order_acquire); }
    void setPositionSamples(int64_t pos) { m_position.store(pos, std::memory_order_release); }

    // Punch/loop region: when loop is enabled and the region is valid,
    // advance() wraps the playhead back to (region start - pre-roll) once it
    // reaches the region end, instead of continuing forward. Pre-roll lets
    // the performer hear context before recording resumes on each pass.
    void setPunchRegion(PunchRegion region) { m_punchRegion = region; }
    PunchRegion punchRegion() const { return m_punchRegion; }
    void setPunchLoopEnabled(bool enabled) { m_punchLoopEnabled.store(enabled, std::memory_order_release); }
    bool punchLoopEnabled() const { return m_punchLoopEnabled.load(std::memory_order_acquire); }
    void setPreRollSamples(int64_t samples) { m_preRollSamples = samples; }
    int64_t preRollSamples() const { return m_preRollSamples; }

    // Playback loop: independent of the punch/loop-recording region above.
    // When enabled, advance() wraps the playhead back to m_loopStartSample
    // once it reaches m_loopEndSample (the session's content end at the
    // moment the loop was toggled on), instead of continuing forward.
    void setPlaybackLoop(int64_t startSample, int64_t endSample) {
        m_loopStartSample = startSample;
        m_loopEndSample = endSample;
    }
    void setPlaybackLoopEnabled(bool enabled) { m_playbackLoopEnabled.store(enabled, std::memory_order_release); }
    bool playbackLoopEnabled() const { return m_playbackLoopEnabled.load(std::memory_order_acquire); }
    int64_t loopStartSample() const { return m_loopStartSample; }

    void advance(int64_t frames) {
        int64_t newPos = m_position.load(std::memory_order_acquire) + frames;
        if (m_punchLoopEnabled.load(std::memory_order_acquire) && m_punchRegion.isValid() &&
            newPos >= m_punchRegion.endSample) {
            newPos = std::max<int64_t>(0, m_punchRegion.startSample - m_preRollSamples);
        } else if (m_playbackLoopEnabled.load(std::memory_order_acquire) && m_loopEndSample > m_loopStartSample &&
                   newPos >= m_loopEndSample) {
            newPos = m_loopStartSample;
        }
        m_position.store(newPos, std::memory_order_release);
    }

private:
    std::atomic<TransportState> m_state{TransportState::Stopped};
    std::atomic<int64_t> m_position{0};
    // PunchRegion is two plain int64_t's; not read/written concurrently with
    // recording start in practice (set before arming), so no atomics needed.
    PunchRegion m_punchRegion;
    std::atomic<bool> m_punchLoopEnabled{false};
    int64_t m_preRollSamples = 0;

    // Same non-atomic reasoning as PunchRegion above: set before playback
    // starts, not mutated concurrently with advance() on the audio thread.
    int64_t m_loopStartSample = 0;
    int64_t m_loopEndSample = 0;
    std::atomic<bool> m_playbackLoopEnabled{false};
};

} // namespace rsd
