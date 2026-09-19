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

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void rebuildCache();

    std::shared_ptr<AudioBuffer> m_buffer;
    QVector<WaveformCache::PeakPair> m_peaks;
};

} // namespace rsd
