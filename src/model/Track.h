#pragma once

#include <QUuid>
#include <QString>
#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>

#include "Clip.h"
#include "audio/Effects.h"

namespace rsd {

// Clip list is stored behind an atomic shared_ptr to a const vector so the
// realtime audio callback can snapshot-read it without locking, while the
// GUI thread performs edits via copy-on-write + atomic swap.
class Track {
public:
    using ClipList = std::vector<std::shared_ptr<Clip>>;

    QUuid id = QUuid::createUuid();
    QString name;
    std::atomic<bool> muted{false};
    std::atomic<bool> soloed{false};
    std::atomic<bool> recordArmed{false};
    // Per-channel gain, not a single scalar: lets the mixer apply independent
    // left/right levels. A pan control is a UI convenience that derives both
    // of these via a linear pan law rather than being stored separately.
    std::atomic<float> gainL{1.0f};
    std::atomic<float> gainR{1.0f};

    Track() {
        m_clips.store(std::make_shared<const ClipList>());
        m_effects.store(std::make_shared<const EffectChain>());
    }

    // GUI thread only.
    void addClip(std::shared_ptr<Clip> clip) {
        auto current = m_clips.load();
        auto updated = std::make_shared<ClipList>(*current);
        updated->push_back(std::move(clip));
        m_clips.store(std::const_pointer_cast<const ClipList>(updated));
    }

    // Replaces the clip with matching id in a single atomic swap (RT-safe:
    // the audio thread never observes a clip list missing the entry mid-edit).
    void replaceClip(const QUuid& clipId, std::shared_ptr<Clip> newClip) {
        auto current = m_clips.load();
        auto updated = std::make_shared<ClipList>(*current);
        for (auto& c : *updated) {
            if (c->id == clipId) {
                c = std::move(newClip);
                break;
            }
        }
        m_clips.store(std::const_pointer_cast<const ClipList>(updated));
    }

    // Splits the clip at the given timeline sample position into two clips
    // (same underlying buffer, non-destructive) in a single atomic swap.
    // No-op if the position isn't strictly inside the clip.
    void splitClip(const QUuid& clipId, int64_t timelineSplitSample) {
        auto current = m_clips.load();
        auto updated = std::make_shared<ClipList>();
        updated->reserve(current->size() + 1);

        for (auto& c : *current) {
            if (c->id != clipId) {
                updated->push_back(c);
                continue;
            }
            int64_t clipStart = c->sessionStartSample;
            int64_t clipEnd = c->sessionStartSample + c->lengthSamples;
            if (timelineSplitSample <= clipStart || timelineSplitSample >= clipEnd) {
                updated->push_back(c);
                continue;
            }

            int64_t firstLength = timelineSplitSample - clipStart;
            int64_t secondLength = clipEnd - timelineSplitSample;

            auto first = std::make_shared<Clip>(*c);
            first->lengthSamples = firstLength;

            auto second = std::make_shared<Clip>(*c);
            second->id = QUuid::createUuid();
            second->sessionStartSample = timelineSplitSample;
            second->sourceOffsetSamples = c->sourceOffsetSamples + firstLength;
            second->lengthSamples = secondLength;

            updated->push_back(first);
            updated->push_back(second);
        }

        m_clips.store(std::const_pointer_cast<const ClipList>(updated));
    }

    void removeClip(const QUuid& clipId) {
        auto current = m_clips.load();
        auto updated = std::make_shared<ClipList>(*current);
        updated->erase(std::remove_if(updated->begin(), updated->end(),
                                       [&](const auto& c) { return c->id == clipId; }),
                        updated->end());
        m_clips.store(std::const_pointer_cast<const ClipList>(updated));
    }

    // RT-safe read: audio callback calls this once per buffer.
    std::shared_ptr<const ClipList> clipsSnapshot() const { return m_clips.load(); }

    // GUI thread only. Restores a previously captured snapshot in a single
    // atomic swap — the basis for undoing any clip-list edit (add/remove/
    // move/trim/split) without inverting each mutation's arithmetic.
    void restoreClips(std::shared_ptr<const ClipList> snapshot) { m_clips.store(std::move(snapshot)); }

    // Effect chain: same copy-on-write + atomic-swap pattern as the clip
    // list, since it's read lock-free by the audio thread every callback.
    // Structural edits (add/remove/reorder) go through these; per-effect
    // parameter tweaks mutate the effect's own atomics directly instead
    // (see Effects.h) so DSP state isn't lost mid-stream.
    std::shared_ptr<const EffectChain> effectsSnapshot() const { return m_effects.load(); }
    void restoreEffects(std::shared_ptr<const EffectChain> snapshot) {
        m_effects.store(std::move(snapshot));
    }

    // GUI thread only.
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

    // Moves the effect with matching id to `newIndex` in the chain (clamped
    // to the valid range), preserving all other effects' relative order.
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
    std::atomic<std::shared_ptr<const ClipList>> m_clips;
    std::atomic<std::shared_ptr<const EffectChain>> m_effects;
};

} // namespace rsd
