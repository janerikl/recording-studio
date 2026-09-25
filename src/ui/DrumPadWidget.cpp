#include "DrumPadWidget.h"

#include <QGridLayout>
#include <QPushButton>

#include "audio/GMDrumMap.h"

namespace rsd {

DrumPadWidget::DrumPadWidget(QWidget* parent) : QWidget(parent) {
    setStyleSheet(
        "QPushButton { background: #2a2a2a; color: #e0e0e0; border: 1px solid #4a4a4a; "
        "min-height: 48px; }"
        "QPushButton:pressed { background: #5a8fd6; }");

    auto* layout = new QGridLayout(this);
    const auto& pads = gmDrumPads();
    int columns = 5;
    for (size_t i = 0; i < pads.size(); ++i) {
        int pitch = pads[i].pitch;
        auto* button = new QPushButton(pads[i].label, this);
        connect(button, &QPushButton::pressed, this, [this, pitch]() { emit noteOn(pitch, 1.0f); });
        connect(button, &QPushButton::released, this, [this, pitch]() { emit noteOff(pitch); });
        layout->addWidget(button, static_cast<int>(i) / columns, static_cast<int>(i) % columns);
    }
}

} // namespace rsd
