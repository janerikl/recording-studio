#include <QtTest>

#include <cmath>

#include "audio/SessionMixer.h"
#include "model/Session.h"

using namespace rsd;

namespace {

std::shared_ptr<Track> makeTrackWithClip(float sampleValue, int64_t lengthFrames) {
    auto track = std::make_shared<Track>();
    auto buffer = std::make_shared<AudioBuffer>();
    buffer->channels = 1;
    buffer->sampleRate = 48000;
    buffer->samples.assign(static_cast<size_t>(lengthFrames), sampleValue);

    auto clip = std::make_shared<Clip>();
    clip->buffer = buffer;
    clip->sessionStartSample = 0;
    clip->sourceOffsetSamples = 0;
    clip->lengthSamples = lengthFrames;
    track->addClip(clip);
    return track;
}

// Instrument-track helpers for the scheduling/CC tests below. These drive
// the real FluidSynth engine (FluidR3_GM when installed, else TimGM6mb).
std::shared_ptr<Track> makeInstrumentTrack(int program) {
    auto track = std::make_shared<Track>();
    track->kind = TrackKind::Instrument;
    track->synthParams.instrumentProgram.store(program);
    return track;
}

void addNote(Track& track, int pitch, int64_t start, int64_t length, float velocity = 0.8f) {
    auto note = std::make_shared<MidiNote>();
    note->pitch = pitch;
    note->velocity = velocity;
    note->startSample = start;
    note->lengthSamples = length;
    track.addMidiNote(note);
}

// Renders `totalFrames` of the track in fixed-size blocks, the same way
// OfflineRenderer does, returning interleaved stereo.
std::vector<float> renderBlocks(Track& track, int64_t totalFrames, unsigned int blockFrames = 1024) {
    std::vector<float> all(static_cast<size_t>(totalFrames) * 2, 0.0f);
    std::vector<float> block(static_cast<size_t>(blockFrames) * 2, 0.0f);
    for (int64_t pos = 0; pos < totalFrames; pos += blockFrames) {
        auto n = static_cast<unsigned int>(std::min<int64_t>(blockFrames, totalFrames - pos));
        renderTrackBlock(track, 48000, 2, pos, n, true, block.data());
        std::copy(block.begin(), block.begin() + n * 2, all.begin() + pos * 2);
    }
    return all;
}

// RMS of the left channel over frames [from, to).
double rmsLeft(const std::vector<float>& buf, int64_t from, int64_t to) {
    double sum = 0.0;
    for (int64_t i = from; i < to; ++i) sum += double(buf[i * 2]) * buf[i * 2];
    return std::sqrt(sum / double(to - from));
}

double maxAbsLeft(const std::vector<float>& buf, int64_t from, int64_t to) {
    double m = 0.0;
    for (int64_t i = from; i < to; ++i) m = std::max(m, double(std::fabs(buf[i * 2])));
    return m;
}

// Fundamental estimate (Hz) of the left channel in [from, from+len) by
// normalized autocorrelation over lags for roughly 300-700 Hz, refined with
// parabolic interpolation. Good enough to see +-50 cent vibrato on A4.
double estimatePitchHz(const std::vector<float>& buf, int64_t from, int64_t len) {
    auto corr = [&](int lag) {
        double num = 0.0, e0 = 0.0, e1 = 0.0;
        for (int64_t i = from; i < from + len; ++i) {
            double a = buf[i * 2], b = buf[(i + lag) * 2];
            num += a * b; e0 += a * a; e1 += b * b;
        }
        return num / std::sqrt(e0 * e1 + 1e-30);
    };
    int bestLag = 70;
    double best = -2.0;
    for (int lag = 70; lag <= 160; ++lag) {
        double c = corr(lag);
        if (c > best) { best = c; bestLag = lag; }
    }
    double c0 = corr(bestLag - 1), c1 = best, c2 = corr(bestLag + 1);
    double denom = c0 - 2.0 * c1 + c2;
    double offset = std::fabs(denom) > 1e-12 ? 0.5 * (c0 - c2) / denom : 0.0;
    return 48000.0 / (bestLag + offset);
}

// Peak-to-peak pitch swing in cents across consecutive windows of a
// held note.
double pitchSwingCents(const std::vector<float>& buf, int64_t from, int64_t to) {
    double lo = 1e9, hi = 0.0;
    for (int64_t w = from; w + 1200 + 200 < to; w += 480) {
        double hz = estimatePitchHz(buf, w, 1200);
        lo = std::min(lo, hz);
        hi = std::max(hi, hz);
    }
    return 1200.0 * std::log2(hi / lo);
}

} // namespace

class TestSessionMixer : public QObject {
    Q_OBJECT

private slots:
    void dryTrackAtUnityPassesClipThroughUnchanged() {
        Session session;
        session.tracks.push_back(makeTrackWithClip(0.5f, 100));

        SessionMixScratch scratch;
        std::vector<float> out(100 * 2, -1.0f);
        mixSessionBlock(session, 48000, 2, 0, 100, true, out.data(), scratch);

        for (float v : out) QCOMPARE(v, 0.5f);
    }

    void mutedTrackProducesSilence() {
        Session session;
        auto track = makeTrackWithClip(0.5f, 100);
        track->muted.store(true);
        session.tracks.push_back(track);

        SessionMixScratch scratch;
        std::vector<float> out(100 * 2, -1.0f);
        mixSessionBlock(session, 48000, 2, 0, 100, true, out.data(), scratch);

        for (float v : out) QCOMPARE(v, 0.0f);
    }

    void nonArmedTrackSilentWhileRecordingButArmedTrackStillRenders() {
        Session session;
        auto armed = makeTrackWithClip(0.5f, 100);
        armed->recordArmed.store(true);
        auto notArmed = makeTrackWithClip(0.5f, 100);
        session.tracks.push_back(armed);
        session.tracks.push_back(notArmed);

        SessionMixScratch scratch;
        std::vector<float> out(100 * 2, -1.0f);
        mixSessionBlock(session, 48000, 2, 0, 100, true, out.data(), scratch, /*isRecording=*/true);

        // Only the armed track's 0.5f contributes; the non-armed track is
        // suppressed, so the mix should equal the armed track alone.
        for (float v : out) QCOMPARE(v, 0.5f);
    }

    void nonArmedTrackStillPlaysWhenNotRecording() {
        Session session;
        auto notArmed = makeTrackWithClip(0.5f, 100);
        session.tracks.push_back(notArmed);

        SessionMixScratch scratch;
        std::vector<float> out(100 * 2, -1.0f);
        mixSessionBlock(session, 48000, 2, 0, 100, true, out.data(), scratch, /*isRecording=*/false);

        for (float v : out) QCOMPARE(v, 0.5f);
    }

    void auxSendAddsScaledCopyOnTopOfDirectContribution() {
        Session session;
        auto bus = std::make_shared<Track>();
        bus->kind = TrackKind::Bus;
        session.tracks.push_back(bus);

        auto track = makeTrackWithClip(0.5f, 100);
        track->setSendBusId(bus->id);
        track->sendLevel.store(0.5f);
        session.tracks.push_back(track);

        SessionMixScratch scratch;
        std::vector<float> out(100 * 2, -1.0f);
        mixSessionBlock(session, 48000, 2, 0, 100, true, out.data(), scratch);

        // Direct contribution (0.5) + bus contribution (0.5 * 0.5 send) = 0.75.
        for (float v : out) QCOMPARE(v, 0.75f);
    }

    void masterVolumeScalesFinalOutput() {
        Session session;
        session.tracks.push_back(makeTrackWithClip(0.5f, 100));
        session.masterBus.volume.store(0.5f);

        SessionMixScratch scratch;
        std::vector<float> out(100 * 2, -1.0f);
        mixSessionBlock(session, 48000, 2, 0, 100, true, out.data(), scratch);

        for (float v : out) QCOMPARE(v, 0.25f);
    }

    void mixSessionBlockUpdatesPerTrackPostFaderPeaks() {
        Session session;
        auto track = makeTrackWithClip(0.5f, 100);
        track->pan.store(0.0f);
        session.tracks.push_back(track);

        SessionMixScratch scratch;
        std::vector<float> out(100 * 2, 0.0f);
        mixSessionBlock(session, 48000, 2, 0, 100, true, out.data(), scratch);

        // Unity volume, center pan: post-fader peak matches the dry sample.
        QCOMPARE(track->postFaderPeakL.load(), 0.5f);
        QCOMPARE(track->postFaderPeakR.load(), 0.5f);
    }

    void mixSessionBlockScalesPerTrackPeakByVolume() {
        Session session;
        auto track = makeTrackWithClip(0.5f, 100);
        track->volume.store(0.5f);
        session.tracks.push_back(track);

        SessionMixScratch scratch;
        std::vector<float> out(100 * 2, 0.0f);
        mixSessionBlock(session, 48000, 2, 0, 100, true, out.data(), scratch);

        QCOMPARE(track->postFaderPeakL.load(), 0.25f);
        QCOMPARE(track->postFaderPeakR.load(), 0.25f);
    }

    void mixSessionBlockZeroesPeakForMutedTrack() {
        Session session;
        auto track = makeTrackWithClip(0.5f, 100);
        track->muted.store(true);
        session.tracks.push_back(track);

        SessionMixScratch scratch;
        std::vector<float> out(100 * 2, 0.0f);
        mixSessionBlock(session, 48000, 2, 0, 100, true, out.data(), scratch);

        QCOMPARE(track->postFaderPeakL.load(), 0.0f);
        QCOMPARE(track->postFaderPeakR.load(), 0.0f);
    }

    void renderTrackBlockExcludesSendAndMasterProcessing() {
        Session session;
        auto bus = std::make_shared<Track>();
        bus->kind = TrackKind::Bus;
        session.tracks.push_back(bus);

        auto track = makeTrackWithClip(0.5f, 100);
        track->setSendBusId(bus->id);
        track->sendLevel.store(0.9f);
        session.tracks.push_back(track);
        session.masterBus.volume.store(0.1f); // would tank the output if this leaked in

        std::vector<float> out(100 * 2, -1.0f);
        renderTrackBlock(*track, 48000, 2, 0, 100, true, out.data());

        // Just the track's own dry signal at unity volume/pan, unaffected by
        // its own send or the (irrelevant here) master volume.
        for (float v : out) QCOMPARE(v, 0.5f);
    }

    void noteStartingMidBlockIsSilentBeforeItsStartSample() {
        // Start at frame 700 of the first 1024-frame block: the old
        // block-granular scheduler would have fired it at frame 0.
        auto track = makeInstrumentTrack(0); // piano: fast attack
        addNote(*track, 60, 700, 24000);
        auto out = renderBlocks(*track, 4096);

        QVERIFY2(maxAbsLeft(out, 0, 700) < 1e-5, "audio before the note's start sample");
        // FluidSynth processes events on its internal 64-frame grid, plus a
        // short attack: must be sounding well within ~3ms of the start.
        QVERIFY(maxAbsLeft(out, 700, 700 + 160) > 1e-3);
    }

    void noteStartingMidSecondBlockIsSilentBeforeItsStartSample() {
        auto track = makeInstrumentTrack(0);
        addNote(*track, 64, 1024 + 900, 24000);
        auto out = renderBlocks(*track, 4096);

        QVERIFY(maxAbsLeft(out, 0, 1024 + 900) < 1e-5);
        QVERIFY(maxAbsLeft(out, 1024 + 900, 1024 + 900 + 160) > 1e-3);
    }

    void noteEndingMidBlockReleasesAtItsEndSample() {
        // Violin sustains while held and dies away quickly after release.
        auto held = makeInstrumentTrack(40);
        addNote(*held, 69, 0, 48000);
        auto released = makeInstrumentTrack(40);
        addNote(*released, 69, 0, 24000 + 300); // ends 300 frames into a block
        auto a = renderBlocks(*held, 48000);
        auto b = renderBlocks(*released, 48000);

        // Identical until the release point (same fresh synth state).
        for (int64_t i = 0; i < 24000 + 300; ++i) QCOMPARE(b[i * 2], a[i * 2]);
        // Released version diverges shortly after its end sample.
        double diff = 0.0;
        for (int64_t i = 24300; i < 24300 + 2048; ++i) diff = std::max(diff, double(std::fabs(a[i * 2] - b[i * 2])));
        QVERIFY(diff > 1e-4);
    }

    void sameNoteOverlapDoesNotCutTheLaterNote() {
        // A: 60 over [0, 12000); B: 60 over [6000, 72000). A's end must not
        // release B, which is still held.
        auto overlapped = makeInstrumentTrack(40);
        addNote(*overlapped, 69, 0, 12000);
        addNote(*overlapped, 69, 6000, 66000);
        auto out = renderBlocks(*overlapped, 72000);

        auto reference = makeInstrumentTrack(40);
        addNote(*reference, 69, 6000, 66000);
        auto ref = renderBlocks(*reference, 72000);

        double rmsOverlapped = rmsLeft(out, 48000, 72000);
        double rmsRef = rmsLeft(ref, 48000, 72000);
        QVERIFY(rmsRef > 1e-3);
        QVERIFY2(rmsOverlapped > 0.5 * rmsRef, "later same-pitch note was cut by the earlier note's end");
    }

    void expressionLaneLowersHeldNoteLevel() {
        auto full = makeInstrumentTrack(40);
        addNote(*full, 69, 0, 48000);
        full->replaceAutomationLane(
            std::make_shared<AutomationLane>(AutomationLane{AutomationTarget::Expression, {{0, 1.0f}}}));

        auto soft = makeInstrumentTrack(40);
        addNote(*soft, 69, 0, 48000);
        soft->replaceAutomationLane(
            std::make_shared<AutomationLane>(AutomationLane{AutomationTarget::Expression, {{0, 0.3f}}}));

        double rmsFull = rmsLeft(renderBlocks(*full, 48000), 12000, 48000);
        double rmsSoft = rmsLeft(renderBlocks(*soft, 48000), 12000, 48000);
        qInfo("expression RMS: 1.0 -> %g, 0.3 -> %g", rmsFull, rmsSoft);
        QVERIFY(rmsFull > 1e-3);
        QVERIFY2(rmsSoft < 0.5 * rmsFull,
                 qPrintable(QString("full %1 soft %2").arg(rmsFull).arg(rmsSoft)));
    }

    void expressionLaneChangesMidNote() {
        // A ramp from 1.0 down to 0.1 across a held note: the end is much
        // quieter than the start, without retriggering the note.
        auto track = makeInstrumentTrack(40);
        addNote(*track, 69, 0, 96000);
        track->replaceAutomationLane(std::make_shared<AutomationLane>(
            AutomationLane{AutomationTarget::Expression, {{24000, 1.0f}, {72000, 0.1f}}}));
        auto out = renderBlocks(*track, 96000);

        // Relative to the same note without automation (the sample has its
        // own natural decay).
        auto flat = makeInstrumentTrack(40);
        addNote(*flat, 69, 0, 96000);
        auto ref = renderBlocks(*flat, 96000);

        double rampRatio = rmsLeft(out, 84000, 96000) / rmsLeft(out, 12000, 24000);
        double flatRatio = rmsLeft(ref, 84000, 96000) / rmsLeft(ref, 12000, 24000);
        QVERIFY2(rampRatio < 0.5 * flatRatio,
                 qPrintable(QString("ramp %1 flat %2").arg(rampRatio).arg(flatRatio)));
    }

    void vibratoLaneModulatesPitchOfHeldNote() {
        auto still = makeInstrumentTrack(40);
        addNote(*still, 69, 0, 96000);
        still->replaceAutomationLane(
            std::make_shared<AutomationLane>(AutomationLane{AutomationTarget::Vibrato, {{0, 0.0f}}}));
        auto stillTwin = makeInstrumentTrack(40);
        addNote(*stillTwin, 69, 0, 96000);
        stillTwin->replaceAutomationLane(
            std::make_shared<AutomationLane>(AutomationLane{AutomationTarget::Vibrato, {{0, 0.0f}}}));
        auto wobbly = makeInstrumentTrack(40);
        addNote(*wobbly, 69, 0, 96000);
        wobbly->replaceAutomationLane(
            std::make_shared<AutomationLane>(AutomationLane{AutomationTarget::Vibrato, {{0, 1.0f}}}));

        auto a = renderBlocks(*still, 96000);
        auto aTwin = renderBlocks(*stillTwin, 96000);
        auto b = renderBlocks(*wobbly, 96000);

        // Rendering is deterministic, so any difference comes from CC1.
        double controlDiff = 0.0, energy = 0.0, diff = 0.0;
        for (int64_t i = 24000; i < 96000; ++i) {
            controlDiff += std::pow(a[i * 2] - aTwin[i * 2], 2.0);
            diff += std::pow(a[i * 2] - b[i * 2], 2.0);
            energy += std::pow(a[i * 2], 2.0);
        }
        QVERIFY(controlDiff < 1e-9 * energy + 1e-12);
        QVERIFY2(diff > 0.05 * energy, qPrintable(QString("relative diff %1").arg(diff / energy)));

        // And it's actually pitch modulation: the swing clearly exceeds the
        // un-vibrato'd note's own.
        double swingStill = pitchSwingCents(a, 24000, 96000);
        double swingWobbly = pitchSwingCents(b, 24000, 96000);
        qInfo("vibrato: relative diff %g, pitch swing %g cents (CC1=0) vs %g cents (CC1=127)",
              diff / energy, swingStill, swingWobbly);
        QVERIFY2(swingWobbly > swingStill + 30.0,
                 qPrintable(QString("still %1c wobbly %2c").arg(swingStill).arg(swingWobbly)));
    }
};

QTEST_APPLESS_MAIN(TestSessionMixer)
#include "test_SessionMixer.moc"
