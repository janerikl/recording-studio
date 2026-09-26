#include <QtTest/QtTest>

#include "ui/LibraryFilterMath.h"

using namespace rsd;

namespace {
LibraryItem makeItem(const QString& name) {
    LibraryItem item;
    item.source = LibrarySource::Loop;
    item.name = name;
    item.path = "/tmp/" + name;
    return item;
}
} // namespace

class LibraryFilterMathTests : public QObject {
    Q_OBJECT

private slots:
    void emptyFilter_keepsAllItems() {
        QVector<LibraryItem> items{makeItem("chirp_sweep.wav"), makeItem("tone_440hz.wav")};
        auto result = filterLibraryItems(items, QString());
        QCOMPARE(result.size(), 2);
    }

    void filter_keepsOnlyMatchingSubstring() {
        QVector<LibraryItem> items{makeItem("stereo_tones.wav"), makeItem("click_pattern.wav"),
                                    makeItem("Instrument 2 - Instrument Roll.wav")};
        auto result = filterLibraryItems(items, "stereo");
        QCOMPARE(result.size(), 1);
        QCOMPARE(result[0].name, QString("stereo_tones.wav"));
    }

    void filter_isCaseInsensitive() {
        QVector<LibraryItem> items{makeItem("Take 1"), makeItem("Recording")};
        auto result = filterLibraryItems(items, "TAKE");
        QCOMPARE(result.size(), 1);
        QCOMPARE(result[0].name, QString("Take 1"));
    }

    void filter_noMatches_returnsEmpty() {
        QVector<LibraryItem> items{makeItem("kick.wav")};
        auto result = filterLibraryItems(items, "zzz");
        QCOMPARE(result.size(), 0);
    }
};

QTEST_APPLESS_MAIN(LibraryFilterMathTests)
#include "test_LibraryFilterMath.moc"
