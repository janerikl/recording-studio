#include "PianoRollGridWidget.h"

#include <QColor>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>
#include <vector>

#include "PianoRollEditMath.h"
#include "command/EditCommands.h"

namespace rsd {

PianoRollGridWidget::PianoRollGridWidget(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setStyleSheet("background: #1a1a1a;");
    updateContentSize();
}

void PianoRollGridWidget::setTrack(std::shared_ptr<Track> track) {
    m_track = std::move(track);
    m_selectedNoteId = QUuid();
    m_dragMode = DragMode::None;
    update();
}

void PianoRollGridWidget::setBpm(double bpm) {
    m_bpm = bpm;
    updateContentSize();
    update();
}

void PianoRollGridWidget::setSampleRate(int sampleRate) {
    m_sampleRate = sampleRate;
    updateContentSize();
    update();
}

void PianoRollGridWidget::setSnapDenominator(int denominator) {
    m_snapDenominator = denominator;
}

int64_t PianoRollGridWidget::snapIntervalSamples() const {
    return rsd::snapIntervalSamples(samplesPerBeat(m_bpm, m_sampleRate), m_snapDenominator);
}

int PianoRollGridWidget::gridHeight() const {
    return (kHighPitch - kLowPitch + 1) * kRowHeightPx;
}

void PianoRollGridWidget::updateContentSize() {
    int64_t beatPx = kPixelsPerBeat;
    setMinimumSize(static_cast<int>(beatPx * kVisibleBeats), gridHeight() + kVelocityLaneHeightPx);
}

int64_t PianoRollGridWidget::xToSample(int x) const {
    int64_t beat = samplesPerBeat(m_bpm, m_sampleRate);
    if (beat <= 0) return 0;
    return static_cast<int64_t>(static_cast<double>(x) / kPixelsPerBeat * static_cast<double>(beat));
}

int PianoRollGridWidget::sampleToX(int64_t sample) const {
    int64_t beat = samplesPerBeat(m_bpm, m_sampleRate);
    if (beat <= 0) return 0;
    return static_cast<int>(static_cast<double>(sample) / static_cast<double>(beat) * kPixelsPerBeat);
}

std::shared_ptr<MidiNote> PianoRollGridWidget::findNoteAt(int x, int y) const {
    if (!m_track) return nullptr;
    auto notes = m_track->midiClipsSnapshot();
    for (auto& note : *notes) {
        MidiNoteEditRect r{sampleToX(note->startSample),
                            sampleToX(note->startSample + note->lengthSamples) - sampleToX(note->startSample),
                            pitchToRowY(note->pitch, gridHeight(), kLowPitch, kHighPitch, kRowHeightPx),
                            kRowHeightPx};
        if (rectContainsPoint(r, x, y)) return note;
    }
    return nullptr;
}

void PianoRollGridWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#1a1a1a"));

    int gh = gridHeight();

    // Row backgrounds: alternate shading per octave for readability.
    for (int pitch = kLowPitch; pitch <= kHighPitch; ++pitch) {
        int y = pitchToRowY(pitch, gh, kLowPitch, kHighPitch, kRowHeightPx);
        static const std::vector<int> kBlackKeyOffsets{1, 3, 6, 8, 10};
        bool isBlackKey = std::find(kBlackKeyOffsets.begin(), kBlackKeyOffsets.end(), pitch % 12) !=
                           kBlackKeyOffsets.end();
        painter.fillRect(0, y, width(), kRowHeightPx, isBlackKey ? QColor("#161616") : QColor("#202020"));
    }

    // Beat grid lines.
    painter.setPen(QColor("#333333"));
    for (int b = 0; b <= kVisibleBeats; ++b) {
        int x = b * kPixelsPerBeat;
        painter.drawLine(x, 0, x, gh);
    }

    painter.setPen(QColor("#444444"));
    painter.drawLine(0, gh, width(), gh);

    if (!m_track) return;

    auto notes = m_track->midiClipsSnapshot();
    for (auto& note : *notes) {
        int x0 = sampleToX(note->startSample);
        int x1 = sampleToX(note->startSample + note->lengthSamples);
        int y = pitchToRowY(note->pitch, gh, kLowPitch, kHighPitch, kRowHeightPx);
        bool selected = note->id == m_selectedNoteId;
        painter.fillRect(x0, y, std::max(2, x1 - x0), kRowHeightPx - 1,
                          selected ? QColor("#ffcc66") : QColor("#66aaff"));

        // Velocity bar in the lane below the grid.
        int vh = static_cast<int>(clampVelocity(note->velocity) * kVelocityLaneHeightPx);
        painter.fillRect(x0, gh + kVelocityLaneHeightPx - vh, std::max(2, x1 - x0), vh,
                          selected ? QColor("#ffcc66") : QColor("#4488cc"));
    }
}

void PianoRollGridWidget::mousePressEvent(QMouseEvent* event) {
    if (!m_track) return;
    int x = event->pos().x();
    int y = event->pos().y();
    int gh = gridHeight();

    if (y >= gh) {
        // Velocity lane: adjust the note whose time range contains this x.
        auto notes = m_track->midiClipsSnapshot();
        for (auto& note : *notes) {
            int x0 = sampleToX(note->startSample);
            int x1 = sampleToX(note->startSample + note->lengthSamples);
            if (x >= x0 && x < x1) {
                m_dragNoteId = note->id;
                m_dragMode = DragMode::Velocity;
                m_editBeforeSnapshot = m_track->midiClipsSnapshot();
                m_selectedNoteId = note->id;
                break;
            }
        }
        update();
        return;
    }

    auto hit = findNoteAt(x, y);
    if (hit) {
        m_selectedNoteId = hit->id;
        m_dragNoteId = hit->id;
        m_dragStartX = x;
        m_dragStartY = y;
        m_dragOrigStart = hit->startSample;
        m_dragOrigPitch = hit->pitch;
        m_dragOrigLength = hit->lengthSamples;

        MidiNoteEditRect r{sampleToX(hit->startSample),
                            sampleToX(hit->startSample + hit->lengthSamples) - sampleToX(hit->startSample),
                            pitchToRowY(hit->pitch, gh, kLowPitch, kHighPitch, kRowHeightPx), kRowHeightPx};
        m_dragMode = isInResizeZone(r, x, kResizeMarginPx) ? DragMode::Resize : DragMode::Move;
        m_editBeforeSnapshot = m_track->midiClipsSnapshot();
        update();
        return;
    }

    // Empty cell: draw a new note, snapped, default length = one snap
    // interval (or one beat if snapping is off), default velocity 0.8.
    auto before = m_track->midiClipsSnapshot();
    int64_t interval = snapIntervalSamples();
    int64_t startSample = snapToGrid(xToSample(x), interval);
    int64_t length = interval > 0 ? interval : samplesPerBeat(m_bpm, m_sampleRate);
    int pitch = yToPitch(y, gh, kLowPitch, kHighPitch, kRowHeightPx);

    auto note = std::make_shared<MidiNote>();
    note->pitch = pitch;
    note->velocity = 0.8f;
    note->startSample = startSample;
    note->lengthSamples = length;
    m_selectedNoteId = note->id;

    m_track->addMidiNote(note);
    commitEdit(before, "Add note");
    update();
}

void PianoRollGridWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragMode == DragMode::None || !m_track) return;
    int x = event->pos().x();
    int y = event->pos().y();
    int gh = gridHeight();

    auto notes = m_track->midiClipsSnapshot();
    std::shared_ptr<MidiNote> original;
    for (auto& n : *notes) {
        if (n->id == m_dragNoteId) { original = n; break; }
    }
    if (!original) return;

    auto edited = std::make_shared<MidiNote>(*original);
    int64_t interval = snapIntervalSamples();

    if (m_dragMode == DragMode::Move) {
        int64_t deltaSamples = xToSample(x) - xToSample(m_dragStartX);
        int64_t rawStart = applyMoveDeltaSamples(m_dragOrigStart, deltaSamples, 0);
        edited->startSample = snapToGrid(rawStart, interval);

        int pitchAtStart = yToPitch(m_dragStartY, gh, kLowPitch, kHighPitch, kRowHeightPx);
        int pitchAtCurrent = yToPitch(y, gh, kLowPitch, kHighPitch, kRowHeightPx);
        int deltaPitch = pitchAtCurrent - pitchAtStart;
        edited->pitch = applyMovePitchDelta(m_dragOrigPitch, deltaPitch, kLowPitch, kHighPitch);
    } else if (m_dragMode == DragMode::Resize) {
        int64_t requested = xToSample(x) - original->startSample;
        int64_t snapped = interval > 0 ? std::max<int64_t>(interval, snapToGrid(requested, interval)) : requested;
        edited->lengthSamples = resizeLengthSamples(snapped, /*minLength=*/1);
    } else if (m_dragMode == DragMode::Velocity) {
        edited->velocity = velocityFromLaneY(y - gh, kVelocityLaneHeightPx);
    }

    m_track->replaceMidiNote(m_dragNoteId, edited);
    update();
}

void PianoRollGridWidget::mouseReleaseEvent(QMouseEvent*) {
    if (m_dragMode == DragMode::None) return;
    m_dragMode = DragMode::None;
    commitEdit(m_editBeforeSnapshot, "Edit note");
    m_editBeforeSnapshot.reset();
}

void PianoRollGridWidget::keyPressEvent(QKeyEvent* event) {
    if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) && m_track &&
        !m_selectedNoteId.isNull()) {
        auto before = m_track->midiClipsSnapshot();
        m_track->removeMidiNote(m_selectedNoteId);
        m_selectedNoteId = QUuid();
        commitEdit(before, "Delete note");
        update();
        return;
    }
    QWidget::keyPressEvent(event);
}

void PianoRollGridWidget::commitEdit(std::shared_ptr<const Track::MidiNoteList> before, const QString& text) {
    auto after = m_track->midiClipsSnapshot();
    if (m_commandStack && before && after != before) {
        m_commandStack->push(std::make_unique<TrackMidiCommand>(m_track, before, after, text));
    }
}

} // namespace rsd
