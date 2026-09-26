#include "ui/PlaybackTimeDisplay.h"

#include <QEvent>
#include <QFont>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QStackedLayout>

#include "ui/TimeDisplayMath.h"

namespace rsd {

namespace {
const char* kClockStyle =
    "QLabel, QLineEdit { background-color: #1a1a1a; color: #33cc55; border: 1px solid #444; "
    "border-radius: 3px; padding: 2px 6px; }";
} // namespace

PlaybackTimeDisplay::PlaybackTimeDisplay(QWidget* parent) : QWidget(parent) {
    QFont clockFont("Monospace");
    clockFont.setStyleHint(QFont::Monospace);
    clockFont.setPointSize(14);
    clockFont.setBold(true);

    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setFont(clockFont);
    m_label->setStyleSheet(kClockStyle);
    m_label->setToolTip("Click to toggle timecode / sample count, double-click to jump to a time");
    m_label->setCursor(Qt::PointingHandCursor);
    m_label->installEventFilter(this);

    m_editor = new QLineEdit(this);
    m_editor->setAlignment(Qt::AlignCenter);
    m_editor->setFont(clockFont);
    m_editor->setStyleSheet(kClockStyle);
    m_editor->installEventFilter(this);
    connect(m_editor, &QLineEdit::returnPressed, this, &PlaybackTimeDisplay::commitEdit);

    m_stack = new QStackedLayout(this);
    m_stack->setContentsMargins(0, 0, 0, 0);
    m_stack->addWidget(m_label);
    m_stack->addWidget(m_editor);
    m_stack->setCurrentWidget(m_label);

    refreshLabel();
}

void PlaybackTimeDisplay::setPositionSamples(int64_t samples) {
    m_positionSamples = samples;
    if (m_stack->currentWidget() == m_label) refreshLabel();
}

void PlaybackTimeDisplay::setSampleRate(unsigned sampleRate) {
    m_sampleRate = sampleRate;
    refreshLabel();
}

void PlaybackTimeDisplay::refreshLabel() {
    m_label->setText(m_showSamples ? formatSampleCount(m_positionSamples)
                                    : formatTimecode(m_positionSamples, m_sampleRate));
}

void PlaybackTimeDisplay::beginEdit() {
    m_editor->setText(m_label->text());
    m_editor->selectAll();
    m_stack->setCurrentWidget(m_editor);
    m_editor->setFocus();
}

void PlaybackTimeDisplay::commitEdit() {
    const QString text = m_editor->text();
    const auto samples = m_showSamples ? parseSampleCount(text) : parseTimecode(text, m_sampleRate);
    m_stack->setCurrentWidget(m_label);
    if (samples.has_value()) {
        emit seekRequested(*samples);
    }
    // If invalid, the label already still shows the pre-edit position —
    // nothing to revert.
}

void PlaybackTimeDisplay::cancelEdit() {
    m_stack->setCurrentWidget(m_label);
}

bool PlaybackTimeDisplay::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_label) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_showSamples = !m_showSamples;
                refreshLabel();
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                // Undo the format toggle the preceding single-click already
                // applied (Qt delivers press+release+press+dblclick for a
                // double-click, so the toggle above already fired once).
                m_showSamples = !m_showSamples;
                beginEdit();
                return true;
            }
        }
    } else if (watched == m_editor) {
        if (event->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                cancelEdit();
                return true;
            }
        } else if (event->type() == QEvent::FocusOut) {
            cancelEdit();
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace rsd
