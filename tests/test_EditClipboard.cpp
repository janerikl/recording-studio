#include <QTest>

#include "model/EditClipboard.h"

using namespace rsd;

namespace {

std::shared_ptr<MidiNote> makeNote(int64_t start, int64_t length, int pitch = 60) {
    auto n = std::make_shared<MidiNote>();
    n->startSample = start;
    n->lengthSamples = length;
    n->pitch = pitch;
    return n;
}

std::shared_ptr<Clip> makeClip(int64_t start, int64_t length, QString name = "c") {
    auto c = std::make_shared<Clip>();
    c->sessionStartSample = start;
    c->lengthSamples = length;
    c->name = std::move(name);
    return c;
}

} // namespace

class EditClipboardTests : public QObject {
    Q_OBJECT

private slots:
    void extractRangeIncludesOnlyOverlappingNotes() {
        std::vector<std::shared_ptr<MidiNote>> notes = {
            makeNote(0, 100),    // entirely before range
            makeNote(150, 100),  // partially overlaps range start
            makeNote(300, 50),   // entirely inside range
            makeNote(900, 100),  // entirely after range
        };
        auto clip = extractRange<MidiNote>(notes, 200, 400);
        QCOMPARE(clip.items.size(), size_t(2));
        QCOMPARE(clip.baseSample, int64_t(200));
        QCOMPARE(clip.spanSamples, int64_t(200));
    }

    void extractRangeDeepCopiesItems() {
        std::vector<std::shared_ptr<MidiNote>> notes = {makeNote(0, 100)};
        auto clip = extractRange<MidiNote>(notes, 0, 100);
        notes[0]->pitch = 99;
        QCOMPARE(clip.items[0]->pitch, 60);
    }

    void pasteRangeOffsetsToTargetSample() {
        std::vector<std::shared_ptr<MidiNote>> notes = {makeNote(200, 50), makeNote(300, 50)};
        auto clip = extractRange<MidiNote>(notes, 200, 400);
        auto pasted = pasteRange<MidiNote>({}, clip, 1000);
        QCOMPARE(pasted.size(), size_t(2));
        QCOMPARE(pasted[0]->startSample, int64_t(1000));
        QCOMPARE(pasted[1]->startSample, int64_t(1100));
    }

    void pasteRangeAssignsFreshIds() {
        std::vector<std::shared_ptr<MidiNote>> notes = {makeNote(0, 50)};
        auto clip = extractRange<MidiNote>(notes, 0, 50);
        auto pasted = pasteRange<MidiNote>({}, clip, 500);
        QVERIFY(pasted[0]->id != clip.items[0]->id);
    }

    void pasteRangeOverwritesOverlappingDestinationItems() {
        std::vector<std::shared_ptr<MidiNote>> destination = {
            makeNote(0, 50),      // untouched, before paste region
            makeNote(1020, 30),   // overlaps pasted region, removed
            makeNote(2000, 50),   // untouched, after paste region
        };
        std::vector<std::shared_ptr<MidiNote>> source = {makeNote(0, 100)};
        auto clip = extractRange<MidiNote>(source, 0, 100);
        auto result = pasteRange<MidiNote>(destination, clip, 1000);

        QCOMPARE(result.size(), size_t(3)); // 2 surviving + 1 pasted
        bool sawOverwritten =
            std::any_of(result.begin(), result.end(), [](auto& n) { return n->startSample == 1020; });
        QVERIFY(!sawOverwritten);
    }

    void pasteRangeWorksForClipsToo() {
        std::vector<std::shared_ptr<Clip>> destination = {makeClip(1000, 100)};
        std::vector<std::shared_ptr<Clip>> source = {makeClip(0, 200)};
        auto clip = extractRange<Clip>(source, 0, 200);
        auto result = pasteRange<Clip>(destination, clip, 500);

        QCOMPARE(result.size(), size_t(2)); // original clip at 1000 survives (no overlap), plus pasted
        QCOMPARE(result.back()->sessionStartSample, int64_t(500));
    }

    void insertItemAfterInsertsCopyAtTargetWithFreshId() {
        std::vector<std::shared_ptr<Clip>> source = {makeClip(0, 100)};
        auto result = insertItemAfter<Clip>(source, *source[0], 100);

        QCOMPARE(result.size(), size_t(2));
        QCOMPARE(result[1]->sessionStartSample, int64_t(100));
        QCOMPARE(result[1]->lengthSamples, int64_t(100));
        QVERIFY(result[1]->id != source[0]->id);
    }

    void insertItemAfterShiftsLaterItemsByInsertedLength() {
        std::vector<std::shared_ptr<MidiNote>> source = {
            makeNote(0, 50),    // original, before insertion point
            makeNote(50, 200),  // starts exactly at insertion point, must shift
            makeNote(500, 30),  // well after, must shift
        };
        auto result = insertItemAfter<MidiNote>(source, *source[0], 50);

        QCOMPARE(result.size(), size_t(4));
        // untouched original
        QVERIFY(std::any_of(result.begin(), result.end(),
                             [](auto& n) { return n->startSample == 0 && n->lengthSamples == 50; }));
        // pasted copy lands exactly at the insertion point
        QVERIFY(std::any_of(result.begin(), result.end(),
                             [](auto& n) { return n->startSample == 50 && n->lengthSamples == 50; }));
        // item that started at the insertion point shifted right by the inserted length
        QVERIFY(std::any_of(result.begin(), result.end(),
                             [](auto& n) { return n->startSample == 100 && n->lengthSamples == 200; }));
        // item well after also shifted right by the inserted length
        QVERIFY(std::any_of(result.begin(), result.end(),
                             [](auto& n) { return n->startSample == 550 && n->lengthSamples == 30; }));
    }

    void insertItemAfterDoesNotShiftItemsBeforeInsertionPoint() {
        std::vector<std::shared_ptr<MidiNote>> source = {makeNote(0, 50), makeNote(60, 20)};
        auto result = insertItemAfter<MidiNote>(source, *source[0], 200);

        QVERIFY(std::any_of(result.begin(), result.end(),
                             [](auto& n) { return n->startSample == 0 && n->lengthSamples == 50; }));
        QVERIFY(std::any_of(result.begin(), result.end(),
                             [](auto& n) { return n->startSample == 60 && n->lengthSamples == 20; }));
    }

    void singleItemClipboardSetAndGetRoundTrip() {
        EditClipboardStore store;
        auto clip = makeClip(0, 100);
        QUuid trackId = QUuid::createUuid();
        store.setSingleClip({clip, 100, trackId});

        QVERIFY(store.hasSingleClip());
        QVERIFY(!store.hasSingleMidi());
        QCOMPARE(store.singleClip().nextPasteSample, int64_t(100));
        QCOMPARE(store.singleClip().sourceTrackId, trackId);
    }

    void singleItemClipboardAdvancesChainPointer() {
        EditClipboardStore store;
        auto clip = makeClip(0, 100);
        store.setSingleClip({clip, 100, QUuid::createUuid()});
        store.advanceSingleClip(200);

        QCOMPARE(store.singleClip().nextPasteSample, int64_t(200));
    }

    void settingSingleClipClearsRangeAndMidiClipboards() {
        EditClipboardStore store;
        std::vector<std::shared_ptr<MidiNote>> notes = {makeNote(0, 50)};
        store.setMidi(extractRange<MidiNote>(notes, 0, 50));
        QVERIFY(store.hasMidi());

        store.setSingleClip({makeClip(0, 100), 100, QUuid::createUuid()});

        QVERIFY(!store.hasMidi());
        QVERIFY(!store.hasSingleMidi());
        QVERIFY(store.hasSingleClip());
    }

    void repeatItemAfterInsertsCountCopiesBackToBack() {
        std::vector<std::shared_ptr<Clip>> source = {makeClip(0, 100)};
        auto result = repeatItemAfter<Clip>(source, *source[0], 100, 3);

        QCOMPARE(result.size(), size_t(4)); // original + 3 repeats
        std::vector<int64_t> starts;
        for (auto& c : result) starts.push_back(c->sessionStartSample);
        std::sort(starts.begin(), starts.end());
        QCOMPARE(starts, (std::vector<int64_t>{0, 100, 200, 300}));
    }

    void repeatItemAfterPushesLaterItemsOutOfTheWay() {
        std::vector<std::shared_ptr<Clip>> source = {makeClip(0, 100), makeClip(150, 50)};
        auto result = repeatItemAfter<Clip>(source, *source[0], 100, 2);

        QCOMPARE(result.size(), size_t(4)); // original + clip-at-150 + 2 repeats
        bool sawShiftedSurvivor = std::any_of(result.begin(), result.end(), [](auto& c) {
            return c->sessionStartSample == 350 && c->lengthSamples == 50;
        });
        QVERIFY(sawShiftedSurvivor);
    }

    void settingRangeClipboardClearsSingleItemClipboards() {
        EditClipboardStore store;
        store.setSingleMidi({makeNote(0, 50), 50, QUuid::createUuid()});
        QVERIFY(store.hasSingleMidi());

        std::vector<std::shared_ptr<Clip>> clips = {makeClip(0, 100)};
        store.setClips(extractRange<Clip>(clips, 0, 100));

        QVERIFY(!store.hasSingleMidi());
        QVERIFY(!store.hasSingleClip());
        QVERIFY(store.hasClips());
    }
};

QTEST_APPLESS_MAIN(EditClipboardTests)
#include "test_EditClipboard.moc"
