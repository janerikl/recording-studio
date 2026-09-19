#pragma once

#include <QWidget>
#include <memory>

#include "model/AudioBuffer.h"
#include "waveform/WaveformCache.h"

namespace rsd {

// Displays a min/max peak waveform for a single (already mixed-down) buffer.
// Recomputes the peak cache when the buffer or widget width changes.
class WaveformWidget : public QWidget {
    Q_OBJECT

public:
    explicit WaveformWidget(QWidget* parent = nullptr);

    void setBuffer(std::shared_ptr<AudioBuffer> buffer);

    // When set (>0), the buffer is drawn at the same sample<->pixel scale as
    // the rest of the timeline (track lanes / ruler) instead of auto-fitting
    // its own duration to the full widget width — otherwise a buffer shorter
    // than the shared timeline stretches to fill the widget, visually
    // misrepresenting where its audio actually sits relative to the tracks.
    void setTimelineLength(int64_t samples);
    void setPlayheadSample(int64_t sample);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void rebuildCache();

    std::shared_ptr<AudioBuffer> m_buffer;
    QVector<WaveformCache::PeakPair> m_peaks;
    int64_t m_timelineLength = 0;
    int m_audioColumns = 0; // how many of the widget's columns the buffer actually occupies
    int64_t m_playheadSample = -1;
};

} // namespace rsd
