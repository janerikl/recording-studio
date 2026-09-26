#include "RhythmStaffWidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <vector>

namespace rsd {

namespace {
constexpr int kStaffLineSpacing = 10;
constexpr int kStemHeight = 34;
constexpr double kPixelsPerBeat = 140.0;
constexpr double kMinNoteSpacing = 40.0;
// Reserved width for the word label at the left, mirroring the reference
// teaching sheet's layout (word | notation, side by side) — this is the
// whole point of the mnemonic, so it's drawn directly next to its
// notation rather than living only in a combo box above.
constexpr int kWordLabelWidth = 150;
} // namespace

RhythmStaffWidget::RhythmStaffWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(90);
}

void RhythmStaffWidget::setPattern(const RhythmPattern& pattern) {
    m_pattern = pattern;
    update();
}

void RhythmStaffWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(30, 30, 30));

    int staffTop = 25;
    int middleLineY = staffTop + kStaffLineSpacing * 2;
    int staffLeft = kWordLabelWidth;

    painter.setPen(QColor(230, 230, 230));
    QFont wordFont = painter.font();
    wordFont.setBold(true);
    wordFont.setPointSize(wordFont.pointSize() + 2);
    painter.setFont(wordFont);
    painter.drawText(QRect(10, 0, kWordLabelWidth - 20, height()), Qt::AlignVCenter | Qt::AlignLeft,
                      m_pattern.word);

    painter.setPen(QColor(150, 150, 150));
    for (int i = 0; i < 5; ++i) {
        int y = staffTop + i * kStaffLineSpacing;
        painter.drawLine(staffLeft, y, width() - 10, y);
    }

    if (m_pattern.notes.empty()) return;

    // Layout: x-center for each note/rest, spaced proportional to duration.
    std::vector<double> centers;
    double x = staffLeft + 20.0;
    for (auto& note : m_pattern.notes) {
        double spacing = std::max(kMinNoteSpacing, note.beats * kPixelsPerBeat);
        centers.push_back(x + spacing / 2.0);
        x += spacing;
    }

    // Identify beam groups: maximal runs of consecutive non-rest notes
    // with duration <= an eighth note (0.5 beats).
    size_t n = m_pattern.notes.size();
    std::vector<int> groupId(n, -1);
    int currentGroup = -1;
    for (size_t i = 0; i < n; ++i) {
        const auto& note = m_pattern.notes[i];
        bool beamable = !note.isRest && note.beats <= 0.5 + 1e-9;
        if (beamable) {
            if (currentGroup == -1) currentGroup = static_cast<int>(i);
            groupId[i] = currentGroup;
        } else {
            currentGroup = -1;
        }
    }
    // Drop single-note "groups" (nothing to beam to) — draw as a lone flag instead.
    std::vector<int> groupCount(n, 0);
    for (size_t i = 0; i < n; ++i) {
        if (groupId[i] >= 0) groupCount[static_cast<size_t>(groupId[i])]++;
    }
    for (size_t i = 0; i < n; ++i) {
        if (groupId[i] >= 0 && groupCount[static_cast<size_t>(groupId[i])] < 2) groupId[i] = -1;
    }

    int stemTopY = middleLineY - kStemHeight;

    // Notehead + stem + rest glyphs.
    painter.setBrush(QColor(220, 220, 220));
    for (size_t i = 0; i < n; ++i) {
        const auto& note = m_pattern.notes[i];
        double cx = centers[i];

        if (note.isRest) {
            painter.setPen(QColor(180, 180, 180));
            int rw = 10;
            painter.drawRect(static_cast<int>(cx) - rw / 2, middleLineY - 6, rw, 12);
            continue;
        }

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(220, 220, 220));
        painter.drawEllipse(QPointF(cx, middleLineY), 6.0, 5.0);

        if (note.beats < 1.0 - 1e-9 || groupId[i] >= 0) {
            painter.setPen(QPen(QColor(220, 220, 220), 2));
            painter.drawLine(QPointF(cx + 6, middleLineY), QPointF(cx + 6, stemTopY));

            // Lone eighth/sixteenth note (not part of a multi-note beam
            // group): draw flags instead of a beam.
            if (groupId[i] < 0 && note.beats <= 0.5 + 1e-9) {
                int flags = note.beats <= 0.25 + 1e-9 ? 2 : 1;
                for (int f = 0; f < flags; ++f) {
                    int fy = stemTopY + f * 6;
                    painter.drawLine(QPointF(cx + 6, fy), QPointF(cx + 16, fy + 8));
                }
            }
        } else if (note.beats >= 1.0 - 1e-9) {
            // Quarter (or longer) note: plain stem, no beam/flag.
            painter.setPen(QPen(QColor(220, 220, 220), 2));
            painter.drawLine(QPointF(cx + 6, middleLineY), QPointF(cx + 6, stemTopY));
        }
    }

    // Beams: one primary line across each group's stem tops, plus a
    // secondary line only over sub-runs of sixteenth notes within it.
    painter.setPen(QPen(QColor(220, 220, 220), 3));
    size_t i = 0;
    while (i < n) {
        if (groupId[i] < 0) {
            ++i;
            continue;
        }
        int g = groupId[i];
        size_t start = i;
        while (i < n && groupId[i] == g) ++i;
        size_t end = i; // [start, end)

        double x0 = centers[start] + 6;
        double x1 = centers[end - 1] + 6;
        painter.drawLine(QPointF(x0, stemTopY), QPointF(x1, stemTopY));

        // Secondary beam over consecutive sixteenth-duration notes.
        size_t j = start;
        while (j < end) {
            if (m_pattern.notes[j].beats <= 0.25 + 1e-9) {
                size_t runStart = j;
                while (j < end && m_pattern.notes[j].beats <= 0.25 + 1e-9) ++j;
                size_t runEnd = j;
                if (runEnd - runStart >= 1) {
                    double sx0 = centers[runStart] + 6;
                    double sx1 = centers[runEnd - 1] + 6;
                    painter.drawLine(QPointF(sx0, stemTopY + 6), QPointF(sx1, stemTopY + 6));
                }
            } else {
                ++j;
            }
        }
    }
}

} // namespace rsd
