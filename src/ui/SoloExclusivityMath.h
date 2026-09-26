#pragma once

#include <QString>
#include <QVector>

namespace rsd {

// Track ids (as track->id.toString()) whose solo should be cleared when
// `toggledId` was just soloed on. Exclusive solo: only one track may be
// soloed at a time, so every other currently-soloed track must be
// un-soloed. Returns an empty list when `toggledId` isn't in `soloedIds`
// (i.e. this call was for un-soloing, not soloing).
QVector<QString> tracksToUnsolo(const QVector<QString>& soloedIds, const QString& toggledId);

} // namespace rsd
