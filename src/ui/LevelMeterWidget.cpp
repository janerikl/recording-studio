#include "LevelMeterWidget.h"

#include <QPainter>
#include <algorithm>

namespace rsd {

LevelMeterWidget::LevelMeterWidget(const QString& label, QWidget* parent)
    : QWidget(parent), m_label(label), m_orientation(Orientation::Horizontal) {
    setFixedHeight(34);
    setMinimumWidth(140);
}

LevelMeterWidget::LevelMeterWidget(Orientation orientation, QWidget* parent)
    : QWidget(parent), m_orientation(orientation) {
    if (m_orientation == Orientation::Vertical) {
        setFixedWidth(18);
        setMinimumHeight(60);
    } else {
        setFixedHeight(34);
        setMinimumWidth(140);
    }
}

static float decay(float current, float incoming) {
    incoming = std::clamp(incoming, 0.0f, 1.0f);
    return incoming > current ? incoming : current * 0.85f;
}

void LevelMeterWidget::setLevels(float left0to1, float right0to1) {
    m_displayLeft = decay(m_displayLeft, left0to1);
    m_displayRight = decay(m_displayRight, right0to1);
    update();
}

static QColor levelColor(float level) {
    if (level > 0.9f) return QColor(220, 60, 60);
    if (level > 0.7f) return QColor(230, 190, 60);
    return QColor(80, 180, 100);
}

void LevelMeterWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(20, 20, 20));

    if (m_orientation == Orientation::Vertical) {
        paintVertical(painter);
    } else {
        paintHorizontal(painter);
    }
}

void LevelMeterWidget::paintHorizontal(QPainter& painter) {
    int labelWidth = 50;
    painter.setPen(QColor(180, 180, 180));
    painter.drawText(QRect(0, 0, labelWidth, height()), Qt::AlignVCenter | Qt::AlignLeft, m_label);

    int barX = labelWidth;
    int barWidth = std::max(0, width() - barX - 4);

    auto drawBar = [&](int y, int h, float level, const char* chLabel) {
        painter.fillRect(barX, y, barWidth, h, QColor(40, 40, 40));
        int filled = static_cast<int>(level * barWidth);
        if (filled > 0) painter.fillRect(barX, y, filled, h, levelColor(level));
        painter.setPen(QColor(140, 140, 140));
        painter.drawText(QRect(barX + 2, y, 16, h), Qt::AlignVCenter | Qt::AlignLeft, chLabel);
    };

    int barHeight = (height() - 6) / 2;
    drawBar(2, barHeight, m_displayLeft, "L");
    drawBar(4 + barHeight, barHeight, m_displayRight, "R");
}

void LevelMeterWidget::paintVertical(QPainter& painter) {
    int gap = 2;
    int barWidth = std::max(1, (width() - gap) / 2);

    auto drawBar = [&](int x, float level) {
        painter.fillRect(x, 0, barWidth, height(), QColor(40, 40, 40));
        int filled = static_cast<int>(level * height());
        if (filled > 0) {
            painter.fillRect(x, height() - filled, barWidth, filled, levelColor(level));
        }
    };

    drawBar(0, m_displayLeft);
    drawBar(barWidth + gap, m_displayRight);
}

} // namespace rsd
