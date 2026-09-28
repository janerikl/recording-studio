#pragma once

#include <QUuid>
#include <QString>
#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>

#include "AutomationLane.h"
#include "Clip.h"
#include "MidiNote.h"
#include "audio/Effects.h"
#include "audio/NoteEventQueue.h"
#include "audio/Synth.h"

namespace rsd {

// Which physical input a track records from when armed. SystemAudio is fed
// by AudioEngine's second capture stream (a PulseAudio ".monitor" loopback
// device), entirely separate from the Mic stream, so a Mic-armed track and a
// SystemAudio-armed track can record concurrently onto separate clips.
enum class AudioSource { Mic, SystemAudio };

// Audio tracks hold recorded/imported Clips; Instrument tracks hold
// MidiNotes played through a built-in synth instead. Bus tracks hold
// neither — they're fed only by other tracks' sends (see sendBusId/
// sendLevel below) and exist purely for their volume/pan/effects chain
// (e.g. a shared reverb bus). Everything else about a Track (mute/solo/
// arm/gain/pan/effects) applies the same way to all three, though Bus
// tracks are never armed/recorded to.
enum class TrackKind { Audio, Instrument, Bus };

// Clip list is stored behind an atomic shared_ptr to a const vector so the
// realtime audio callback can snapshot-read it without locking, while the
// GUI thread performs edits via copy-on-write + atomic swap.
class Track {
public:
    using ClipList = std::vector<std::shared_ptr<Clip>>;
    using MidiNoteList = std::vector<std::shared_ptr<MidiNote>>;
    using AutomationLaneList = std::vector<std::shared_ptr<AutomationLane>>;

    QUuid id = QUuid::createUuid();
    QString name;
    TrackKind kind = TrackKind::Audio;
    std::atomic<bool> muted{false};
    std::atomic<bool> soloed{false};
    std::atomic<bool> recordArmed{false};
    std::atomic<AudioSource> inputSource{AudioSource::Mic};
    // Volume/pan are the source of truth for the mixer; gainL/gainR are
    // derived from them each block via PanLawMath::panToGains (by
    // AudioEngine, using an automation curve's current value when one
    // exists for that target, falling back to these static atomics).
    std::atomic<float> volume{1.0f};
    std::atomic<float> pan{0.0f};

    // Aux send: post-fader tap of this track's output into a Bus track's
    // aux buffer, in addition to (not instead of) this track's normal
    // contribution to the master mix. At most one send per track for v1.
    // A Bus track's own send is meaningless (buses don't send to other
    // buses, avoiding any cycle) and is left untouched/ignored by
    // AudioEngine. Stored as atomic<shared_ptr<const QUuid>> rather than
    // atomic<QUuid> directly: QUuid is 16 bytes, which isn't lock-free on
    // this platform's libstdc++/libatomic setup, whereas the
    // atomic-shared_ptr pattern is already used lock-free elsewhere in
    // this class (m_clips etc.) — nullptr means "no send".
    std::atomic<float> sendLevel{0.0f};

    // Post-fader (post volume/pan) peak level for this track's most recently
    // mixed block, written by mixSessionBlock and polled by the UI's meter
    // timer (see MainWindow::updateMeters). Not decayed/smoothed here —
    // that's the meter widget's job, same as the existing global meters.
    std::atomic<float> postFaderPeakL{0.0f};
    std::atomic<float> postFaderPeakR{0.0f};

    QUuid sendBusId() const {
        auto id = m_sendBusId.load();
        return id ? *id : QUuid();
    }
    void setSendBusId(const QUuid& id) {
        m_sendBusId.store(id.isNull() ? nullptr : std::make_shared<const QUuid>(id));
    }

    // Instrument-track only, but harmless to carry on every track. Params
    // are live-tweaked atomics (see Effects.h's Effect for the same
    // pattern); the queue/engine are RT-thread-owned, same trust model as
    // AudioEngine's PunchRecorder — the GUI thread only ever pushes to the
    // queue, never touches the engine directly.
    SynthParams synthParams;
    NoteEventQueue liveNoteEvents;
    SynthEngine synthEngine;

    Track() {
        m_clips.store(std::make_shared<const ClipList>());
        m_effects.store(std::make_shared<const EffectChain>());
        m_takeLanes.store(std::make_shared<const ClipList>());
        m_midiNotes.store(std::make_shared<const MidiNoteList>());
        m_automationLanes.store(std::make_shared<const AutomationLaneList>());
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

    // Alternate takes for comping: the passes captured by the most recent
    // punch/loop recording, separate from the active comp (`clips` above).
    // Same copy-on-write/atomic-swap pattern; a new punch/loop recording
    // replaces the whole set (v1 scope: one take group per track at a time).
    std::shared_ptr<const ClipList> takesSnapshot() const { return m_takeLanes.load(); }
    void restoreTakes(std::shared_ptr<const ClipList> snapshot) { m_takeLanes.store(std::move(snapshot)); }
    void setTakes(ClipList takes) {
        m_takeLanes.store(std::make_shared<const ClipList>(std::move(takes)));
    }

    // Recorded MIDI notes for an Instrument track: same copy-on-write/
    // atomic-swap pattern as `clips`.
    std::shared_ptr<const MidiNoteList> midiClipsSnapshot() const { return m_midiNotes.load(); }
    void restoreMidiClips(std::shared_ptr<const MidiNoteList> snapshot) {
        m_midiNotes.store(std::move(snapshot));
    }
    void setMidiClips(MidiNoteList notes) {
        m_midiNotes.store(std::make_shared<const MidiNoteList>(std::move(notes)));
    }

    // Same atomic-swap pattern as replaceClip/removeClip/addClip, for the
    // piano-roll editor's live-drag feedback (mutate on every mouse move,
    // command pushed with before/after snapshots only on release).
    void replaceMidiNote(const QUuid& noteId, std::shared_ptr<MidiNote> newNote) {
        auto current = m_midiNotes.load();
        auto updated = std::make_shared<MidiNoteList>(*current);
        for (auto& n : *updated) {
            if (n->id == noteId) {
                n = std::move(newNote);
                break;
            }
        }
        m_midiNotes.store(std::const_pointer_cast<const MidiNoteList>(updated));
    }

    void addMidiNote(std::shared_ptr<MidiNote> note) {
        auto current = m_midiNotes.load();
        auto updated = std::make_shared<MidiNoteList>(*current);
        updated->push_back(std::move(note));
        m_midiNotes.store(std::const_pointer_cast<const MidiNoteList>(updated));
    }

    void removeMidiNote(const QUuid& noteId) {
        auto current = m_midiNotes.load();
        auto updated = std::make_shared<MidiNoteList>(*current);
        updated->erase(std::remove_if(updated->begin(), updated->end(),
                                       [&](const auto& n) { return n->id == noteId; }),
                        updated->end());
        m_midiNotes.store(std::const_pointer_cast<const MidiNoteList>(updated));
    }

    // Automation curves (Volume/Pan, plus Expression/Vibrato CCs for
    // Instrument tracks): same copy-on-write/atomic-swap pattern as
    // `clips`/`midiClips`. At most one lane per AutomationTarget;
    // AudioEngine reads this snapshot each block and, for a target with no
    // lane (or an empty one), falls back to the static volume/pan atomics
    // (or the MIDI CC defaults).
    std::shared_ptr<const AutomationLaneList> automationLanesSnapshot() const {
        return m_automationLanes.load();
    }
    void restoreAutomationLanes(std::shared_ptr<const AutomationLaneList> snapshot) {
        m_automationLanes.store(std::move(snapshot));
    }

    // Replaces the lane for `newLane->target` if one exists, else appends
    // it. Used both for live-drag feedback (mutate on every mouse move) and
    // for adding a lane's very first point.
    void replaceAutomationLane(std::shared_ptr<AutomationLane> newLane) {
        auto current = m_automationLanes.load();
        auto updated = std::make_shared<AutomationLaneList>(*current);
        bool found = false;
        for (auto& lane : *updated) {
            if (lane->target == newLane->target) {
                lane = newLane;
                found = true;
                break;
            }
        }
        if (!found) updated->push_back(std::move(newLane));
        m_automationLanes.store(std::const_pointer_cast<const AutomationLaneList>(updated));
    }

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
    std::atomic<std::shared_ptr<const ClipList>> m_takeLanes;
    std::atomic<std::shared_ptr<const MidiNoteList>> m_midiNotes;
    std::atomic<std::shared_ptr<const AutomationLaneList>> m_automationLanes;
    std::atomic<std::shared_ptr<const QUuid>> m_sendBusId{nullptr};
};

} // namespace rsd
