#include <QtTest>

#include "ui/PopoverPositioning.h"

using rsd::computePopoverPosition;

class PopoverPositioningTests : public QObject {
    Q_OBJECT

private slots:
    // Plenty of room below the button: popover hangs from the button's
    // bottom-left corner.
    void fitsBelow_anchorsToBottomLeftOfButton() {
        QRect button(100, 100, 60, 24);
        QSize popover(280, 400);
        QRect screen(0, 0, 1920, 1080);
        QPoint pos = computePopoverPosition(button, popover, screen);
        QCOMPARE(pos, QPoint(100, 124));
    }

    // Not enough room below, but enough above: flips to sit above the
    // button instead of running off the bottom of the screen.
    void noRoomBelow_flipsAbove() {
        QRect button(100, 900, 60, 24);
        QSize popover(280, 400);
        QRect screen(0, 0, 1920, 1080);
        QPoint pos = computePopoverPosition(button, popover, screen);
        QCOMPARE(pos, QPoint(100, 900 - 400));
    }

    // Would overflow the right edge: shifted left just enough to stay
    // fully on screen.
    void overflowsRight_shiftsLeftToStayOnScreen() {
        QRect button(1800, 100, 60, 24);
        QSize popover(280, 400);
        QRect screen(0, 0, 1920, 1080);
        QPoint pos = computePopoverPosition(button, popover, screen);
        QCOMPARE(pos.x(), 1920 - 280);
        QCOMPARE(pos.y(), 124);
    }

    // Would overflow the left edge: clamped to screen's left edge.
    void overflowsLeft_clampsToScreenLeft() {
        QRect button(-50, 100, 60, 24);
        QSize popover(280, 400);
        QRect screen(0, 0, 1920, 1080);
        QPoint pos = computePopoverPosition(button, popover, screen);
        QCOMPARE(pos.x(), 0);
    }

    // Neither above nor below fully fits (tiny screen): clamps to the
    // top of the screen rather than running off both edges.
    void noRoomEitherSide_clampsToScreenTop() {
        QRect button(10, 100, 60, 24);
        QSize popover(280, 400);
        QRect screen(0, 0, 400, 300);
        QPoint pos = computePopoverPosition(button, popover, screen);
        QVERIFY(pos.y() >= 0);
        QVERIFY(pos.y() + popover.height() <= screen.height() || pos.y() == 0);
    }
};

QTEST_MAIN(PopoverPositioningTests)
#include "test_PopoverPositioning.moc"
