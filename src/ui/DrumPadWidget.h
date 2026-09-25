#pragma once

#include <QWidget>

namespace rsd {

// Clickable grid of GM drum pads (see GMDrumMap.h) — the drum-kit
// counterpart to PianoKeyboardWidget. Press/release a pad to trigger the
// corresponding percussion note on/off.
class DrumPadWidget : public QWidget {
    Q_OBJECT

public:
    explicit DrumPadWidget(QWidget* parent = nullptr);

signals:
    void noteOn(int pitch, float velocity);
    void noteOff(int pitch);
};

} // namespace rsd
