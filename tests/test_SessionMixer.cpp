#include <QtTest>

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
};

QTEST_APPLESS_MAIN(TestSessionMixer)
#include "test_SessionMixer.moc"
