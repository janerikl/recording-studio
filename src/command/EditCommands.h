#pragma once

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "Command.h"
#include "model/Session.h"
#include "model/Track.h"

namespace rsd {

// Groups several commands (e.g. a cross-track clip move, which touches both
// the source and destination track's clip lists) into one undo step.
class CompositeCommand : public Command {
public:
    explicit CompositeCommand(std::vector<std::unique_ptr<Command>> commands, QString text = "Edit")
        : m_commands(std::move(commands)), m_text(std::move(text)) {}

    void redo() override {
        for (auto& c : m_commands) c->redo();
    }
    void undo() override {
        for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it) (*it)->undo();
    }
    QString text() const override { return m_text; }

private:
    std::vector<std::unique_ptr<Command>> m_commands;
    QString m_text;
};

// Covers add/remove/move/trim/split-clip edits on a single track: the whole
// clip list is atomically-swappable already (Track::restoreClips), so undo is
// "restore the snapshot from before the edit" rather than inverting each
// mutation's arithmetic. Callers capture `before` via track->clipsSnapshot()
// prior to mutating and `after` once the mutation (already performed via the
// normal Track methods) has settled.
class TrackClipsCommand : public Command {
public:
    TrackClipsCommand(std::shared_ptr<Track> track, std::shared_ptr<const Track::ClipList> before,
                       std::shared_ptr<const Track::ClipList> after, QString text = "Edit Clip")
        : m_track(std::move(track)), m_before(std::move(before)), m_after(std::move(after)),
          m_text(std::move(text)) {}

    void redo() override { m_track->restoreClips(m_after); }
    void undo() override { m_track->restoreClips(m_before); }
    QString text() const override { return m_text; }

private:
    std::shared_ptr<Track> m_track;
    std::shared_ptr<const Track::ClipList> m_before;
    std::shared_ptr<const Track::ClipList> m_after;
    QString m_text;
};

// Scalar mixer/transport state for one track (gain, mute, solo, arm). Fader
// drags and pan-dial moves should capture `before` on press and push one of
// these on release, not on every intermediate value.
struct TrackState {
    float gainL = 1.0f;
    float gainR = 1.0f;
    bool muted = false;
    bool soloed = false;
    bool recordArmed = false;

    static TrackState capture(const Track& t) {
        return {t.gainL.load(), t.gainR.load(), t.muted.load(), t.soloed.load(), t.recordArmed.load()};
    }
};

class TrackStateCommand : public Command {
public:
    TrackStateCommand(std::shared_ptr<Track> track, TrackState before, TrackState after,
                       QString text = "Change Track")
        : m_track(std::move(track)), m_before(before), m_after(after), m_text(std::move(text)) {}

    void redo() override { apply(m_after); }
    void undo() override { apply(m_before); }
    QString text() const override { return m_text; }

private:
    void apply(const TrackState& s) {
        m_track->gainL.store(s.gainL);
        m_track->gainR.store(s.gainR);
        m_track->muted.store(s.muted);
        m_track->soloed.store(s.soloed);
        m_track->recordArmed.store(s.recordArmed);
    }

    std::shared_ptr<Track> m_track;
    TrackState m_before;
    TrackState m_after;
    QString m_text;
};

class SetClipGainCommand : public Command {
public:
    SetClipGainCommand(std::shared_ptr<Track> track, QUuid clipId, float before, float after)
        : m_track(std::move(track)), m_clipId(clipId), m_before(before), m_after(after) {}

    void redo() override { apply(m_after); }
    void undo() override { apply(m_before); }
    QString text() const override { return "Set Clip Gain"; }

private:
    void apply(float gain) {
        for (auto& c : *m_track->clipsSnapshot()) {
            if (c->id == m_clipId) {
                auto edited = std::make_shared<Clip>(*c);
                edited->gain = gain;
                m_track->replaceClip(m_clipId, edited);
                return;
            }
        }
    }

    std::shared_ptr<Track> m_track;
    QUuid m_clipId;
    float m_before;
    float m_after;
};

// Fade lengths/curves for one clip, bundled since a fade edit UI (e.g.
// dragging both fade handles at once) typically changes both together.
struct ClipFadeState {
    int64_t fadeInSamples = 0;
    int64_t fadeOutSamples = 0;
    FadeCurve fadeInCurve = FadeCurve::Linear;
    FadeCurve fadeOutCurve = FadeCurve::Linear;
};

class SetClipFadeCommand : public Command {
public:
    SetClipFadeCommand(std::shared_ptr<Track> track, QUuid clipId, ClipFadeState before,
                        ClipFadeState after)
        : m_track(std::move(track)), m_clipId(clipId), m_before(before), m_after(after) {}

    void redo() override { apply(m_after); }
    void undo() override { apply(m_before); }
    QString text() const override { return "Set Clip Fade"; }

private:
    void apply(const ClipFadeState& s) {
        for (auto& c : *m_track->clipsSnapshot()) {
            if (c->id == m_clipId) {
                auto edited = std::make_shared<Clip>(*c);
                edited->fadeInSamples = s.fadeInSamples;
                edited->fadeOutSamples = s.fadeOutSamples;
                edited->fadeInCurve = s.fadeInCurve;
                edited->fadeOutCurve = s.fadeOutCurve;
                m_track->replaceClip(m_clipId, edited);
                return;
            }
        }
    }

    std::shared_ptr<Track> m_track;
    QUuid m_clipId;
    ClipFadeState m_before;
    ClipFadeState m_after;
};

// Covers add/remove/reorder edits on a track's effect chain: mirrors
// TrackClipsCommand exactly, since the chain is the same kind of
// atomically-swappable snapshot (Track::restoreEffects).
class EffectChainCommand : public Command {
public:
    EffectChainCommand(std::shared_ptr<Track> track, std::shared_ptr<const EffectChain> before,
                        std::shared_ptr<const EffectChain> after, QString text = "Edit Effects")
        : m_track(std::move(track)), m_before(std::move(before)), m_after(std::move(after)),
          m_text(std::move(text)) {}

    void redo() override { m_track->restoreEffects(m_after); }
    void undo() override { m_track->restoreEffects(m_before); }
    QString text() const override { return m_text; }

private:
    std::shared_ptr<Track> m_track;
    std::shared_ptr<const EffectChain> m_before;
    std::shared_ptr<const EffectChain> m_after;
    QString m_text;
};

// A single scalar parameter tweak on an effect (EQ band gain, compressor
// ratio, delay mix, bypass toggle, ...). Unlike clip/effect-chain edits,
// effect parameters are plain atomics on the Effect itself (see Effects.h),
// so undo/redo just re-applies the old/new value via a setter closure
// instead of swapping a whole snapshot.
template <typename T>
class SetEffectParamCommand : public Command {
public:
    SetEffectParamCommand(std::function<void(T)> setter, T before, T after, QString text = "Edit Effect")
        : m_setter(std::move(setter)), m_before(before), m_after(after), m_text(std::move(text)) {}

    void redo() override { m_setter(m_after); }
    void undo() override { m_setter(m_before); }
    QString text() const override { return m_text; }

private:
    std::function<void(T)> m_setter;
    T m_before;
    T m_after;
    QString m_text;
};

class AddTrackCommand : public Command {
public:
    AddTrackCommand(Session* session, std::shared_ptr<Track> track)
        : m_session(session), m_track(std::move(track)) {}

    void redo() override { m_session->tracks.push_back(m_track); }
    void undo() override { m_session->removeTrack(m_track->id); }
    QString text() const override { return "Add Track"; }

private:
    Session* m_session;
    std::shared_ptr<Track> m_track;
};

class RemoveTrackCommand : public Command {
public:
    RemoveTrackCommand(Session* session, std::shared_ptr<Track> track, size_t index)
        : m_session(session), m_track(std::move(track)), m_index(index) {}

    void redo() override { m_session->removeTrack(m_track->id); }
    void undo() override { m_session->insertTrack(m_track, m_index); }
    QString text() const override { return "Remove Track"; }

private:
    Session* m_session;
    std::shared_ptr<Track> m_track;
    size_t m_index;
};

} // namespace rsd
