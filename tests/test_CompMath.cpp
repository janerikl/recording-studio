#include <QTest>

#include <algorithm>

#include "model/CompMath.h"

using namespace rsd;

namespace {
std::shared_ptr<Clip> makeClip(int64_t start, int64_t length, int64_t sourceOffset = 0) {
    auto c = std::make_shared<Clip>();
    c->sessionStartSample = start;
    c->sourceOffsetSamples = sourceOffset;
    c->lengthSamples = length;
    return c;
}
} // namespace

class CompMathTests : public QObject {
    Q_OBJECT

private slots:
    void emptyTrack_justInsertsTake() {
        auto take = makeClip(0, 100);
        auto result = promoteTakeToComp({}, take, 100, 200);

        QCOMPARE(result.size(), size_t(1));
        QCOMPARE(result[0]->sessionStartSample, int64_t(100));
        QCOMPARE(result[0]->lengthSamples, int64_t(100));
    }

    void nonOverlappingClips_passThroughUnchanged() {
        auto before = makeClip(0, 50);   // ends at 50, region starts at 100
        auto after = makeClip(300, 50);  // starts at 300, region ends at 200
        auto take = makeClip(0, 100);

        auto result = promoteTakeToComp({before, after}, take, 100, 200);

        QCOMPARE(result.size(), size_t(3));
        QVERIFY(std::any_of(result.begin(), result.end(),
                             [](auto& c) { return c->sessionStartSample == 0 && c->lengthSamples == 50; }));
        QVERIFY(std::any_of(result.begin(), result.end(), [](auto& c) {
            return c->sessionStartSample == 300 && c->lengthSamples == 50;
        }));
    }

    void clipFullyInsideRegion_isDropped() {
        auto inside = makeClip(120, 30); // [120, 150) fully within [100, 200)
        auto take = makeClip(0, 100);

        auto result = promoteTakeToComp({inside}, take, 100, 200);

        QCOMPARE(result.size(), size_t(1)); // only the promoted take remains
        QCOMPARE(result[0]->sessionStartSample, int64_t(100));
    }

    void clipStraddlingStart_trimmedToLeftRemainder() {
        auto straddling = makeClip(50, 100); // [50, 150), region [100, 200)
        auto take = makeClip(0, 100);

        auto result = promoteTakeToComp({straddling}, take, 100, 200);

        QCOMPARE(result.size(), size_t(2));
        auto leftIt = std::find_if(result.begin(), result.end(),
                                    [](auto& c) { return c->sessionStartSample == 50; });
        QVERIFY(leftIt != result.end());
        QCOMPARE((*leftIt)->lengthSamples, int64_t(50)); // trimmed to end at 100
    }

    void clipStraddlingEnd_trimmedToRightRemainderWithAdvancedOffset() {
        auto straddling = makeClip(150, 100, /*sourceOffset=*/10); // [150, 250), region [100, 200)
        auto take = makeClip(0, 100);

        auto result = promoteTakeToComp({straddling}, take, 100, 200);

        QCOMPARE(result.size(), size_t(2));
        auto rightIt = std::find_if(result.begin(), result.end(),
                                     [](auto& c) { return c->sessionStartSample == 200; });
        QVERIFY(rightIt != result.end());
        QCOMPARE((*rightIt)->lengthSamples, int64_t(50)); // trimmed to start at 200
        QCOMPARE((*rightIt)->sourceOffsetSamples, int64_t(10 + 50)); // advanced by the 50 cut off the front
    }

    void clipSpanningWholeRegion_splitsIntoTwoRemainders() {
        auto spanning = makeClip(50, 300); // [50, 350), region [100, 200)
        auto take = makeClip(0, 100);

        auto result = promoteTakeToComp({spanning}, take, 100, 200);

        QCOMPARE(result.size(), size_t(3)); // left remainder, right remainder, promoted take
        auto leftIt = std::find_if(result.begin(), result.end(),
                                    [](auto& c) { return c->sessionStartSample == 50; });
        auto rightIt = std::find_if(result.begin(), result.end(),
                                     [](auto& c) { return c->sessionStartSample == 200; });
        QVERIFY(leftIt != result.end());
        QVERIFY(rightIt != result.end());
        QCOMPARE((*leftIt)->lengthSamples, int64_t(50));  // [50, 100)
        QCOMPARE((*rightIt)->lengthSamples, int64_t(150)); // [200, 350)
    }

    void promotedTake_alwaysCoversExactRegion() {
        auto take = makeClip(999, 5); // take's own stored position is irrelevant
        auto result = promoteTakeToComp({}, take, 100, 250);

        QCOMPARE(result.size(), size_t(1));
        QCOMPARE(result[0]->sessionStartSample, int64_t(100));
        QCOMPARE(result[0]->lengthSamples, int64_t(150));
        QCOMPARE(result[0]->sourceOffsetSamples, int64_t(0));
    }
};

QTEST_MAIN(CompMathTests)
#include "test_CompMath.moc"
