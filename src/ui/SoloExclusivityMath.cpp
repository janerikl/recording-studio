#include "ui/SoloExclusivityMath.h"

namespace rsd {

QVector<QString> tracksToUnsolo(const QVector<QString>& soloedIds, const QString& toggledId) {
    QVector<QString> result;
    if (!soloedIds.contains(toggledId)) return result;

    for (const auto& id : soloedIds) {
        if (id != toggledId) result.push_back(id);
    }
    return result;
}

} // namespace rsd
