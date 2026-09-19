#include "LevelMeterWidget.h"

#include <QPainter>
#include <algorithm>

namespace rsd {

LevelMeterWidget::LevelMeterWidget(const QString& label, QWidget* parent)
    : QWidget(parent), m_label(label) {
    setFixedHeight(20);
    setMinimumWidth(120);
}

void LevelMeterWidget::setLevel(float level0to1) {
    level0to1 = std::clamp(level0to1, 0.0f, 1.0f);
    if (level0to1 > m_displayLevel) {
        m_displayLevel = level0to1; // snap up instantly
    } else {
        m_displayLevel *= 0.85f; // decay smoothly
    }
    update();
}

void LevelMeterWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(20, 20, 20));

    int labelWidth = 50;
    painter.setPen(QColor(180, 180, 180));
    painter.drawText(QRect(0, 0, labelWidth, height()), Qt::AlignVCenter | Qt::AlignLeft, m_label);

    int barX = labelWidth;
    int barWidth = std::max(0, width() - barX - 4);
    int filled = static_cast<int>(m_displayLevel * barWidth);

    painter.fillRect(barX, 3, barWidth, height() - 6, QColor(40, 40, 40));

    if (filled > 0) {
        QColor color = m_displayLevel > 0.9f ? QColor(220, 60, 60)
                        : m_displayLevel > 0.7f ? QColor(230, 190, 60)
                                                 : QColor(80, 180, 100);
        painter.fillRect(barX, 3, filled, height() - 6, color);
    }
}

} // namespace rsd
