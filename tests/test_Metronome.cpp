#include <QTest>

#include <algorithm>
#include <vector>

#include "audio/Metronome.h"

using namespace rsd;

namespace {
bool anyNonZero(const std::vector<float>& buf) {
    for (float v : buf) {
        if (v != 0.0f) return true;
    }
    return false;
}
} // namespace

class MetronomeTests : public QObject {
    Q_OBJECT

private slots:
    void disabledProducesSilence() {
        Metronome m;
        std::vector<float> out(512 * 2, 0.0f);
        m.render(out.data(), 512, 2, 48000, 0, 120.0, false);
        QVERIFY(!anyNonZero(out));
    }

    void clicksAtBlockStart() {
        Metronome m;
        std::vector<float> out(512 * 2, 0.0f);
        m.render(out.data(), 512, 2, 48000, 0, 120.0, true);
        QVERIFY(anyNonZero(out));
    }

    void noClickFarFromBeatBoundary() {
        Metronome m;
        std::vector<float> out(512 * 2, 0.0f);
        // 120 BPM @ 48kHz = beat every 24000 samples; well clear of one here.
        m.render(out.data(), 512, 2, 48000, 5000, 120.0, true);
        QVERIFY(!anyNonZero(out));
    }

    void clickEventuallyDecaysToSilence() {
        Metronome m;
        std::vector<float> out(512 * 2, 0.0f);
        m.render(out.data(), 512, 2, 48000, 0, 120.0, true);
        for (int i = 0; i < 20; ++i) {
            std::fill(out.begin(), out.end(), 0.0f);
            m.render(out.data(), 512, 2, 48000, static_cast<int64_t>(512) * (i + 1), 120.0, true);
        }
        QVERIFY(!anyNonZero(out));
    }
};

QTEST_MAIN(MetronomeTests)
#include "test_Metronome.moc"
