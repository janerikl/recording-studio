#include <QTest>

#include "model/PracticeExercise.h"

using namespace rsd;

class PracticeExerciseTests : public QObject {
    Q_OBJECT

private slots:
    void hasBuiltInExercises() { QVERIFY(builtInPracticeExercises().size() >= 2); }

    void noExerciseIsEmpty() {
        for (const auto& ex : builtInPracticeExercises()) {
            QVERIFY(!ex.name.isEmpty());
            QVERIFY(!ex.pitches.empty());
        }
    }

    void cMajorScaleIsSevenNotesUp() {
        const auto& exercises = builtInPracticeExercises();
        auto it = std::find_if(exercises.begin(), exercises.end(),
                                [](const PracticeExercise& e) { return e.name.contains("C Major"); });
        QVERIFY(it != exercises.end());
        QCOMPARE(it->pitches, (std::vector<int>{60, 62, 64, 65, 67, 69, 71, 72}));
    }
};

QTEST_MAIN(PracticeExerciseTests)
#include "test_PracticeExercise.moc"
