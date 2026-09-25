#pragma once

#include <QWidget>
#include <optional>
#include <set>

namespace rsd {

// Clickable on-screen piano keyboard (v1 note input: no hardware MIDI).
// Two octaves, C3-C5 (MIDI 48-72). Click-drag across keys glides the
// currently-held note off and the newly-entered key on, like a real
// keyboard glissando; releasing anywhere ends the note. Also playable via
// computer-keyboard shortcuts (see PianoKeyMap.h) — polyphonic, unlike
// the mouse's single-note glide, so chords can be held with multiple
// keys at once. Requires focus (click the widget first) to receive key
// events.
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
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

public:
    // Learn-to-play aid: highlights the given pitch (e.g. the next note in
    // a practice exercise) distinctly from a currently-held key.
    // std::nullopt clears the highlight.
    void setExpectedPitch(std::optional<int> pitch);

private:
    int pitchAtX(int x) const;
    void setHeldPitch(std::optional<int> pitch);

    std::optional<int> m_heldPitch;
    std::set<int> m_keyboardHeldPitches;
    std::optional<int> m_expectedPitch;
};

} // namespace rsd
