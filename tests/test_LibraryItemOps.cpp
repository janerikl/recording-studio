#include <QtTest/QtTest>

#include "ui/LibraryItemOps.h"

using namespace rsd;

namespace {
LibraryItem makeProjectItem(const QString& itemId, const QString& name) {
    LibraryItem item;
    item.source = LibrarySource::ProjectMedia;
    item.itemId = itemId;
    item.name = name;
    return item;
}

LibraryItem makeLoopItem(const QString& path, const QString& name) {
    LibraryItem item;
    item.source = LibrarySource::Loop;
    item.path = path;
    item.name = name;
    return item;
}
} // namespace

class LibraryItemOpsTests : public QObject {
    Q_OBJECT

private slots:
    void renameLibraryItemById_matchingId_updatesName() {
        QVector<LibraryItem> items{makeProjectItem("a", "Take 1"), makeProjectItem("b", "Take 2")};
        auto result = renameLibraryItemById(items, "pm:a", "Best Take");
        QCOMPARE(result[0].name, QString("Best Take"));
        QCOMPARE(result[1].name, QString("Take 2"));
    }

    void renameLibraryItemById_unknownId_isNoop() {
        QVector<LibraryItem> items{makeLoopItem("/a/kick.wav", "kick.wav")};
        auto result = renameLibraryItemById(items, "loop:/nope.wav", "New Name");
        QCOMPARE(result[0].name, QString("kick.wav"));
    }

    void removeLibraryItemById_matchingId_dropsIt() {
        QVector<LibraryItem> items{makeProjectItem("a", "Take 1"), makeProjectItem("b", "Take 2")};
        auto result = removeLibraryItemById(items, "pm:a");
        QCOMPARE(result.size(), 1);
        QCOMPARE(result[0].itemId, QString("b"));
    }

    void removeLibraryItemById_unknownId_leavesListUnchanged() {
        QVector<LibraryItem> items{makeLoopItem("/a/kick.wav", "kick.wav")};
        auto result = removeLibraryItemById(items, "loop:/nope.wav");
        QCOMPARE(result.size(), 1);
    }
};

QTEST_APPLESS_MAIN(LibraryItemOpsTests)
#include "test_LibraryItemOps.moc"
