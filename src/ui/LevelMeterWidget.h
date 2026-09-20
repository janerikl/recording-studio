#pragma once

#include <QWidget>

namespace rsd {

// A stereo peak meter: two stacked horizontal bars (L on top, R below),
// each with a decaying falloff (smoother than raw instantaneous values
// polled at ~30fps).
class LevelMeterWidget : public QWidget {
    Q_OBJECT

public:
    // Horizontal: the original layout (stacked L/R bars with text labels),
    // used in the transport toolbar's global input/output meters.
    // Vertical: two thin side-by-side bars filling bottom-up with no text,
    // sized to sit in a narrow mixer strip.
    enum class Orientation { Horizontal, Vertical };

    explicit LevelMeterWidget(const QString& label, QWidget* parent = nullptr);
    explicit LevelMeterWidget(Orientation orientation, QWidget* parent = nullptr);

    void setLevels(float left0to1, float right0to1); // call periodically from a timer

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_label;
    Orientation m_orientation = Orientation::Horizontal;
    float m_displayLeft = 0.0f;
    float m_displayRight = 0.0f;

    void paintHorizontal(class QPainter& painter);
    void paintVertical(class QPainter& painter);
};

} // namespace rsd
