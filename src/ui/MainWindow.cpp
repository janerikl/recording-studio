#include "MainWindow.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QShortcut>
#include <QStringList>
#include <QStyle>
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

    // --- Actions (shared between menus and the toolbar where noted) ---
    m_recordAction = new QAction(recordIcon(), "Record", this);
    m_recordAction->setToolTip("Record (R)");
    m_playAction = new QAction(style()->standardIcon(QStyle::SP_MediaPlay), "Play", this);
    m_playAction->setToolTip("Play — resumes from the playhead (Space)");
    m_playFromStartAction =
        new QAction(style()->standardIcon(QStyle::SP_MediaSkipBackward), "Play from Start", this);
    m_playFromStartAction->setToolTip("Play from Start — always rewinds to 0 first");
    m_stopAction = new QAction(style()->standardIcon(QStyle::SP_MediaStop), "Stop", this);
    m_stopAction->setToolTip("Stop (Space)");
    m_stopAction->setEnabled(false);

    m_addTrackAction =
        new QAction(style()->standardIcon(QStyle::SP_FileDialogNewFolder), "Add Track", this);
    m_addTrackAction->setToolTip("Add Track");
    m_removeTrackAction = new QAction(style()->standardIcon(QStyle::SP_TrashIcon), "Remove Track", this);
    m_removeTrackAction->setToolTip("Remove the Active track");

    auto* importAction = new QAction("Import...", this);
    auto* exportAction = new QAction("Export Active Track...", this);
    auto* saveSessionAction = new QAction("Save Session...", this);
    auto* loadSessionAction = new QAction("Load Session...", this);
    auto* settingsAction = new QAction("Settings...", this);

    m_undoAction = new QAction(style()->standardIcon(QStyle::SP_ArrowBack), "Undo", this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);
    m_redoAction = new QAction(style()->standardIcon(QStyle::SP_ArrowForward), "Redo", this);
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_redoAction->setEnabled(false);
    m_deleteClipAction =
        new QAction(style()->standardIcon(QStyle::SP_DialogDiscardButton), "Delete Selected Clip", this);
    m_deleteClipAction->setShortcut(QKeySequence(Qt::Key_Delete));
    m_deleteClipAction->setEnabled(false);

    connect(m_recordAction, &QAction::triggered, this, &MainWindow::onRecordClicked);
    connect(m_playAction, &QAction::triggered, this, &MainWindow::onPlayClicked);
    connect(m_playFromStartAction, &QAction::triggered, this, &MainWindow::onPlayFromStartClicked);
    connect(m_stopAction, &QAction::triggered, this, &MainWindow::onStopClicked);
    connect(importAction, &QAction::triggered, this, &MainWindow::onImportClicked);
    connect(exportAction, &QAction::triggered, this, &MainWindow::onExportClicked);
    connect(m_addTrackAction, &QAction::triggered, this, &MainWindow::onAddTrackClicked);
    connect(m_removeTrackAction, &QAction::triggered, this, &MainWindow::onRemoveTrackClicked);
    connect(m_deleteClipAction, &QAction::triggered, this, &MainWindow::onDeleteClipClicked);
    connect(saveSessionAction, &QAction::triggered, this, &MainWindow::onSaveSessionClicked);
    connect(loadSessionAction, &QAction::triggered, this, &MainWindow::onLoadSessionClicked);
    connect(m_undoAction, &QAction::triggered, this, &MainWindow::onUndoClicked);
    connect(m_redoAction, &QAction::triggered, this, &MainWindow::onRedoClicked);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::onSettingsClicked);

    // --- Menus ---
    auto* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction(importAction);
    fileMenu->addAction(exportAction);
    fileMenu->addSeparator();
    fileMenu->addAction(saveSessionAction);
    fileMenu->addAction(loadSessionAction);
    fileMenu->addSeparator();
    fileMenu->addAction(settingsAction);

    auto* editMenu = menuBar()->addMenu("&Edit");
    editMenu->addAction(m_undoAction);
    editMenu->addAction(m_redoAction);
    editMenu->addSeparator();
    editMenu->addAction(m_deleteClipAction);
    editMenu->addSeparator();
    editMenu->addAction(m_addTrackAction);
    editMenu->addAction(m_removeTrackAction);

    // --- Toolbar: frequently-used actions as icons, text hidden (tooltip shows on hover) ---
    auto* toolbar = addToolBar("Main");
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolbar->setMovable(false);
    toolbar->addAction(m_recordAction);
    toolbar->addAction(m_playAction);
    toolbar->addAction(m_playFromStartAction);
    toolbar->addAction(m_stopAction);
    toolbar->addSeparator();
    toolbar->addAction(m_addTrackAction);
    toolbar->addAction(m_removeTrackAction);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);

    m_statusLabel = new QLabel("Stopped — 0 tracks, 0 clips", central);
    layout->addWidget(m_statusLabel);

    auto* meterRow = new QWidget(central);
    auto* meterLayout = new QHBoxLayout(meterRow);
    m_inputMeter = new LevelMeterWidget("In", meterRow);
    m_outputMeter = new LevelMeterWidget("Out", meterRow);
    meterLayout->addWidget(m_inputMeter);
    meterLayout->addWidget(m_outputMeter);
    layout->addWidget(meterRow);

    m_ruler = new TimeRulerWidget(central);
    m_ruler->setSampleRate(m_session->sampleRate);
    connect(m_ruler, &TimeRulerWidget::seekRequested, this, &MainWindow::onSeekRequested);
    layout->addWidget(m_ruler);

    m_timeline = new TimelineView(central);
    connect(m_timeline, &TimelineView::trackSelected, this, &MainWindow::onTrackSelected);
    connect(m_timeline, &TimelineView::clipSelectionChanged, this,
            &MainWindow::onClipSelectionChanged);
    connect(m_timeline, &TimelineView::seekRequested, this, &MainWindow::onSeekRequested);
    connect(m_timeline, &TimelineView::editStarted, this, &MainWindow::onClipEditStarted);
    layout->addWidget(m_timeline, 1);

    setCentralWidget(central);

    m_ringDrainTimer = new QTimer(this);
    m_ringDrainTimer->setInterval(30);
    connect(m_ringDrainTimer, &QTimer::timeout, this, &MainWindow::drainCaptureRing);

    m_playheadTimer = new QTimer(this);
    m_playheadTimer->setInterval(33); // ~30fps
    connect(m_playheadTimer, &QTimer::timeout, this, &MainWindow::updatePlayhead);

    m_meterTimer = new QTimer(this);
    m_meterTimer->setInterval(33);
    connect(m_meterTimer, &QTimer::timeout, this, &MainWindow::updateMeters);
    m_meterTimer->start(); // always running, so input signal is visible before Record

    if (!m_engine->start()) {
        m_statusLabel->setText("Failed to start audio engine — check console");
        m_recordAction->setEnabled(false);
        m_playAction->setEnabled(false);
        m_playFromStartAction->setEnabled(false);
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

    // Delete/Undo/Redo shortcuts already live on their QActions above; only
    // Backspace (an alias for delete-clip) and R need their own QShortcut.
    auto* backspaceShortcut = new QShortcut(QKeySequence(Qt::Key_Backspace), this);
    connect(backspaceShortcut, &QShortcut::activated, this, &MainWindow::onDeleteClipClicked);

    auto* recordShortcut = new QShortcut(QKeySequence(Qt::Key_R), this);
    connect(recordShortcut, &QShortcut::activated, this, [this]() {
        if (m_engine->transport().state() == TransportState::Stopped) onRecordClicked();
    });
}

void MainWindow::onAddTrackClicked() {
    pushUndoSnapshot();
    ++m_trackCounter;
    auto track = m_session->addTrack(QString("Track %1").arg(m_trackCounter));
    m_timeline->addTrack(track);
    if (!m_activeTrack) m_activeTrack = track;
    updateStatusLabel();
    refreshTimelineScale();
}

void MainWindow::onRemoveTrackClicked() {
    if (!m_activeTrack) return;
    pushUndoSnapshot();
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
    m_deleteClipAction->setEnabled(hasSelection);
}

void MainWindow::onDeleteClipClicked() {
    if (!m_trackWithClipSelection) return;
    pushUndoSnapshot();
    m_timeline->deleteSelectedClipOn(m_trackWithClipSelection->id);
    m_deleteClipAction->setEnabled(false);
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

    m_recordAction->setEnabled(false);
    m_playAction->setEnabled(false);
    m_playFromStartAction->setEnabled(false);
    m_stopAction->setEnabled(true);

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

    m_recordAction->setEnabled(false);
    m_playAction->setEnabled(false);
    m_playFromStartAction->setEnabled(false);
    m_stopAction->setEnabled(true);
    m_statusLabel->setText("Playing...");
}

void MainWindow::onStopClicked() {
    const bool wasRecording = m_engine->transport().state() == TransportState::Recording;
    m_engine->transport().setState(TransportState::Stopped);
    m_ringDrainTimer->stop();
    m_playheadTimer->stop();

    if (wasRecording && m_activeRecordingClip && !m_recordTargetTracks.empty()) {
        pushUndoSnapshot();
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

    m_recordAction->setEnabled(true);
    m_playAction->setEnabled(true);
    m_playFromStartAction->setEnabled(true);
    m_stopAction->setEnabled(false);
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

void MainWindow::updateMeters() {
    m_inputMeter->setLevel(m_engine->inputPeak());
    m_outputMeter->setLevel(m_engine->outputPeak());
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

    pushUndoSnapshot();
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

void MainWindow::onSettingsClicked() {
    onStopClicked(); // don't restart the stream mid-playback/recording

    SettingsDialog dialog(*m_engine, this);
    if (dialog.exec() != QDialog::Accepted) return;

    // Existing clips keep their sample counts at whatever rate they were
    // recorded/imported at — changing the engine's rate here doesn't
    // resample them, so pitch/duration will shift for prior content. Fine
    // for a rate chosen before recording; a caveat for changing mid-session.
    m_session->sampleRate = static_cast<int>(dialog.chosenSampleRate());
    m_ruler->setSampleRate(m_session->sampleRate);
    refreshTimelineScale();

    m_recordAction->setEnabled(true);
    m_playAction->setEnabled(true);
    m_playFromStartAction->setEnabled(true);
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

    m_undoStack.clear();
    m_redoStack.clear();
    updateUndoRedoButtons();

    rebuildTimelineFromSession();
    QMessageBox::information(this, "Session Loaded", "Loaded: " + path);
}

void MainWindow::rebuildTimelineFromSession() {
    m_timeline->clear();
    m_activeTrack.reset();
    m_trackWithClipSelection.reset();
    m_deleteClipAction->setEnabled(false);

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

void MainWindow::onClipEditStarted() {
    pushUndoSnapshot();
}

SessionSnapshot MainWindow::captureSnapshot() const {
    SessionSnapshot snapshot;
    for (auto& track : m_session->tracks) {
        TrackSnapshot ts;
        ts.id = track->id;
        ts.name = track->name;
        ts.gain = track->gain;
        ts.muted = track->muted.load();
        ts.soloed = track->soloed.load();
        ts.recordArmed = track->recordArmed.load();
        for (auto& clip : *track->clipsSnapshot()) {
            ClipSnapshot cs;
            cs.id = clip->id;
            cs.buffer = clip->buffer; // shared, immutable sample data — not deep-copied
            cs.sessionStartSample = clip->sessionStartSample;
            cs.sourceOffsetSamples = clip->sourceOffsetSamples;
            cs.lengthSamples = clip->lengthSamples;
            cs.name = clip->name;
            cs.muted = clip->muted;
            ts.clips.push_back(std::move(cs));
        }
        snapshot.push_back(std::move(ts));
    }
    return snapshot;
}

void MainWindow::restoreSnapshot(const SessionSnapshot& snapshot) {
    m_session->tracks.clear();
    for (auto& ts : snapshot) {
        auto track = std::make_shared<Track>();
        track->id = ts.id;
        track->name = ts.name;
        track->gain = ts.gain;
        track->muted.store(ts.muted);
        track->soloed.store(ts.soloed);
        track->recordArmed.store(ts.recordArmed);
        for (auto& cs : ts.clips) {
            auto clip = std::make_shared<Clip>();
            clip->id = cs.id;
            clip->buffer = cs.buffer;
            clip->sessionStartSample = cs.sessionStartSample;
            clip->sourceOffsetSamples = cs.sourceOffsetSamples;
            clip->lengthSamples = cs.lengthSamples;
            clip->name = cs.name;
            clip->muted = cs.muted;
            track->addClip(clip);
        }
        m_session->tracks.push_back(track);
    }
    rebuildTimelineFromSession();
}

void MainWindow::pushUndoSnapshot() {
    m_undoStack.push_back(captureSnapshot());
    if (m_undoStack.size() > kMaxUndoDepth) m_undoStack.erase(m_undoStack.begin());
    m_redoStack.clear(); // a fresh edit invalidates any redo history
    updateUndoRedoButtons();
}

void MainWindow::onUndoClicked() {
    if (m_undoStack.empty()) return;
    onStopClicked(); // don't mutate session state while the audio thread is reading it

    m_redoStack.push_back(captureSnapshot());
    SessionSnapshot snapshot = m_undoStack.back();
    m_undoStack.pop_back();
    restoreSnapshot(snapshot);
    updateUndoRedoButtons();
}

void MainWindow::onRedoClicked() {
    if (m_redoStack.empty()) return;
    onStopClicked();

    m_undoStack.push_back(captureSnapshot());
    SessionSnapshot snapshot = m_redoStack.back();
    m_redoStack.pop_back();
    restoreSnapshot(snapshot);
    updateUndoRedoButtons();
}

void MainWindow::updateUndoRedoButtons() {
    m_undoAction->setEnabled(!m_undoStack.empty());
    m_redoAction->setEnabled(!m_redoStack.empty());
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

QIcon MainWindow::recordIcon() {
    // Qt's standard icon set has no "record" glyph; draw the conventional
    // filled red circle instead of bundling an external asset.
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(220, 50, 50));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(6, 6, 20, 20);
    return QIcon(pixmap);
}

} // namespace rsd
