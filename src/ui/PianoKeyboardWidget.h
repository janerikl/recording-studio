#pragma once

#include <QWidget>
#include <optional>

namespace rsd {

// Clickable on-screen piano keyboard (v1 note input: no hardware MIDI).
// Two octaves, C3-C5 (MIDI 48-72). Click-drag across keys glides the
// currently-held note off and the newly-entered key on, like a real
// keyboard glissando; releasing anywhere ends the note.
class PianoKeyboardWidget : public QWidget {
    Q_OBJECT

public:
    explicit PianoKeyboardWidget(QWidget* parent = nullptr);

    static constexpr int kLowPitch = 48;  // C3
    static constexpr int kHighPitch = 72; // C5

signals:
    void noteOn(int pitch, float velocity);
    void noteOff(int pitch);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    int pitchAtX(int x) const;
    void setHeldPitch(std::optional<int> pitch);

    std::optional<int> m_heldPitch;
};

} // namespace rsd
