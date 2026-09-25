#include <QTest>

#include "audio/GMDrumMap.h"

using namespace rsd;

class GMDrumMapTests : public QObject {
    Q_OBJECT

private slots:
    void hasPads() { QVERIFY(gmDrumPads().size() >= 8); }

    void kickIsPitch36() {
        const auto& pads = gmDrumPads();
        auto it = std::find_if(pads.begin(), pads.end(), [](const GMDrumPad& p) { return p.pitch == 36; });
        QVERIFY(it != pads.end());
        QCOMPARE(it->label, QStringLiteral("Kick"));
    }

    void allPitchesUnique() {
        const auto& pads = gmDrumPads();
        for (size_t i = 0; i < pads.size(); ++i) {
            for (size_t j = i + 1; j < pads.size(); ++j) {
                QVERIFY(pads[i].pitch != pads[j].pitch);
            }
        }
    }

    void noEmptyLabels() {
        for (const auto& p : gmDrumPads()) QVERIFY(!p.label.isEmpty());
    }
};

QTEST_MAIN(GMDrumMapTests)
#include "test_GMDrumMap.moc"
