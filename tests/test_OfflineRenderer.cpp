#include <QtTest>

#include "audio/OfflineRenderer.h"
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

class TestOfflineRenderer : public QObject {
    Q_OBJECT

private slots:
    void contentLengthIsLatestClipEnd() {
        Session session;
        session.tracks.push_back(makeTrackWithClip(0.1f, 500));

        auto track2 = std::make_shared<Track>();
        auto clip2 = std::make_shared<Clip>();
        clip2->sessionStartSample = 300;
        clip2->lengthSamples = 200; // ends at 500, same as track 1 — add a longer one too
        track2->addClip(clip2);
        auto clip3 = std::make_shared<Clip>();
        clip3->sessionStartSample = 600;
        clip3->lengthSamples = 200; // ends at 800
        track2->addClip(clip3);
        session.tracks.push_back(track2);

        QCOMPARE(sessionContentLengthSamples(session), int64_t(800));
    }

    void contentLengthIsZeroForEmptySession() {
        Session session;
        QCOMPARE(sessionContentLengthSamples(session), int64_t(0));
    }

    void mixdownSpansMultipleInternalBlocksCorrectly() {
        // 2000 samples, well past the renderer's internal 1024-frame block
        // size, so this exercises the boundary between two blocks.
        Session session;
        session.tracks.push_back(makeTrackWithClip(0.5f, 2000));

        auto buffer = renderSessionMixdown(session, 48000, 2, 2000);
        QCOMPARE(buffer->frameCount(), int64_t(2000));
        QCOMPARE(buffer->channels, 2);

        for (int64_t frame = 0; frame < 2000; ++frame) {
            QCOMPARE(buffer->samples[frame * 2], 0.5f);
            QCOMPARE(buffer->samples[frame * 2 + 1], 0.5f);
        }
    }

    void stemSpansMultipleInternalBlocksAndSkipsBusRouting() {
        Session session;
        auto bus = std::make_shared<Track>();
        bus->kind = TrackKind::Bus;
        session.tracks.push_back(bus);

        auto track = makeTrackWithClip(0.5f, 2000);
        track->setSendBusId(bus->id);
        track->sendLevel.store(1.0f);
        session.tracks.push_back(track);

        auto buffer = renderTrackStem(*track, 48000, 2, 2000);
        QCOMPARE(buffer->frameCount(), int64_t(2000));
        for (int64_t frame = 0; frame < 2000; ++frame) {
            QCOMPARE(buffer->samples[frame * 2], 0.5f); // not doubled by the send
        }
    }
};

QTEST_APPLESS_MAIN(TestOfflineRenderer)
#include "test_OfflineRenderer.moc"
