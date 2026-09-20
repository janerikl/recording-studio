#include <QtTest>

#include "audio/PunchRecorder.h"
#include "audio/PunchRegion.h"
#include "audio/TransportClock.h"
#include "command/CommandStack.h"
#include "command/PunchRecordingCommand.h"

using namespace rsd;

class TestPunchRecording : public QObject {
    Q_OBJECT

private slots:
    void punchRegionValidityAndLength() {
        PunchRegion invalid{100, 100};
        QVERIFY(!invalid.isValid());
        QCOMPARE(invalid.lengthSamples(), int64_t(0));

        PunchRegion valid{100, 500};
        QVERIFY(valid.isValid());
        QCOMPARE(valid.lengthSamples(), int64_t(400));
    }

    void transportDoesNotWrapWhenLoopDisabled() {
        TransportClock clock;
        clock.setPunchRegion({100, 200});
        clock.setPunchLoopEnabled(false);
        clock.setPositionSamples(190);

        clock.advance(20); // would cross region end at 200
        QCOMPARE(clock.positionSamples(), int64_t(210));
    }

    void transportWrapsToPreRollBeforeRegionStartWhenLoopEnabled() {
        TransportClock clock;
        clock.setPunchRegion({100, 200});
        clock.setPunchLoopEnabled(true);
        clock.setPreRollSamples(30);
        clock.setPositionSamples(190);

        clock.advance(20); // crosses region end (200) -> wraps
        QCOMPARE(clock.positionSamples(), int64_t(70)); // 100 - 30 pre-roll
    }

    void transportWrapClampsToZeroWhenPreRollExceedsRegionStart() {
        TransportClock clock;
        clock.setPunchRegion({10, 50});
        clock.setPunchLoopEnabled(true);
        clock.setPreRollSamples(1000);
        clock.setPositionSamples(45);

        clock.advance(10);
        QCOMPARE(clock.positionSamples(), int64_t(0));
    }

    void transportIgnoresRegionOutsidePunchLoopMode() {
        TransportClock clock;
        clock.setPunchRegion({0, 0}); // invalid region
        clock.setPunchLoopEnabled(true);
        clock.setPositionSamples(1000);

        clock.advance(50);
        QCOMPARE(clock.positionSamples(), int64_t(1050));
    }

    void punchRecorderCapturesOnlyWithinRegion() {
        PunchRecorder recorder;
        recorder.prepare(PunchRegion{10, 15}, 1); // 5-frame mono region

        std::vector<float> in(20, 0.0f);
        for (size_t i = 0; i < in.size(); ++i) in[i] = static_cast<float>(i);

        // Callback buffer covers positions [5, 20), region is [10, 15).
        recorder.process(5, in.data(), 20);

        QVERIFY(recorder.hasCaptured());
        const auto& buf = recorder.buffer();
        QCOMPARE(buf.size(), size_t(5));
        // in[5..9] correspond to timeline positions 10..14.
        for (int i = 0; i < 5; ++i) QCOMPARE(buf[i], in[5 + i]);
    }

    void punchRecorderSecondPassReplacesFirst() {
        PunchRecorder recorder;
        recorder.prepare(PunchRegion{0, 4}, 1);

        std::vector<float> passOne = {1.0f, 1.0f, 1.0f, 1.0f};
        recorder.process(0, passOne.data(), 4);
        QCOMPARE(recorder.buffer(), (std::vector<float>{1.0f, 1.0f, 1.0f, 1.0f}));
        QCOMPARE(recorder.passCount(), 0);

        // Loop wraps back to region start for pass two.
        std::vector<float> passTwo = {2.0f, 2.0f, 2.0f, 2.0f};
        recorder.process(0, passTwo.data(), 4);

        QCOMPARE(recorder.buffer(), (std::vector<float>{2.0f, 2.0f, 2.0f, 2.0f}));
        QCOMPARE(recorder.passCount(), 1);
    }

    void buildPunchRecordingCommandReturnsNullWhenNothingCaptured() {
        auto track = std::make_shared<Track>();
        PunchRecorder recorder;
        recorder.prepare(PunchRegion{0, 10}, 2);

        auto cmd = buildPunchRecordingCommand(track, recorder, 48000);
        QVERIFY(cmd == nullptr);
    }

    void buildPunchRecordingCommandProducesSingleUndoableClipAdd() {
        auto track = std::make_shared<Track>();
        PunchRecorder recorder;
        recorder.prepare(PunchRegion{100, 104}, 1);
        std::vector<float> audio = {0.1f, 0.2f, 0.3f, 0.4f};
        recorder.process(100, audio.data(), 4);

        // Simulate a second loop pass replacing the first.
        std::vector<float> audio2 = {0.5f, 0.6f, 0.7f, 0.8f};
        recorder.process(100, audio2.data(), 4);
        QCOMPARE(recorder.passCount(), 1);

        auto cmd = buildPunchRecordingCommand(track, recorder, 48000);
        QVERIFY(cmd != nullptr);

        // addClip() already happened inside buildPunchRecordingCommand;
        // pushing onto the stack just records the undo boundary.
        CommandStack stack;
        stack.push(std::move(cmd));

        QCOMPARE(track->clipsSnapshot()->size(), size_t(1));
        auto clip = track->clipsSnapshot()->front();
        QCOMPARE(clip->sessionStartSample, int64_t(100));
        QCOMPARE(clip->lengthSamples, int64_t(4));
        QCOMPARE(clip->buffer->samples, (std::vector<float>{0.5f, 0.6f, 0.7f, 0.8f}));

        // One undo reverts the entire multi-pass session in a single step.
        stack.undo();
        QCOMPARE(track->clipsSnapshot()->size(), size_t(0));

        stack.redo();
        QCOMPARE(track->clipsSnapshot()->size(), size_t(1));
    }
};

QTEST_APPLESS_MAIN(TestPunchRecording)
#include "test_PunchRecording.moc"
