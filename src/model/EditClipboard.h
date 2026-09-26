#pragma once

#include <QUuid>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

#include "Clip.h"
#include "MidiNote.h"

namespace rsd {

// Copy/paste of a time range of a track's content (Clips or MidiNotes)
// between the same or a different track. Pure list transforms, independent
// of Track/CommandStack/UI, so they're unit-testable directly; callers
// capture before/after Track snapshots around extractRange()/pasteRange()
// to push the existing TrackClipsCommand/TrackMidiCommand for undo.

template <typename Item>
int64_t itemStart(const Item& item) {
    if constexpr (std::is_same_v<Item, Clip>) return item.sessionStartSample;
    else return item.startSample;
}

template <typename Item>
void setItemStart(Item& item, int64_t newStart) {
    if constexpr (std::is_same_v<Item, Clip>) item.sessionStartSample = newStart;
    else item.startSample = newStart;
}

template <typename Item>
int64_t itemEnd(const Item& item) {
    return itemStart(item) + item.lengthSamples;
}

template <typename Item>
struct RangeClipboard {
    int64_t baseSample = 0;  // start of the copied range
    int64_t spanSamples = 0; // copied range length; sizes the paste's overwrite region
    std::vector<std::shared_ptr<Item>> items;

    bool empty() const { return items.empty(); }
};

using MidiClipboard = RangeClipboard<MidiNote>;
using ClipClipboard = RangeClipboard<Clip>;

// Copies every item that at least partially overlaps [rangeStart, rangeEnd)
// out of `source`, deep-copying each so later edits to the source don't
// alter the clipboard.
template <typename Item>
RangeClipboard<Item> extractRange(const std::vector<std::shared_ptr<Item>>& source, int64_t rangeStart,
                                   int64_t rangeEnd) {
    RangeClipboard<Item> clip;
    clip.baseSample = rangeStart;
    clip.spanSamples = rangeEnd - rangeStart;
    for (auto& item : source) {
        if (!item) continue;
        if (itemEnd(*item) > rangeStart && itemStart(*item) < rangeEnd) {
            clip.items.push_back(std::make_shared<Item>(*item));
        }
    }
    return clip;
}

// Pastes `clip` into `destination` at `targetSample`, offsetting every
// pasted item by (targetSample - clip.baseSample) and removing any existing
// item in `destination` that overlaps the pasted region
// [targetSample, targetSample + clip.spanSamples). Returns the new list;
// does not mutate `destination`. Pasted items get fresh ids so multiple
// pastes of the same clipboard never collide.
template <typename Item>
std::vector<std::shared_ptr<Item>> pasteRange(const std::vector<std::shared_ptr<Item>>& destination,
                                               const RangeClipboard<Item>& clip, int64_t targetSample) {
    if (clip.empty()) return destination;
    int64_t offset = targetSample - clip.baseSample;
    int64_t pasteEnd = targetSample + clip.spanSamples;

    std::vector<std::shared_ptr<Item>> result;
    result.reserve(destination.size() + clip.items.size());
    for (auto& item : destination) {
        if (item && itemEnd(*item) > targetSample && itemStart(*item) < pasteEnd) continue; // overwritten
        result.push_back(item);
    }
    for (auto& item : clip.items) {
        auto pasted = std::make_shared<Item>(*item);
        pasted->id = QUuid::createUuid();
        setItemStart(*pasted, itemStart(*item) + offset);
        result.push_back(pasted);
    }
    return result;
}

// Shifts every item in `source` starting at or after `insertAt` later by
// `itemToCopy`'s length (making room), then inserts a fresh-id copy of
// `itemToCopy` at `insertAt`. Used for single-item paste: unlike
// pasteRange() this never overwrites anything, it only pushes.
template <typename Item>
std::vector<std::shared_ptr<Item>> insertItemAfter(const std::vector<std::shared_ptr<Item>>& source,
                                                    const Item& itemToCopy, int64_t insertAt) {
    int64_t span = itemToCopy.lengthSamples;

    std::vector<std::shared_ptr<Item>> result;
    result.reserve(source.size() + 1);
    for (auto& existing : source) {
        if (existing && itemStart(*existing) >= insertAt) {
            auto shifted = std::make_shared<Item>(*existing);
            setItemStart(*shifted, itemStart(*shifted) + span);
            result.push_back(shifted);
        } else {
            result.push_back(existing);
        }
    }

    auto pasted = std::make_shared<Item>(itemToCopy);
    pasted->id = QUuid::createUuid();
    setItemStart(*pasted, insertAt);
    result.push_back(pasted);
    return result;
}

// Applies insertItemAfter() `count` times in a row, each new copy placed
// right after the previous one (first copy at `firstInsertAt`, matching
// insertItemAfter's push semantics so later items keep getting shoved along
// as each repeat lands). Used for the clip/note context menu's "Repeat...".
template <typename Item>
std::vector<std::shared_ptr<Item>> repeatItemAfter(const std::vector<std::shared_ptr<Item>>& source,
                                                    const Item& itemToCopy, int64_t firstInsertAt, int count) {
    std::vector<std::shared_ptr<Item>> result = source;
    int64_t insertAt = firstInsertAt;
    for (int i = 0; i < count; ++i) {
        result = insertItemAfter<Item>(result, itemToCopy, insertAt);
        insertAt += itemToCopy.lengthSamples;
    }
    return result;
}

// A single copied item (as opposed to a time-range clipboard), plus the
// chain state needed for "each paste lands after the last one": the sample
// the next paste will land at, and the track it was copied from — a
// single-item paste only makes sense chained onto its own source track.
template <typename Item>
struct SingleItemClipboard {
    std::shared_ptr<Item> item;
    int64_t nextPasteSample = 0;
    QUuid sourceTrackId;

    bool empty() const { return item == nullptr; }
};

using SingleMidiClipboard = SingleItemClipboard<MidiNote>;
using SingleClipClipboard = SingleItemClipboard<Clip>;

// Holds at most one clipboard (a time range of Clips/MidiNotes, or a single
// copied Clip/MidiNote) at a time — setting one discards all the others,
// matching a single system-clipboard mental model.
class EditClipboardStore {
public:
    static EditClipboardStore& instance() {
        static EditClipboardStore inst;
        return inst;
    }

    EditClipboardStore() = default;

    void setMidi(MidiClipboard clip) {
        clearAll();
        m_midi = std::move(clip);
        m_hasMidi = true;
    }
    void setClips(ClipClipboard clip) {
        clearAll();
        m_clips = std::move(clip);
        m_hasClips = true;
    }
    void setSingleMidi(SingleMidiClipboard clip) {
        clearAll();
        m_singleMidi = std::move(clip);
        m_hasSingleMidi = true;
    }
    void setSingleClip(SingleClipClipboard clip) {
        clearAll();
        m_singleClip = std::move(clip);
        m_hasSingleClip = true;
    }

    void advanceSingleMidi(int64_t newNextPasteSample) { m_singleMidi.nextPasteSample = newNextPasteSample; }
    void advanceSingleClip(int64_t newNextPasteSample) { m_singleClip.nextPasteSample = newNextPasteSample; }

    bool hasMidi() const { return m_hasMidi; }
    bool hasClips() const { return m_hasClips; }
    bool hasSingleMidi() const { return m_hasSingleMidi; }
    bool hasSingleClip() const { return m_hasSingleClip; }
    const MidiClipboard& midi() const { return m_midi; }
    const ClipClipboard& clips() const { return m_clips; }
    const SingleMidiClipboard& singleMidi() const { return m_singleMidi; }
    const SingleClipClipboard& singleClip() const { return m_singleClip; }

private:
    void clearAll() {
        m_hasMidi = false;
        m_hasClips = false;
        m_hasSingleMidi = false;
        m_hasSingleClip = false;
    }

    MidiClipboard m_midi;
    ClipClipboard m_clips;
    SingleMidiClipboard m_singleMidi;
    SingleClipClipboard m_singleClip;
    bool m_hasMidi = false;
    bool m_hasClips = false;
    bool m_hasSingleMidi = false;
    bool m_hasSingleClip = false;
};

} // namespace rsd
