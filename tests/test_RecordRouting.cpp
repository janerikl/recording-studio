#include <QtTest>

#include "audio/RecordRouting.h"
#include "model/Track.h"

using namespace rsd;

class TestRecordRouting : public QObject {
    Q_OBJECT

private slots:
    void defaultsToMic() {
        Track t;
        QCOMPARE(t.inputSource.load(), AudioSource::Mic);
    }

    void splitRoutesEachTrackByItsSource() {
        auto mic1 = std::make_shared<Track>();
        auto sys1 = std::make_shared<Track>();
        sys1->inputSource.store(AudioSource::SystemAudio);
        auto mic2 = std::make_shared<Track>();

        auto split = splitTracksBySource({mic1, sys1, mic2});

        QCOMPARE(split.micTracks.size(), size_t(2));
        QCOMPARE(split.micTracks[0], mic1);
        QCOMPARE(split.micTracks[1], mic2);
        QCOMPARE(split.systemAudioTracks.size(), size_t(1));
        QCOMPARE(split.systemAudioTracks[0], sys1);
    }

    void emptyInputProducesEmptySplit() {
        auto split = splitTracksBySource({});
        QVERIFY(split.micTracks.empty());
        QVERIFY(split.systemAudioTracks.empty());
    }

    void filterRecordableTracksKeepsOnlyAudioKind() {
        auto audio = std::make_shared<Track>();
        auto instrument = std::make_shared<Track>();
        instrument->kind = TrackKind::Instrument;
        auto bus = std::make_shared<Track>();
        bus->kind = TrackKind::Bus;

        auto recordable = filterRecordableTracks({audio, instrument, bus});

        QCOMPARE(recordable.size(), size_t(1));
        QCOMPARE(recordable[0], audio);
    }

    void allSystemAudioLeavesMicEmpty() {
        auto sys1 = std::make_shared<Track>();
        sys1->inputSource.store(AudioSource::SystemAudio);
        auto sys2 = std::make_shared<Track>();
        sys2->inputSource.store(AudioSource::SystemAudio);

        auto split = splitTracksBySource({sys1, sys2});

        QVERIFY(split.micTracks.empty());
        QCOMPARE(split.systemAudioTracks.size(), size_t(2));
    }
};

QTEST_MAIN(TestRecordRouting)
#include "test_RecordRouting.moc"
