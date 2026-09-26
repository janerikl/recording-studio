#include <QTest>

#include "model/LibraryFolder.h"

using namespace rsd;

class LibraryFolderTests : public QObject {
    Q_OBJECT

private slots:
    void addFolder_topLevel_appearsInRoots() {
        LibraryFolderTree tree;
        auto folder = tree.addFolder("Drums");
        QCOMPARE(tree.roots.size(), 1);
        QCOMPARE(tree.roots[0], folder);
        QCOMPARE(folder->name, QString("Drums"));
    }

    void addFolder_nested_appearsUnderParent() {
        LibraryFolderTree tree;
        auto parent = tree.addFolder("Drums");
        auto child = tree.addFolder("Kicks", parent.get());

        QCOMPARE(tree.roots.size(), 1);
        QCOMPARE(parent->children.size(), 1);
        QCOMPARE(parent->children[0], child);
    }

    void moveItem_addsRefToTargetFolder() {
        LibraryFolderTree tree;
        auto folder = tree.addFolder("Drums");
        tree.moveItem("loop:/a/kick.wav", folder.get());

        QCOMPARE(folder->itemRefs.size(), 1);
        QCOMPARE(folder->itemRefs[0], QString("loop:/a/kick.wav"));
    }

    void moveItem_removesRefFromPreviousFolder() {
        LibraryFolderTree tree;
        auto a = tree.addFolder("A");
        auto b = tree.addFolder("B");

        tree.moveItem("pm:1", a.get());
        QCOMPARE(a->itemRefs.size(), 1);

        tree.moveItem("pm:1", b.get());
        QVERIFY(a->itemRefs.isEmpty());
        QCOMPARE(b->itemRefs.size(), 1);
    }

    void moveItem_toNullptr_leavesItemUnfiled() {
        LibraryFolderTree tree;
        auto a = tree.addFolder("A");
        tree.moveItem("pm:1", a.get());
        tree.moveItem("pm:1", nullptr);
        QVERIFY(a->itemRefs.isEmpty());
    }

    void findFolder_locatesNestedFolderById() {
        LibraryFolderTree tree;
        auto parent = tree.addFolder("Drums");
        auto child = tree.addFolder("Kicks", parent.get());

        auto found = tree.findFolder(child->id);
        QCOMPARE(found, child.get());
    }

    void removeFolder_topLevel_removesFromRoots() {
        LibraryFolderTree tree;
        auto folder = tree.addFolder("Drums");
        tree.removeFolder(folder->id);
        QVERIFY(tree.roots.isEmpty());
    }

    void removeFolder_nested_removesFromParentChildren() {
        LibraryFolderTree tree;
        auto parent = tree.addFolder("Drums");
        auto child = tree.addFolder("Kicks", parent.get());
        tree.removeFolder(child->id);
        QVERIFY(parent->children.isEmpty());
    }

    void renameFolder_existingId_updatesName() {
        LibraryFolderTree tree;
        auto folder = tree.addFolder("Drums");
        tree.renameFolder(folder->id, "Percussion");
        QCOMPARE(folder->name, QString("Percussion"));
    }

    void renameFolder_unknownId_isNoop() {
        LibraryFolderTree tree;
        auto folder = tree.addFolder("Drums");
        tree.renameFolder(QUuid::createUuid(), "Percussion");
        QCOMPARE(folder->name, QString("Drums"));
    }
};

QTEST_MAIN(LibraryFolderTests)
#include "test_LibraryFolder.moc"
