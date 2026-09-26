#pragma once

#include <cstdint>

#include <QWidget>

class QLabel;
class QLineEdit;
class QStackedLayout;

namespace rsd {

// Always-visible digital playback position readout for the top bar — not a
// Qt::Popup (unlike EffectsPopoverWidget) since this needs to stay on
// screen through the whole playback, not close on an outside click.
// Single click toggles between Timecode and raw sample count; double-click
// turns it into an editable field to jump to a typed time (Pro
// Tools/Logic-style click-to-edit transport clock).
class PlaybackTimeDisplay : public QWidget {
    Q_OBJECT

public:
    explicit PlaybackTimeDisplay(QWidget* parent = nullptr);

    void setPositionSamples(int64_t samples);
    void setSampleRate(unsigned sampleRate);

signals:
    // Emitted only on a successfully parsed edit commit (Enter). Invalid
    // input or a cancelled edit (Escape/focus-out) never emits this.
    void seekRequested(int64_t samples);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void refreshLabel();
    void beginEdit();
    void commitEdit();
    void cancelEdit();

    QLabel* m_label = nullptr;
    QLineEdit* m_editor = nullptr;
    QStackedLayout* m_stack = nullptr;

    int64_t m_positionSamples = 0;
    unsigned m_sampleRate = 48000;
    bool m_showSamples = false;
};

} // namespace rsd
