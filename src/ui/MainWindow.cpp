#include "MainWindow.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QShortcut>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>

#include "io/AudioFileIO.h"
#include "io/SessionIO.h"

namespace rsd {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      m_engine(std::make_unique<AudioEngine>()),
      m_session(std::make_unique<Session>()) {
    setWindowTitle("Recording Studio");
    resize(900, 600);

    m_session->sampleRate = static_cast<int>(m_engine->sampleRate());
    m_session->channels = static_cast<int>(m_engine->channels());
    m_engine->setSession(m_session.get());

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);

    auto* buttonRow = new QWidget(central);
    auto* buttonLayout = new QHBoxLayout(buttonRow);

    m_recordButton = new QPushButton("Record", buttonRow);
    m_playButton = new QPushButton("Play", buttonRow);
    m_playFromStartButton = new QPushButton("Play from Start", buttonRow);
    m_stopButton = new QPushButton("Stop", buttonRow);
    auto* importButton = new QPushButton("Import...", buttonRow);
    auto* exportButton = new QPushButton("Export...", buttonRow);
    auto* addTrackButton = new QPushButton("Add Track", buttonRow);
    auto* removeTrackButton = new QPushButton("Remove Track", buttonRow);
    m_deleteClipButton = new QPushButton("Delete Selected Clip", buttonRow);
    auto* saveSessionButton = new QPushButton("Save Session...", buttonRow);
    auto* loadSessionButton = new QPushButton("Load Session...", buttonRow);
    m_stopButton->setEnabled(false);
    m_deleteClipButton->setEnabled(false);

    connect(m_recordButton, &QPushButton::clicked, this, &MainWindow::onRecordClicked);
    connect(m_playButton, &QPushButton::clicked, this, &MainWindow::onPlayClicked);
    connect(m_playFromStartButton, &QPushButton::clicked, this,
            &MainWindow::onPlayFromStartClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(importButton, &QPushButton::clicked, this, &MainWindow::onImportClicked);
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::onExportClicked);
    connect(addTrackButton, &QPushButton::clicked, this, &MainWindow::onAddTrackClicked);
    connect(removeTrackButton, &QPushButton::clicked, this, &MainWindow::onRemoveTrackClicked);
    connect(m_deleteClipButton, &QPushButton::clicked, this, &MainWindow::onDeleteClipClicked);
    connect(saveSessionButton, &QPushButton::clicked, this, &MainWindow::onSaveSessionClicked);
    connect(loadSessionButton, &QPushButton::clicked, this, &MainWindow::onLoadSessionClicked);

    buttonLayout->addWidget(m_recordButton);
    buttonLayout->addWidget(m_playButton);
    buttonLayout->addWidget(m_playFromStartButton);
    buttonLayout->addWidget(m_stopButton);
    buttonLayout->addWidget(importButton);
    buttonLayout->addWidget(exportButton);
    buttonLayout->addWidget(addTrackButton);
    buttonLayout->addWidget(removeTrackButton);
    buttonLayout->addWidget(m_deleteClipButton);
    buttonLayout->addWidget(saveSessionButton);
    buttonLayout->addWidget(loadSessionButton);
    layout->addWidget(buttonRow);

    m_statusLabel = new QLabel("Stopped — 0 tracks, 0 clips", central);
    layout->addWidget(m_statusLabel);

    m_ruler = new TimeRulerWidget(central);
    m_ruler->setSampleRate(m_session->sampleRate);
    connect(m_ruler, &TimeRulerWidget::seekRequested, this, &MainWindow::onSeekRequested);
    layout->addWidget(m_ruler);

    m_timeline = new TimelineView(central);
    connect(m_timeline, &TimelineView::trackSelected, this, &MainWindow::onTrackSelected);
    connect(m_timeline, &TimelineView::clipSelectionChanged, this,
            &MainWindow::onClipSelectionChanged);
    connect(m_timeline, &TimelineView::seekRequested, this, &MainWindow::onSeekRequested);
    layout->addWidget(m_timeline, 1);

    setCentralWidget(central);

    m_ringDrainTimer = new QTimer(this);
    m_ringDrainTimer->setInterval(30);
    connect(m_ringDrainTimer, &QTimer::timeout, this, &MainWindow::drainCaptureRing);

    m_playheadTimer = new QTimer(this);
    m_playheadTimer->setInterval(33); // ~30fps
    connect(m_playheadTimer, &QTimer::timeout, this, &MainWindow::updatePlayhead);

    if (!m_engine->start()) {
        m_statusLabel->setText("Failed to start audio engine — check console");
        m_recordButton->setEnabled(false);
        m_playButton->setEnabled(false);
        m_playFromStartButton->setEnabled(false);
    }

    onAddTrackClicked(); // start with one track
    refreshTimelineScale();

    // Space toggles play/stop; Delete/Backspace removes the selected clip;
    // R starts recording. Standard transport/editor conventions.
    auto* spaceShortcut = new QShortcut(QKeySequence(Qt::Key_Space), this);
    connect(spaceShortcut, &QShortcut::activated, this, [this]() {
        if (m_engine->transport().state() == TransportState::Stopped) {
            onPlayClicked();
        } else {
            onStopClicked();
        }
    });

    auto* deleteShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), this);
    connect(deleteShortcut, &QShortcut::activated, this, &MainWindow::onDeleteClipClicked);
    auto* backspaceShortcut = new QShortcut(QKeySequence(Qt::Key_Backspace), this);
    connect(backspaceShortcut, &QShortcut::activated, this, &MainWindow::onDeleteClipClicked);

    auto* recordShortcut = new QShortcut(QKeySequence(Qt::Key_R), this);
    connect(recordShortcut, &QShortcut::activated, this, [this]() {
        if (m_engine->transport().state() == TransportState::Stopped) onRecordClicked();
    });
}

void MainWindow::onAddTrackClicked() {
    ++m_trackCounter;
    auto track = m_session->addTrack(QString("Track %1").arg(m_trackCounter));
    m_timeline->addTrack(track);
    if (!m_activeTrack) m_activeTrack = track;
    updateStatusLabel();
    refreshTimelineScale();
}

void MainWindow::onRemoveTrackClicked() {
    if (!m_activeTrack) return;
    auto idToRemove = m_activeTrack->id;

    auto it = std::find_if(m_session->tracks.begin(), m_session->tracks.end(),
                            [&](const auto& t) { return t->id == idToRemove; });
    if (it == m_session->tracks.end()) return;

    m_timeline->removeTrack(idToRemove);
    m_session->tracks.erase(it);
    m_activeTrack = m_session->tracks.empty() ? nullptr : m_session->tracks.front();
    updateStatusLabel();
    refreshTimelineScale();
}

void MainWindow::onTrackSelected(std::shared_ptr<Track> track) {
    m_activeTrack = std::move(track);
}

void MainWindow::onClipSelectionChanged(std::shared_ptr<Track> track, bool hasSelection) {
    m_trackWithClipSelection = hasSelection ? std::move(track) : nullptr;
    m_deleteClipButton->setEnabled(hasSelection);
}

void MainWindow::onDeleteClipClicked() {
    if (!m_trackWithClipSelection) return;
    m_timeline->deleteSelectedClipOn(m_trackWithClipSelection->id);
    m_deleteClipButton->setEnabled(false);
    m_trackWithClipSelection.reset();
    updateStatusLabel();
    refreshTimelineScale();
}

void MainWindow::onRecordClicked() {
    // Record-armed tracks are the target; if none are armed, fall back to
    // whichever track is Active so recording still works out of the box.
    m_recordTargetTracks.clear();
    for (auto& track : m_session->tracks) {
        if (track->recordArmed.load()) m_recordTargetTracks.push_back(track);
    }
    if (m_recordTargetTracks.empty() && m_activeTrack) {
        m_recordTargetTracks.push_back(m_activeTrack);
    }
    if (m_recordTargetTracks.empty()) {
        QMessageBox::warning(this, "No Track", "Add a track first.");
        return;
    }

    m_activeRecordingClip = std::make_shared<Clip>();
    m_activeRecordingClip->buffer = std::make_shared<AudioBuffer>();
    m_activeRecordingClip->buffer->channels = m_session->channels;
    m_activeRecordingClip->buffer->sampleRate = m_session->sampleRate;
    m_activeRecordingClip->name = "Recording";
    m_activeRecordingClip->sessionStartSample = m_engine->transport().positionSamples();

    m_engine->transport().setState(TransportState::Recording);
    m_ringDrainTimer->start();
    m_playheadTimer->start();

    m_recordButton->setEnabled(false);
    m_playButton->setEnabled(false);
    m_playFromStartButton->setEnabled(false);
    m_stopButton->setEnabled(true);

    QStringList names;
    for (auto& t : m_recordTargetTracks) names << t->name;
    m_statusLabel->setText("Recording into " + names.join(", ") + "...");
}

void MainWindow::onPlayClicked() {
    // Resumes from wherever the playhead currently is (e.g. after a seek on
    // the ruler), unlike "Play from Start" which always rewinds to 0 first.
    startPlayback();
}

void MainWindow::onPlayFromStartClicked() {
    m_engine->transport().setPositionSamples(0);
    startPlayback();
}

void MainWindow::startPlayback() {
    m_engine->transport().setState(TransportState::Playing);
    m_playheadTimer->start();

    m_recordButton->setEnabled(false);
    m_playButton->setEnabled(false);
    m_playFromStartButton->setEnabled(false);
    m_stopButton->setEnabled(true);
    m_statusLabel->setText("Playing...");
}

void MainWindow::onStopClicked() {
    const bool wasRecording = m_engine->transport().state() == TransportState::Recording;
    m_engine->transport().setState(TransportState::Stopped);
    m_ringDrainTimer->stop();
    m_playheadTimer->stop();

    if (wasRecording && m_activeRecordingClip && !m_recordTargetTracks.empty()) {
        drainCaptureRing(); // flush any remaining samples
        m_activeRecordingClip->lengthSamples = m_activeRecordingClip->buffer->frameCount();

        // Every armed track gets its own Clip (so each can be trimmed/moved
        // independently later) but they all share the same recorded
        // AudioBuffer — identical audio, no data duplicated in memory.
        for (auto& track : m_recordTargetTracks) {
            auto clip = std::make_shared<Clip>(*m_activeRecordingClip);
            clip->id = QUuid::createUuid();
            track->addClip(clip);
            refreshWaveformFor(track);
        }
        m_activeRecordingClip.reset();
        m_recordTargetTracks.clear();
        refreshTimelineScale();
    }

    m_recordButton->setEnabled(true);
    m_playButton->setEnabled(true);
    m_playFromStartButton->setEnabled(true);
    m_stopButton->setEnabled(false);
    updateStatusLabel();
    updatePlayhead();
}

void MainWindow::onSeekRequested(int64_t sample) {
    m_engine->transport().setPositionSamples(sample);
    updatePlayhead();
}

void MainWindow::updatePlayhead() {
    int64_t pos = m_engine->transport().positionSamples();
    m_timeline->setPlayheadSample(pos);
    m_ruler->setPlayheadSample(pos);
}

void MainWindow::drainCaptureRing() {
    if (!m_activeRecordingClip) return;

    float tmp[4096];
    size_t n;
    while ((n = m_engine->captureRing().read(tmp, 4096)) > 0) {
        auto& samples = m_activeRecordingClip->buffer->samples;
        samples.insert(samples.end(), tmp, tmp + n);
    }
}

void MainWindow::onImportClicked() {
    if (!m_activeTrack) {
        QMessageBox::warning(this, "No Track", "Add a track first.");
        return;
    }

    QString path = QFileDialog::getOpenFileName(this, "Import Audio File", QString(),
                                                  "Audio Files (*.wav *.flac *.ogg *.aiff)");
    if (path.isEmpty()) return;

    auto buffer = AudioFileIO::loadFile(path);
    if (!buffer) {
        QMessageBox::warning(this, "Import Failed", "Could not load: " + path);
        return;
    }

    auto clip = std::make_shared<Clip>();
    clip->buffer = buffer;
    clip->name = QFileInfo(path).fileName();
    clip->sessionStartSample = 0;
    clip->sourceOffsetSamples = 0;
    clip->lengthSamples = buffer->frameCount();

    m_activeTrack->addClip(clip);
    updateStatusLabel();
    refreshWaveformFor(m_activeTrack);
    refreshTimelineScale();
}

void MainWindow::onExportClicked() {
    if (!m_activeTrack) {
        QMessageBox::warning(this, "No Track", "Add a track first.");
        return;
    }

    QString path = QFileDialog::getSaveFileName(this, "Export Active Track", QString(),
                                                  "WAV Files (*.wav)");
    if (path.isEmpty()) return;

    auto rendered = renderTrackToBuffer(*m_activeTrack);
    if (!AudioFileIO::writeFile(path, *rendered)) {
        QMessageBox::warning(this, "Export Failed", "Could not write: " + path);
        return;
    }

    QMessageBox::information(this, "Export Complete", "Saved to: " + path);
}

void MainWindow::onSaveSessionClicked() {
    QString path = QFileDialog::getSaveFileName(this, "Save Session", QString(),
                                                  "Recording Studio Project (*.rsdproj)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".rsdproj")) path += ".rsdproj";

    if (!SessionIO::saveSession(path, *m_session)) {
        QMessageBox::warning(this, "Save Failed", "Could not save session to: " + path);
        return;
    }

    QMessageBox::information(this, "Session Saved", "Saved to: " + path);
}

void MainWindow::onLoadSessionClicked() {
    QString path = QFileDialog::getOpenFileName(this, "Load Session", QString(),
                                                  "Recording Studio Project (*.rsdproj)");
    if (path.isEmpty()) return;

    onStopClicked(); // stop any playback/recording before swapping session state

    if (!SessionIO::loadSession(path, *m_session)) {
        QMessageBox::warning(this, "Load Failed", "Could not load session from: " + path);
        return;
    }

    rebuildTimelineFromSession();
    QMessageBox::information(this, "Session Loaded", "Loaded: " + path);
}

void MainWindow::rebuildTimelineFromSession() {
    m_timeline->clear();
    m_activeTrack.reset();
    m_trackWithClipSelection.reset();
    m_deleteClipButton->setEnabled(false);

    m_trackCounter = 0;
    for (auto& track : m_session->tracks) {
        m_timeline->addTrack(track);
        if (!m_activeTrack) m_activeTrack = track;
        ++m_trackCounter;
    }

    m_engine->transport().setPositionSamples(0);
    updateStatusLabel();
    refreshTimelineScale();
    updatePlayhead();
}

std::shared_ptr<AudioBuffer> MainWindow::renderTrackToBuffer(const Track& track) const {
    auto clips = track.clipsSnapshot();

    int64_t totalFrames = 0;
    for (auto& clip : *clips) {
        totalFrames = std::max(totalFrames, clip->sessionStartSample + clip->lengthSamples);
    }

    auto out = std::make_shared<AudioBuffer>();
    out->channels = m_session->channels;
    out->sampleRate = m_session->sampleRate;
    out->samples.assign(static_cast<size_t>(totalFrames) * out->channels, 0.0f);

    for (auto& clip : *clips) {
        if (clip->muted || !clip->buffer) continue;
        for (int64_t i = 0; i < clip->lengthSamples; ++i) {
            int64_t sourceFrame = clip->sourceOffsetSamples + i;
            if (sourceFrame < 0 || sourceFrame >= clip->buffer->frameCount()) continue;
            int64_t destFrame = clip->sessionStartSample + i;

            for (int ch = 0; ch < out->channels; ++ch) {
                int srcCh = ch % clip->buffer->channels;
                out->samples[destFrame * out->channels + ch] +=
                    clip->buffer->samples[sourceFrame * clip->buffer->channels + srcCh];
            }
        }
    }

    return out;
}

void MainWindow::refreshWaveformFor(const std::shared_ptr<Track>& track) {
    if (!track) return;
    m_timeline->refreshTrackWaveform(track->id);
}

void MainWindow::refreshTimelineScale() {
    int64_t maxEnd = 0;
    for (auto& track : m_session->tracks) {
        auto clips = track->clipsSnapshot();
        for (auto& clip : *clips) {
            maxEnd = std::max(maxEnd, clip->sessionStartSample + clip->lengthSamples);
        }
    }
    // Same fixed floor + headroom policy as ClipLaneWidget used to compute
    // locally — now computed once here so every lane and the ruler agree.
    int64_t floor = static_cast<int64_t>(m_session->sampleRate) * 30;
    int64_t headroom = static_cast<int64_t>(m_session->sampleRate) * 10;
    int64_t total = std::max(floor, maxEnd + headroom);

    m_timeline->setSharedTimelineLength(total);
    m_ruler->setTimelineLength(total);
}

void MainWindow::updateStatusLabel() {
    int totalClips = 0;
    for (auto& track : m_session->tracks) {
        totalClips += static_cast<int>(track->clipsSnapshot()->size());
    }
    m_statusLabel->setText(
        QString("Stopped — %1 track(s), %2 clip(s)").arg(m_session->tracks.size()).arg(totalClips));
}

} // namespace rsd
