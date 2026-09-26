#include <QTest>

#include "model/LibraryItem.h"

using namespace rsd;

class LibraryItemTests : public QObject {
    Q_OBJECT

private slots:
    void projectMediaId_derivedFromPersistentItemId() {
        LibraryItem item;
        item.source = LibrarySource::ProjectMedia;
        item.itemId = "abc-123";
        QCOMPARE(item.id(), QString("pm:abc-123"));
    }

    void loopId_derivedFromCanonicalPath() {
        LibraryItem item;
        item.source = LibrarySource::Loop;
        item.path = "/home/user/loops/kick.wav";
        QCOMPARE(item.id(), QString("loop:/home/user/loops/kick.wav"));
    }

    void sameProjectMediaItemId_producesSameId() {
        LibraryItem a;
        a.source = LibrarySource::ProjectMedia;
        a.itemId = "same-id";

        LibraryItem b;
        b.source = LibrarySource::ProjectMedia;
        b.itemId = "same-id";

        QCOMPARE(a.id(), b.id());
    }

    void differentSources_neverCollide() {
        LibraryItem pm;
        pm.source = LibrarySource::ProjectMedia;
        pm.itemId = "x";

        LibraryItem loop;
        loop.source = LibrarySource::Loop;
        loop.path = "x";

        QVERIFY(pm.id() != loop.id());
    }

    void isValid_falseForDefaultConstructedItem() {
        LibraryItem item;
        QVERIFY(!item.isValid());
    }

    void isValid_trueOnceIdentityIsSet() {
        LibraryItem pm;
        pm.source = LibrarySource::ProjectMedia;
        pm.itemId = "abc";
        QVERIFY(pm.isValid());

        LibraryItem loop;
        loop.source = LibrarySource::Loop;
        loop.path = "/a/b.wav";
        QVERIFY(loop.isValid());
    }
};

QTEST_MAIN(LibraryItemTests)
#include "test_LibraryItem.moc"
