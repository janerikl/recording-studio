#pragma once

#include <QPoint>
#include <QRect>
#include <QSize>

namespace rsd {

// Where to place a popover of `popoverSize` so it sits below the button
// that triggered it (`anchorGlobalRect`, in global screen coordinates) and
// stays fully within `screenGeometry`. Falls back to appearing above the
// button when there isn't room below, and clamps horizontally/vertically
// as a last resort so the popover is never partially off-screen.
inline QPoint computePopoverPosition(const QRect& anchorGlobalRect, const QSize& popoverSize,
                                      const QRect& screenGeometry) {
    int x = anchorGlobalRect.left();
    int y = anchorGlobalRect.top() + anchorGlobalRect.height();

    bool fitsBelow = y + popoverSize.height() <= screenGeometry.bottom() + 1;
    bool fitsAbove = anchorGlobalRect.top() - popoverSize.height() >= screenGeometry.top();
    if (!fitsBelow && fitsAbove) {
        y = anchorGlobalRect.top() - popoverSize.height();
    } else if (!fitsBelow) {
        // Doesn't fully fit either side: clamp to the screen's top edge.
        y = screenGeometry.top();
    }

    int maxX = screenGeometry.right() + 1 - popoverSize.width();
    if (x > maxX) x = maxX;
    if (x < screenGeometry.left()) x = screenGeometry.left();

    return QPoint(x, y);
}

} // namespace rsd
