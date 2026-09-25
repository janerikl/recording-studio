#include <QTest>

#include "model/PracticeMath.h"

using namespace rsd;

class PracticeMathTests : public QObject {
    Q_OBJECT

private slots:
    void correctNoteAdvances() {
        std::vector<int> pitches = {60, 62, 64};
        QCOMPARE(practiceAdvance(pitches, 0, 60), size_t(1));
    }

    void wrongNoteDoesNotAdvance() {
        std::vector<int> pitches = {60, 62, 64};
        QCOMPARE(practiceAdvance(pitches, 0, 61), size_t(0));
    }

    void alreadyCompleteStaysComplete() {
        std::vector<int> pitches = {60, 62, 64};
        QCOMPARE(practiceAdvance(pitches, 3, 60), size_t(3));
    }

    void completionDetection() {
        std::vector<int> pitches = {60, 62, 64};
        QVERIFY(!practiceComplete(pitches, 2));
        QVERIFY(practiceComplete(pitches, 3));
    }

    void emptyExerciseIsImmediatelyComplete() {
        std::vector<int> pitches;
        QVERIFY(practiceComplete(pitches, 0));
    }
};

QTEST_MAIN(PracticeMathTests)
#include "test_PracticeMath.moc"
