#include <QSet>
#include <QTest>
#include <algorithm>

#include "model/RhythmPattern.h"

using namespace rsd;

class RhythmPatternTests : public QObject {
    Q_OBJECT

private slots:
    void hasSevenBuiltInPatterns() { QCOMPARE(builtInRhythmPatterns().size(), size_t(7)); }

    void mozzarellaIsFourSixteenths() {
        const auto& p = builtInRhythmPatterns()[0];
        QCOMPARE(p.word, QString("Mozzarella"));
        QCOMPARE(p.notes.size(), size_t(4));
        for (auto& n : p.notes) {
            QCOMPARE(n.beats, 0.25);
            QCOMPARE(n.isRest, false);
        }
    }

    void orangeHasTwoRests() {
        const auto& patterns = builtInRhythmPatterns();
        auto it = std::find_if(patterns.begin(), patterns.end(),
                                [](const RhythmPattern& p) { return p.word == "Orange"; });
        QVERIFY(it != patterns.end());
        int restCount = 0;
        for (auto& n : it->notes) {
            if (n.isRest) ++restCount;
        }
        QCOMPARE(restCount, 2);
    }

    void allPatternWordsAreUnique() {
        const auto& patterns = builtInRhythmPatterns();
        QSet<QString> words;
        for (auto& p : patterns) words.insert(p.word);
        QCOMPARE(words.size(), patterns.size());
    }
};

QTEST_APPLESS_MAIN(RhythmPatternTests)
#include "test_RhythmPattern.moc"
