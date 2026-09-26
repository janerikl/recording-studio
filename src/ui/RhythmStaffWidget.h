#pragma once

#include <QWidget>

#include "model/RhythmPattern.h"

namespace rsd {

// Paint-only widget: draws a simplified single-pitch rhythm staff for one
// RhythmPattern (see PracticePanel's Rhythm mode) — noteheads spaced by
// duration, beamed in runs of eighth/sixteenth notes, with rest glyphs.
// Not pitch-accurate (every notehead sits on the same line) since this
// trains duration reading only, matching the reference teaching sheet.
class RhythmStaffWidget : public QWidget {
    Q_OBJECT

public:
    explicit RhythmStaffWidget(QWidget* parent = nullptr);

    void setPattern(const RhythmPattern& pattern);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    RhythmPattern m_pattern;
};

} // namespace rsd
