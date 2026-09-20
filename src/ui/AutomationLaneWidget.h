#pragma once

#include <QWidget>
#include <cstdint>
#include <memory>

#include "command/CommandStack.h"
#include "model/Track.h"

class QComboBox;

namespace rsd {

// Editable automation curve for one track/target, stacked under a track's
// main clip lane (same slot/toggle pattern as TakeLaneWidget). Click-to-add
// a breakpoint, drag to move it (time + value), Delete to remove the
// selected one. Shares the main lane's timeline scale (setScale) so the
// curve lines up visually with clips above it.
class AutomationLaneWidget : public QWidget {
    Q_OBJECT

public:
    explicit AutomationLaneWidget(std::shared_ptr<Track> track, QWidget* parent = nullptr);

    void setCommandStack(CommandStack* stack) { m_commandStack = stack; }
    void setScale(int64_t visibleLengthSamples, int64_t scrollOffsetSamples);

    static constexpr int kHeight = 48;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    int64_t xToSample(int x) const;
    int sampleToX(int64_t sample) const;
    float yToValue(int y) const;
    int valueToY(float value) const;
    std::shared_ptr<AutomationLane> currentLane() const; // nullptr if none exists yet for the target
    void commitEdit(std::shared_ptr<const Track::AutomationLaneList> before, const QString& text);

    AutomationTarget currentTarget() const;

    std::shared_ptr<Track> m_track;
    CommandStack* m_commandStack = nullptr;
    QComboBox* m_targetCombo = nullptr;

    int64_t m_visibleLengthSamples = 0;
    int64_t m_scrollOffsetSamples = 0;

    int m_selectedPointIndex = -1; // index within the current target's lane, -1 = none
    bool m_dragging = false;
    std::shared_ptr<const Track::AutomationLaneList> m_editBeforeSnapshot;

    static constexpr int kHitToleranceSamples = 2000;
    static constexpr int kComboHeight = 18;
};

} // namespace rsd
