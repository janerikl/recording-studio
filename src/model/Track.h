#pragma once

#include <QUuid>
#include <QString>
#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>

#include "Clip.h"

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
    float gain = 1.0f;

    Track() { m_clips.store(std::make_shared<const ClipList>()); }

    // GUI thread only.
    void addClip(std::shared_ptr<Clip> clip) {
        auto current = m_clips.load();
        auto updated = std::make_shared<ClipList>(*current);
        updated->push_back(std::move(clip));
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

private:
    std::atomic<std::shared_ptr<const ClipList>> m_clips;
};

} // namespace rsd
