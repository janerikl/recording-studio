#pragma once

#include <algorithm>
#include <atomic>
#include <memory>

#include "audio/Effects.h"

namespace rsd {

// The final mix stage: every track's (and bus track's) contribution sums
// here, then this volume + effect chain apply before output. Not a Track
// (no clips/pan/mute/solo/sends — those concepts don't apply to the final
// output), but reuses the same copy-on-write effect-chain pattern since
// it's read lock-free by the audio thread every callback.
class MasterBus {
public:
    std::atomic<float> volume{1.0f};

    MasterBus() { m_effects.store(std::make_shared<const EffectChain>()); }

    std::shared_ptr<const EffectChain> effectsSnapshot() const { return m_effects.load(); }
    void restoreEffects(std::shared_ptr<const EffectChain> snapshot) {
        m_effects.store(std::move(snapshot));
    }

    void addEffect(std::shared_ptr<Effect> effect) {
        auto current = m_effects.load();
        auto updated = std::make_shared<EffectChain>(*current);
        updated->push_back(std::move(effect));
        m_effects.store(std::const_pointer_cast<const EffectChain>(updated));
    }

    void removeEffect(const QUuid& effectId) {
        auto current = m_effects.load();
        auto updated = std::make_shared<EffectChain>(*current);
        updated->erase(std::remove_if(updated->begin(), updated->end(),
                                       [&](const auto& e) { return e->id == effectId; }),
                        updated->end());
        m_effects.store(std::const_pointer_cast<const EffectChain>(updated));
    }

    // Mirrors Track::moveEffect.
    void moveEffect(const QUuid& effectId, int newIndex) {
        auto current = m_effects.load();
        auto updated = std::make_shared<EffectChain>(*current);
        auto it = std::find_if(updated->begin(), updated->end(),
                                [&](const auto& e) { return e->id == effectId; });
        if (it == updated->end()) return;
        auto effect = *it;
        updated->erase(it);
        newIndex = std::clamp(newIndex, 0, static_cast<int>(updated->size()));
        updated->insert(updated->begin() + newIndex, effect);
        m_effects.store(std::const_pointer_cast<const EffectChain>(updated));
    }

private:
    std::atomic<std::shared_ptr<const EffectChain>> m_effects;
};

} // namespace rsd
