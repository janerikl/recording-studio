#pragma once

#include <QWidget>

namespace rsd {

// A horizontal peak meter with a decaying falloff (smoother than raw
// instantaneous values polled at ~30fps).
class LevelMeterWidget : public QWidget {
    Q_OBJECT

public:
    explicit LevelMeterWidget(const QString& label, QWidget* parent = nullptr);

    void setLevel(float level0to1); // call periodically from a timer

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_label;
    float m_displayLevel = 0.0f;
};

} // namespace rsd
