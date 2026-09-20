#pragma once

#include <QWidget>
#include <cstdint>
#include <memory>

#include "model/Clip.h"

namespace rsd {

// Read-only display of one punch/loop recording take, stacked under a
// track's main clip lane when its take lanes are expanded. Click promotes
// this take to the active comp (see MainWindow's handling of
// TimelineView::takeSelected). Shares the main lane's timeline scale
// (pushed in via setScale) so waveforms line up visually, but does not
// scroll/zoom independently.
class TakeLaneWidget : public QWidget {
    Q_OBJECT

public:
    TakeLaneWidget(std::shared_ptr<Clip> take, int takeNumber, QWidget* parent = nullptr);

    void setActive(bool active);
    void setScale(int64_t visibleLengthSamples, int64_t scrollOffsetSamples);

    static constexpr int kHeight = 36;

signals:
    void takeClicked(std::shared_ptr<Clip> take);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    int sampleToX(int64_t sample) const;

    std::shared_ptr<Clip> m_take;
    int m_takeNumber;
    bool m_active = false;
    int64_t m_visibleLengthSamples = 0;
    int64_t m_scrollOffsetSamples = 0;
};

} // namespace rsd
