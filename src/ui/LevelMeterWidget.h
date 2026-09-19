#pragma once

#include <QWidget>

namespace rsd {

// A stereo peak meter: two stacked horizontal bars (L on top, R below),
// each with a decaying falloff (smoother than raw instantaneous values
// polled at ~30fps).
class LevelMeterWidget : public QWidget {
    Q_OBJECT

public:
    explicit LevelMeterWidget(const QString& label, QWidget* parent = nullptr);

    void setLevels(float left0to1, float right0to1); // call periodically from a timer

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_label;
    float m_displayLeft = 0.0f;
    float m_displayRight = 0.0f;
};

} // namespace rsd
