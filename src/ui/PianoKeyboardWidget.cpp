#include "PianoKeyboardWidget.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include "PianoKeyMap.h"
#include "audio/NoteNaming.h"

namespace rsd {

namespace {
bool isWhiteKey(int pitch) {
    static const bool kWhite[12] = {true,  false, true,  false, true, true,
                                     false, true,  false, true,  false, true};
    int pc = ((pitch % 12) + 12) % 12;
    return kWhite[pc];
}

int whiteKeyCount(int lowPitch, int highPitch) {
    int n = 0;
    for (int p = lowPitch; p <= highPitch; ++p) {
        if (isWhiteKey(p)) ++n;
    }
    return n;
}
} // namespace

PianoKeyboardWidget::PianoKeyboardWidget(QWidget* parent) : QWidget(parent) {
    setFixedHeight(90);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

int PianoKeyboardWidget::pitchAtX(int x) const {
    int whites = whiteKeyCount(kLowPitch, kHighPitch);
    if (whites <= 0 || width() <= 0) return kLowPitch;
    float keyW = static_cast<float>(width()) / static_cast<float>(whites);

    // Black keys sit on top visually and take priority in the upper part of
    // the widget; each one is centered on the boundary right after the
    // white key immediately below it.
    int whiteIndex = 0;
    for (int p = kLowPitch; p <= kHighPitch; ++p) {
        if (isWhiteKey(p)) {
            ++whiteIndex;
            continue;
        }
        float centerX = static_cast<float>(whiteIndex) * keyW;
        float halfW = keyW * 0.3f;
        if (x >= centerX - halfW && x < centerX + halfW) return p;
    }

    whiteIndex = 0;
    for (int p = kLowPitch; p <= kHighPitch; ++p) {
        if (!isWhiteKey(p)) continue;
        float left = static_cast<float>(whiteIndex) * keyW;
        if (x >= left && x < left + keyW) return p;
        ++whiteIndex;
    }
    return kLowPitch;
}

void PianoKeyboardWidget::setHeldPitch(std::optional<int> pitch) {
    if (m_heldPitch == pitch) return;
    if (m_heldPitch) emit noteOff(*m_heldPitch);
    m_heldPitch = pitch;
    if (m_heldPitch) emit noteOn(*m_heldPitch, 0.9f);
    update();
}

void PianoKeyboardWidget::mousePressEvent(QMouseEvent* event) {
    setHeldPitch(pitchAtX(event->pos().x()));
}

void PianoKeyboardWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!m_heldPitch) return; // only glide while a key is held down
    setHeldPitch(pitchAtX(event->pos().x()));
}

void PianoKeyboardWidget::mouseReleaseEvent(QMouseEvent*) { setHeldPitch(std::nullopt); }

void PianoKeyboardWidget::leaveEvent(QEvent*) { setHeldPitch(std::nullopt); }

void PianoKeyboardWidget::setExpectedPitch(std::optional<int> pitch) {
    if (m_expectedPitch == pitch) return;
    m_expectedPitch = pitch;
    update();
}

void PianoKeyboardWidget::keyPressEvent(QKeyEvent* event) {
    if (event->isAutoRepeat()) return;
    auto pitch = pitchForComputerKey(event->key());
    if (!pitch || m_keyboardHeldPitches.count(*pitch)) {
        QWidget::keyPressEvent(event);
        return;
    }
    m_keyboardHeldPitches.insert(*pitch);
    emit noteOn(*pitch, 0.9f);
    update();
}

void PianoKeyboardWidget::keyReleaseEvent(QKeyEvent* event) {
    if (event->isAutoRepeat()) return;
    auto pitch = pitchForComputerKey(event->key());
    if (!pitch || !m_keyboardHeldPitches.count(*pitch)) {
        QWidget::keyReleaseEvent(event);
        return;
    }
    m_keyboardHeldPitches.erase(*pitch);
    emit noteOff(*pitch);
    update();
}

void PianoKeyboardWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(20, 20, 20));

    int whites = whiteKeyCount(kLowPitch, kHighPitch);
    if (whites <= 0) return;
    float keyW = static_cast<float>(width()) / static_cast<float>(whites);

    int whiteIndex = 0;
    for (int p = kLowPitch; p <= kHighPitch; ++p) {
        if (!isWhiteKey(p)) continue;
        float left = static_cast<float>(whiteIndex) * keyW;
        bool held = (m_heldPitch && *m_heldPitch == p) || m_keyboardHeldPitches.count(p);
        bool expected = m_expectedPitch && *m_expectedPitch == p;
        painter.setPen(QColor(60, 60, 60));
        painter.setBrush(held         ? QColor(160, 210, 255)
                          : expected  ? QColor(150, 235, 160)
                                      : QColor(235, 235, 235));
        QRectF keyRect(left, 0, keyW, height());
        painter.drawRect(keyRect);

        painter.setPen(QColor(90, 90, 90));
        painter.drawText(keyRect.adjusted(0, 0, 0, -18), Qt::AlignBottom | Qt::AlignHCenter,
                          midiNoteName(p));
        QString label = computerKeyLabelForPitch(p);
        if (!label.isEmpty()) {
            painter.drawText(keyRect.adjusted(0, 0, 0, -6), Qt::AlignBottom | Qt::AlignHCenter, label);
        }
        ++whiteIndex;
    }

    whiteIndex = 0;
    float blackH = height() * 0.6f;
    float blackW = keyW * 0.6f;
    for (int p = kLowPitch; p <= kHighPitch; ++p) {
        if (isWhiteKey(p)) {
            ++whiteIndex;
            continue;
        }
        float centerX = static_cast<float>(whiteIndex) * keyW;
        bool held = (m_heldPitch && *m_heldPitch == p) || m_keyboardHeldPitches.count(p);
        bool expected = m_expectedPitch && *m_expectedPitch == p;
        painter.setPen(Qt::NoPen);
        painter.setBrush(held        ? QColor(90, 150, 220)
                          : expected ? QColor(70, 160, 90)
                                     : QColor(25, 25, 25));
        QRectF keyRect(centerX - blackW / 2.0f, 0, blackW, blackH);
        painter.drawRect(keyRect);

        painter.setPen(QColor(200, 200, 200));
        QString label = computerKeyLabelForPitch(p);
        if (!label.isEmpty()) {
            painter.drawText(keyRect.adjusted(0, 0, 0, -14), Qt::AlignBottom | Qt::AlignHCenter, label);
        }
        painter.drawText(keyRect.adjusted(0, 0, 0, -2), Qt::AlignBottom | Qt::AlignHCenter, midiNoteName(p));
    }
}

} // namespace rsd
