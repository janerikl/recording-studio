#include "MainWindow.h"

#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QShortcut>
#include <QStringList>
#include <QStyle>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>
#include <algorithm>

#include "audio/PanLawMath.h"
#include "audio/RecordRouting.h"
#include "command/EditCommands.h"
#include "command/PunchRecordingCommand.h"
#include "io/AudioFileIO.h"
#include "io/SessionIO.h"
#include "model/CompMath.h"
#include "ui/TimelineScaleMath.h"

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

    // Started early (before any Settings dialog can be opened) so the
    // "Recording Studio System Audio" device has time to register with
    // PipeWire and show up in the device list. No-op if pw-loopback isn't
    // installed — Settings' System Audio dropdown just has nothing to pick.
    m_systemAudioLoopback.start();

    // --- Actions (shared between menus and the toolbar where noted) ---
    // Prefer the user's system icon theme (freedesktop names) since it looks
    // native and consistent with the rest of the desktop; fall back to Qt's
    // generic built-ins only if a theme icon by that name isn't installed.
    m_recordAction = new QAction(QIcon::fromTheme("media-record-symbolic", recordIcon()), "Record", this);
    m_recordAction->setToolTip("Record (R)");
    m_playAction = new QAction(
        QIcon::fromTheme("media-playback-start-symbolic", style()->standardIcon(QStyle::SP_MediaPlay)),
        "Play", this);
    m_playAction->setToolTip("Play — resumes from the playhead (Space)");
    m_playFromStartAction = new QAction(
        QIcon::fromTheme("media-seek-backward-symbolic",
                          style()->standardIcon(QStyle::SP_MediaSkipBackward)),
        "Play from Start", this);
    m_playFromStartAction->setToolTip("Play from Start — always rewinds to 0 first");
    m_stopAction = new QAction(
        QIcon::fromTheme("media-playback-stop-symbolic", style()->standardIcon(QStyle::SP_MediaStop)),
        "Stop", this);
    m_stopAction->setToolTip("Stop (Space)");
    m_stopAction->setEnabled(false);

    m_addTrackAction = new QAction(
        QIcon::fromTheme("list-add-symbolic", style()->standardIcon(QStyle::SP_FileDialogNewFolder)),
        "Add Track", this);
    m_addTrackAction->setToolTip("Add Track");
    m_addInstrumentTrackAction = new QAction(
        QIcon::fromTheme("list-add-symbolic", style()->standardIcon(QStyle::SP_FileDialogNewFolder)),
        "Add Instrument Track", this);
    m_addInstrumentTrackAction->setToolTip("Add Instrument Track (basic synth, on-screen keyboard)");
    m_removeTrackAction = new QAction(
        QIcon::fromTheme("list-remove-symbolic", style()->standardIcon(QStyle::SP_TrashIcon)),
        "Remove Track", this);
    m_removeTrackAction->setToolTip("Remove the Active track");

    m_zoomInAction = new QAction(QIcon::fromTheme("zoom-in-symbolic"), "Zoom In", this);
    m_zoomInAction->setToolTip("Zoom In (Ctrl++)");
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    m_zoomOutAction = new QAction(QIcon::fromTheme("zoom-out-symbolic"), "Zoom Out", this);
    m_zoomOutAction->setToolTip("Zoom Out (Ctrl+-)");
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    m_zoomResetAction = new QAction(QIcon::fromTheme("zoom-original-symbolic"), "Reset Zoom", this);
    m_zoomResetAction->setToolTip("Reset Zoom");

    auto* importAction =
        new QAction(QIcon::fromTheme("document-open-symbolic"), "Import...", this);
    auto* exportAction =
        new QAction(QIcon::fromTheme("document-save-as-symbolic"), "Export Active Track...", this);
    auto* saveSessionAction =
        new QAction(QIcon::fromTheme("document-save-symbolic"), "Save Session...", this);
    saveSessionAction->setShortcut(QKeySequence::Save); // Ctrl+S
    auto* loadSessionAction =
        new QAction(QIcon::fromTheme("document-open-symbolic"), "Load Session...", this);
    auto* closeSessionAction =
        new QAction(QIcon::fromTheme("window-close-symbolic"), "Close Session", this);
    auto* settingsAction =
        new QAction(QIcon::fromTheme("preferences-system-symbolic"), "Settings...", this);

    m_undoAction = new QAction(
        QIcon::fromTheme("edit-undo-symbolic", style()->standardIcon(QStyle::SP_ArrowBack)), "Undo", this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);
    m_redoAction = new QAction(
        QIcon::fromTheme("edit-redo-symbolic", style()->standardIcon(QStyle::SP_ArrowForward)), "Redo",
        this);
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_redoAction->setEnabled(false);
    m_deleteClipAction = new QAction(
        QIcon::fromTheme("edit-delete-symbolic", style()->standardIcon(QStyle::SP_DialogDiscardButton)),
        "Delete Selected Clip", this);
    m_deleteClipAction->setShortcut(QKeySequence(Qt::Key_Delete));
    m_deleteClipAction->setEnabled(false);

    connect(m_recordAction, &QAction::triggered, this, &MainWindow::onRecordClicked);
    connect(m_playAction, &QAction::triggered, this, &MainWindow::onPlayClicked);
    connect(m_playFromStartAction, &QAction::triggered, this, &MainWindow::onPlayFromStartClicked);
    connect(m_stopAction, &QAction::triggered, this, &MainWindow::onStopClicked);
    connect(importAction, &QAction::triggered, this, &MainWindow::onImportClicked);
    connect(exportAction, &QAction::triggered, this, &MainWindow::onExportClicked);
    connect(m_addTrackAction, &QAction::triggered, this, &MainWindow::onAddTrackClicked);
    connect(m_addInstrumentTrackAction, &QAction::triggered, this,
            &MainWindow::onAddInstrumentTrackClicked);
    connect(m_removeTrackAction, &QAction::triggered, this, &MainWindow::onRemoveTrackClicked);
    connect(m_zoomInAction, &QAction::triggered, this, &MainWindow::onZoomInClicked);
    connect(m_zoomOutAction, &QAction::triggered, this, &MainWindow::onZoomOutClicked);
    connect(m_zoomResetAction, &QAction::triggered, this, &MainWindow::onZoomResetClicked);
    connect(m_deleteClipAction, &QAction::triggered, this, &MainWindow::onDeleteClipClicked);
    connect(saveSessionAction, &QAction::triggered, this, &MainWindow::onSaveSessionClicked);
    connect(loadSessionAction, &QAction::triggered, this, &MainWindow::onLoadSessionClicked);
    connect(closeSessionAction, &QAction::triggered, this, &MainWindow::onCloseSessionClicked);
    connect(m_undoAction, &QAction::triggered, this, &MainWindow::onUndoClicked);
    connect(m_redoAction, &QAction::triggered, this, &MainWindow::onRedoClicked);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::onSettingsClicked);

    auto* loadRecentOnStartupAction = new QAction("Load Most Recent Session on Startup", this);
    loadRecentOnStartupAction->setCheckable(true);
    {
        QSettings settings("RecordingStudio", "RecordingStudio");
        loadRecentOnStartupAction->setChecked(settings.value("loadRecentOnStartup", false).toBool());
    }
    connect(loadRecentOnStartupAction, &QAction::toggled, this, [](bool checked) {
        QSettings settings("RecordingStudio", "RecordingStudio");
        settings.setValue("loadRecentOnStartup", checked);
    });

    // --- Menus ---
    auto* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction(importAction);
    fileMenu->addAction(exportAction);
    fileMenu->addSeparator();
    fileMenu->addAction(saveSessionAction);
    fileMenu->addAction(loadSessionAction);
    m_recentSessionsMenu = fileMenu->addMenu("Open Recent");
    fileMenu->addAction(closeSessionAction);
    fileMenu->addSeparator();
    fileMenu->addAction(loadRecentOnStartupAction);
    fileMenu->addAction(settingsAction);

    {
        QSettings settings("RecordingStudio", "RecordingStudio");
        m_recentSessionPaths = settings.value("recentSessions").toStringList();
    }
    rebuildRecentSessionsMenu();

    auto* editMenu = menuBar()->addMenu("&Edit");
    editMenu->addAction(m_undoAction);
    editMenu->addAction(m_redoAction);
    editMenu->addSeparator();
    editMenu->addAction(m_deleteClipAction);
    editMenu->addSeparator();
    editMenu->addAction(m_addTrackAction);
    editMenu->addAction(m_addInstrumentTrackAction);
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
    toolbar->addAction(m_addInstrumentTrackAction);
    toolbar->addAction(m_removeTrackAction);
    toolbar->addSeparator();
    toolbar->addAction(m_zoomInAction);
    toolbar->addAction(m_zoomOutAction);
    toolbar->addAction(m_zoomResetAction);
    toolbar->addSeparator();

    m_loopRecordCheckBox = new QCheckBox("Loop Record", this);
    m_loopRecordCheckBox->setToolTip(
        "When checked, Record plays 2s pre-roll then loops the punch region "
        "(right-drag on the ruler, or the In/Out fields) until Stop, "
        "replacing the take each pass.");
    toolbar->addWidget(m_loopRecordCheckBox);

    toolbar->addWidget(new QLabel(" In: ", this));
    m_punchInSpin = new QDoubleSpinBox(this);
    m_punchInSpin->setRange(0.0, 3600.0);
    m_punchInSpin->setDecimals(2);
    m_punchInSpin->setSuffix(" s");
    toolbar->addWidget(m_punchInSpin);

    toolbar->addWidget(new QLabel(" Out: ", this));
    m_punchOutSpin = new QDoubleSpinBox(this);
    m_punchOutSpin->setRange(0.0, 3600.0);
    m_punchOutSpin->setDecimals(2);
    m_punchOutSpin->setSuffix(" s");
    toolbar->addWidget(m_punchOutSpin);

    connect(m_punchInSpin, &QDoubleSpinBox::valueChanged, this, &MainWindow::onPunchFieldsChanged);
    connect(m_punchOutSpin, &QDoubleSpinBox::valueChanged, this, &MainWindow::onPunchFieldsChanged);

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
    connect(m_ruler, &TimeRulerWidget::punchRegionEdited, this,
            &MainWindow::onPunchRegionEditedOnRuler);
    layout->addWidget(m_ruler);

    // Compact summary strip: the mixed-down combination of every track,
    // respecting solo/mute/gain — refreshed only on structural changes (not
    // a live oscilloscope), via refreshMasterAndScale().
    m_masterWaveform = new WaveformWidget(central);
    m_masterWaveform->setFixedHeight(80);
    layout->addWidget(m_masterWaveform);

    m_timeline = new TimelineView(central);
    connect(m_timeline, &TimelineView::trackSelected, this, &MainWindow::onTrackSelected);
    connect(m_timeline, &TimelineView::clipSelectionChanged, this,
            &MainWindow::onClipSelectionChanged);
    connect(m_timeline, &TimelineView::seekRequested, this, &MainWindow::onSeekRequested);
    connect(m_timeline, &TimelineView::clipMovedToTrack, this, &MainWindow::onClipMovedToTrack);
    m_timeline->setCommandStack(&m_commandStack);
    connect(m_timeline, &TimelineView::mediaDroppedOnTrack, this,
            &MainWindow::onMediaDroppedOnTrack);
    connect(m_timeline, &TimelineView::effectsPanelRequested, this,
            &MainWindow::onEffectsPanelRequested);
    connect(m_timeline, &TimelineView::takeSelected, this, &MainWindow::onTakeSelected);
    layout->addWidget(m_timeline, 1);

    setCentralWidget(central);

    auto* mediaDock = new QDockWidget("Media Library", this);
    m_mediaLibrary = new MediaLibraryPanel(mediaDock);
    connect(m_mediaLibrary, &MediaLibraryPanel::fileLoadFailed, this, [this](const QString& path) {
        QMessageBox::warning(this, "Import Failed", "Could not load: " + path);
    });
    mediaDock->setWidget(m_mediaLibrary);
    addDockWidget(Qt::RightDockWidgetArea, mediaDock);

    m_effectsDock = new QDockWidget("Effects Rack", this);
    m_effectsRack = new EffectsRackPanel(m_effectsDock);
    m_effectsRack->setCommandStack(&m_commandStack);
    m_effectsRack->setSampleRate(m_session->sampleRate);
    connect(m_effectsRack, &EffectsRackPanel::effectCountChanged, this,
            [this](std::shared_ptr<Track> track) {
                if (track) m_timeline->refreshTrackEffectsButton(track->id);
            });
    m_effectsDock->setWidget(m_effectsRack);
    addDockWidget(Qt::RightDockWidgetArea, m_effectsDock);
    tabifyDockWidget(mediaDock, m_effectsDock);

    m_instrumentDock = new QDockWidget("Instrument", this);
    m_instrumentPanel = new InstrumentPanel(m_instrumentDock);
    connect(m_instrumentPanel, &InstrumentPanel::noteOn, this, &MainWindow::onInstrumentNoteOn);
    connect(m_instrumentPanel, &InstrumentPanel::noteOff, this, &MainWindow::onInstrumentNoteOff);
    m_instrumentDock->setWidget(m_instrumentPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_instrumentDock);
    tabifyDockWidget(mediaDock, m_instrumentDock);

    m_pianoRollDock = new QDockWidget("Piano Roll", this);
    m_pianoRollPanel = new PianoRollPanel(m_pianoRollDock);
    m_pianoRollPanel->setCommandStack(&m_commandStack);
    m_pianoRollPanel->setBpm(m_session->bpm);
    m_pianoRollPanel->setSampleRate(m_session->sampleRate);
    m_pianoRollDock->setWidget(m_pianoRollPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_pianoRollDock);
    tabifyDockWidget(mediaDock, m_pianoRollDock);

    // Each dock's built-in toggleViewAction stays in sync automatically
    // (checked/unchecked) whether it's hidden from here or via the dock's
    // own close button, so no extra state tracking is needed.
    auto* viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction(mediaDock->toggleViewAction());
    viewMenu->addAction(m_effectsDock->toggleViewAction());
    viewMenu->addAction(m_instrumentDock->toggleViewAction());
    viewMenu->addAction(m_pianoRollDock->toggleViewAction());

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

    // Ctrl+wheel zoom over the ruler, master strip, or track lanes.
    m_ruler->installEventFilter(this);
    m_masterWaveform->installEventFilter(this);
    m_timeline->viewport()->installEventFilter(this);

    if (!m_engine->start()) {
        m_statusLabel->setText("Failed to start audio engine — check console");
        m_recordAction->setEnabled(false);
        m_playAction->setEnabled(false);
        m_playFromStartAction->setEnabled(false);
    }

    bool autoLoaded = false;
    if (loadRecentOnStartupAction->isChecked() && !m_recentSessionPaths.isEmpty()) {
        autoLoaded = loadSessionFromPath(m_recentSessionPaths.first(), /*showSuccessMessage=*/false);
    }
    if (!autoLoaded) {
        onAddTrackClicked(); // start with one blank track, the normal default
    }
    refreshMasterAndScale();

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

    // Restore window size/position and dock layout from last run, if any.
    QSettings settings("RecordingStudio", "RecordingStudio");
    if (settings.contains("mainWindow/geometry")) {
        restoreGeometry(settings.value("mainWindow/geometry").toByteArray());
    }
    if (settings.contains("mainWindow/state")) {
        restoreState(settings.value("mainWindow/state").toByteArray());
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    QSettings settings("RecordingStudio", "RecordingStudio");
    settings.setValue("mainWindow/geometry", saveGeometry());
    settings.setValue("mainWindow/state", saveState());
    QMainWindow::closeEvent(event);
}

void MainWindow::onAddTrackClicked() {
    ++m_trackCounter;
    auto track = std::make_shared<Track>();
    track->name = QString("Track %1").arg(m_trackCounter);
    m_commandStack.push(std::make_unique<AddTrackCommand>(m_session.get(), track));
    m_timeline->addTrack(track);
    if (!m_activeTrack) m_activeTrack = track;
    updateStatusLabel();
    refreshMasterAndScale();
    updateUndoRedoButtons();
}

void MainWindow::onAddInstrumentTrackClicked() {
    ++m_trackCounter;
    auto track = std::make_shared<Track>();
    track->kind = TrackKind::Instrument;
    track->name = QString("Instrument %1").arg(m_trackCounter);
    m_commandStack.push(std::make_unique<AddTrackCommand>(m_session.get(), track));
    m_timeline->addTrack(track);
    if (!m_activeTrack) m_activeTrack = track;
    updateStatusLabel();
    refreshMasterAndScale();
    updateUndoRedoButtons();
}

void MainWindow::onRemoveTrackClicked() {
    if (!m_activeTrack) return;
    auto idToRemove = m_activeTrack->id;

    auto it = std::find_if(m_session->tracks.begin(), m_session->tracks.end(),
                            [&](const auto& t) { return t->id == idToRemove; });
    if (it == m_session->tracks.end()) return;

    size_t index = static_cast<size_t>(std::distance(m_session->tracks.begin(), it));
    m_commandStack.push(std::make_unique<RemoveTrackCommand>(m_session.get(), *it, index));
    m_timeline->removeTrack(idToRemove);
    m_activeTrack = m_session->tracks.empty() ? nullptr : m_session->tracks.front();
    updateStatusLabel();
    refreshMasterAndScale();
    updateUndoRedoButtons();
}

void MainWindow::onTrackSelected(std::shared_ptr<Track> track) {
    m_activeTrack = std::move(track);
    m_effectsRack->setTrack(m_activeTrack);
    m_instrumentPanel->setTrack(m_activeTrack);
    m_pianoRollPanel->setTrack(m_activeTrack);
}

void MainWindow::onInstrumentNoteOn(int pitch, float velocity) {
    if (!m_activeTrack || m_activeTrack->kind != TrackKind::Instrument) return;
    m_activeTrack->liveNoteEvents.push({pitch, velocity, true});

    if (m_activeTrack->recordArmed.load() && m_engine->transport().state() == TransportState::Recording) {
        m_pendingNoteStarts[pitch] = m_engine->transport().positionSamples();
    }
}

void MainWindow::onInstrumentNoteOff(int pitch) {
    if (!m_activeTrack || m_activeTrack->kind != TrackKind::Instrument) return;
    m_activeTrack->liveNoteEvents.push({pitch, 0.0f, false});

    auto it = m_pendingNoteStarts.find(pitch);
    if (it == m_pendingNoteStarts.end()) return;
    int64_t start = it->second;
    m_pendingNoteStarts.erase(it);

    int64_t length = m_engine->transport().positionSamples() - start;
    if (length <= 0) return;

    auto note = std::make_shared<MidiNote>();
    note->pitch = pitch;
    note->velocity = 0.9f;
    note->startSample = start;
    note->lengthSamples = length;
    m_pendingRecordedNotes.push_back(std::move(note));
}

void MainWindow::onEffectsPanelRequested(std::shared_ptr<Track>) {
    // Track selection already happened via the row's own "Active" radio
    // button (TrackRowWidget checks it before emitting this signal); just
    // bring the (possibly tabbed-behind) effects dock to the front.
    m_effectsDock->raise();
}

void MainWindow::onTakeSelected(std::shared_ptr<Track> track, std::shared_ptr<Clip> take) {
    if (!track || !take) return;

    int64_t regionStart = take->sessionStartSample;
    int64_t regionEnd = regionStart + take->lengthSamples;

    auto before = track->clipsSnapshot();
    auto updated = promoteTakeToComp(*before, take, regionStart, regionEnd);
    track->restoreClips(std::make_shared<const Track::ClipList>(std::move(updated)));
    auto after = track->clipsSnapshot();

    m_commandStack.push(std::make_unique<TrackClipsCommand>(track, before, after, "Comp Take"));
    updateUndoRedoButtons();
    refreshWaveformFor(track);
    m_timeline->refreshTrackTakeLanes(track->id); // updates which take shows as "(active)"
    refreshMasterAndScale();
}

void MainWindow::onClipSelectionChanged(std::shared_ptr<Track> track, bool hasSelection) {
    m_trackWithClipSelection = hasSelection ? std::move(track) : nullptr;
    m_deleteClipAction->setEnabled(hasSelection);
}

void MainWindow::onDeleteClipClicked() {
    if (!m_trackWithClipSelection) return;
    // deleteSelectedClipOn() pushes its own undo command (via
    // ClipLaneWidget::deleteSelected(), which owns the before/after snapshot).
    m_timeline->deleteSelectedClipOn(m_trackWithClipSelection->id);
    m_deleteClipAction->setEnabled(false);
    m_trackWithClipSelection.reset();
    updateStatusLabel();
    refreshMasterAndScale();
    updateUndoRedoButtons();
}

void MainWindow::onPunchRegionEditedOnRuler(PunchRegion region) {
    const QSignalBlocker blockIn(m_punchInSpin);
    const QSignalBlocker blockOut(m_punchOutSpin);
    double sr = std::max(1, m_session->sampleRate);
    m_punchInSpin->setValue(static_cast<double>(region.startSample) / sr);
    m_punchOutSpin->setValue(static_cast<double>(region.endSample) / sr);
}

void MainWindow::onPunchFieldsChanged() {
    double sr = std::max(1, m_session->sampleRate);
    PunchRegion region{static_cast<int64_t>(m_punchInSpin->value() * sr),
                        static_cast<int64_t>(m_punchOutSpin->value() * sr)};
    m_ruler->setPunchRegion(region);
}

void MainWindow::onRecordClicked() {
    // Record-armed tracks are the target; if none are armed, fall back to
    // whichever track is Active so recording still works out of the box.
    std::vector<std::shared_ptr<Track>> armedTracks;
    for (auto& track : m_session->tracks) {
        if (track->recordArmed.load()) armedTracks.push_back(track);
    }
    if (armedTracks.empty() && m_activeTrack) {
        armedTracks.push_back(m_activeTrack);
    }
    if (armedTracks.empty()) {
        QMessageBox::warning(this, "No Track", "Add a track first.");
        return;
    }

    // Instrument tracks don't record from an audio input stream at all —
    // MIDI note capture is handled separately (PianoKeyboardWidget writes
    // directly into the track's pending notes while armed+recording).
    std::vector<std::shared_ptr<Track>> armedAudioTracks;
    for (auto& t : armedTracks) {
        if (t->kind == TrackKind::Audio) armedAudioTracks.push_back(t);
    }
    auto split = splitTracksBySource(armedAudioTracks);
    m_recordTargetTracks = split.micTracks;
    m_systemAudioRecordTargetTracks = split.systemAudioTracks;

    if (!m_systemAudioRecordTargetTracks.empty() && !m_engine->systemAudioRunning()) {
        QMessageBox::warning(this, "System Audio Unavailable",
                              "A track is armed with Source: System Audio, but no system audio "
                              "device is configured/available (Settings > System Audio Device). "
                              "That track won't record.");
        m_systemAudioRecordTargetTracks.clear();
    }

    PunchRegion punchRegion = m_ruler->punchRegion();
    if (m_loopRecordCheckBox->isChecked() && punchRegion.isValid()) {
        // Punch/loop recording only supports the mic capture path (a single
        // target track fed by AudioEngine's punch recorder); system-audio
        // armed tracks are silently skipped for this mode.
        if (m_recordTargetTracks.empty()) {
            QMessageBox::warning(this, "No Mic Track",
                                  "Loop recording needs a Mic-source track armed.");
            return;
        }
        m_punchRecordingActive = true;
        auto track = m_recordTargetTracks.front();
        m_recordTargetTracks = {track}; // punch/loop recording targets a single track
        m_systemAudioRecordTargetTracks.clear();

        unsigned int channels = static_cast<unsigned int>(m_session->channels);
        int64_t preRollSamples =
            static_cast<int64_t>(kPunchPreRollSeconds * m_session->sampleRate);

        m_engine->setRecordTargetTrack(track);
        m_engine->punchRecorder().prepare(punchRegion, channels);
        m_engine->transport().setPunchRegion(punchRegion);
        m_engine->transport().setPreRollSamples(preRollSamples);
        m_engine->transport().setPunchLoopEnabled(true);
        m_engine->transport().setPositionSamples(
            std::max<int64_t>(0, punchRegion.startSample - preRollSamples));
        m_engine->transport().setState(TransportState::Recording);
        m_playheadTimer->start();

        m_recordAction->setEnabled(false);
        m_playAction->setEnabled(false);
        m_playFromStartAction->setEnabled(false);
        m_stopAction->setEnabled(true);
        m_statusLabel->setText("Loop recording into " + track->name + " (punch " +
                                m_punchInSpin->text() + "-" + m_punchOutSpin->text() + ")...");
        return;
    }

    m_punchRecordingActive = false;

    auto makeRecordingClip = [this]() {
        auto clip = std::make_shared<Clip>();
        clip->buffer = std::make_shared<AudioBuffer>();
        clip->buffer->channels = m_session->channels;
        clip->buffer->sampleRate = m_session->sampleRate;
        clip->name = "Recording";
        clip->sessionStartSample = m_engine->transport().positionSamples();
        return clip;
    };

    m_activeRecordingClip.reset();
    m_activeSystemAudioRecordingClip.reset();
    if (!m_recordTargetTracks.empty()) m_activeRecordingClip = makeRecordingClip();
    if (!m_systemAudioRecordTargetTracks.empty()) {
        m_activeSystemAudioRecordingClip = makeRecordingClip();
    }

    m_engine->transport().setState(TransportState::Recording);
    m_ringDrainTimer->start();
    m_playheadTimer->start();

    m_recordAction->setEnabled(false);
    m_playAction->setEnabled(false);
    m_playFromStartAction->setEnabled(false);
    m_stopAction->setEnabled(true);

    QStringList names;
    for (auto& t : m_recordTargetTracks) names << t->name;
    for (auto& t : m_systemAudioRecordTargetTracks) names << t->name;
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

    if (wasRecording && m_activeTrack && m_activeTrack->kind == TrackKind::Instrument) {
        // Finalize any note still held down when Stop was pressed, treating
        // this moment as its end.
        int64_t stopPos = m_engine->transport().positionSamples();
        for (auto& [pitch, start] : m_pendingNoteStarts) {
            int64_t length = stopPos - start;
            if (length <= 0) continue;
            auto note = std::make_shared<MidiNote>();
            note->pitch = pitch;
            note->velocity = 0.9f;
            note->startSample = start;
            note->lengthSamples = length;
            m_pendingRecordedNotes.push_back(std::move(note));
        }
        m_pendingNoteStarts.clear();

        if (!m_pendingRecordedNotes.empty()) {
            auto before = m_activeTrack->midiClipsSnapshot();
            Track::MidiNoteList updated(*before);
            for (auto& n : m_pendingRecordedNotes) updated.push_back(n);
            m_activeTrack->setMidiClips(updated);
            auto after = m_activeTrack->midiClipsSnapshot();
            m_commandStack.push(
                std::make_unique<TrackMidiCommand>(m_activeTrack, before, after, "Record MIDI"));
            m_pendingRecordedNotes.clear();
            m_timeline->refreshTrackWaveform(m_activeTrack->id);
            updateUndoRedoButtons();
        }
    }

    if (wasRecording && m_punchRecordingActive) {
        m_engine->transport().setPunchLoopEnabled(false);
        m_punchRecordingActive = false;
        if (!m_recordTargetTracks.empty()) {
            auto track = m_recordTargetTracks.front();
            if (m_engine->punchRecorder().takesCapExceeded()) {
                QMessageBox::information(
                    this, "Take Limit Reached",
                    QString("Only the last %1 takes are kept for comping; earlier passes in this "
                            "recording were discarded.")
                        .arg(PunchRecorder::kMaxTakes));
            }
            auto cmd = buildPunchRecordingCommand(track, m_engine->punchRecorder(),
                                                   static_cast<unsigned int>(m_session->sampleRate));
            if (cmd) {
                m_commandStack.push(std::move(cmd));
                refreshWaveformFor(track);
                m_timeline->refreshTrackTakeLanes(track->id);
            }
        }
        m_recordTargetTracks.clear();
        refreshMasterAndScale();
        updateUndoRedoButtons();
    } else if (wasRecording &&
               ((m_activeRecordingClip && !m_recordTargetTracks.empty()) ||
                (m_activeSystemAudioRecordingClip && !m_systemAudioRecordTargetTracks.empty()))) {
        drainCaptureRing(); // flush any remaining samples

        // Every armed track gets its own Clip (so each can be trimmed/moved
        // independently later) but tracks sharing a source share that
        // source's recorded AudioBuffer — identical audio, no data
        // duplicated in memory. Mic and system-audio tracks get separate
        // buffers since they came from separate capture streams.
        std::vector<std::unique_ptr<Command>> subCommands;
        auto addClipsFor = [&](std::shared_ptr<Clip>& sourceClip,
                                std::vector<std::shared_ptr<Track>>& targets) {
            if (!sourceClip || targets.empty()) return;
            sourceClip->lengthSamples = sourceClip->buffer->frameCount();
            for (auto& track : targets) {
                auto before = track->clipsSnapshot();
                auto clip = std::make_shared<Clip>(*sourceClip);
                clip->id = QUuid::createUuid();
                track->addClip(clip);
                subCommands.push_back(std::make_unique<TrackClipsCommand>(
                    track, before, track->clipsSnapshot(), "Record"));
                refreshWaveformFor(track);
            }
        };
        addClipsFor(m_activeRecordingClip, m_recordTargetTracks);
        addClipsFor(m_activeSystemAudioRecordingClip, m_systemAudioRecordTargetTracks);

        m_commandStack.push(std::make_unique<CompositeCommand>(std::move(subCommands), "Record"));
        m_activeRecordingClip.reset();
        m_activeSystemAudioRecordingClip.reset();
        m_recordTargetTracks.clear();
        m_systemAudioRecordTargetTracks.clear();
        refreshMasterAndScale();
        updateUndoRedoButtons();
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
    m_masterWaveform->setPlayheadSample(pos);
}

void MainWindow::updateMeters() {
    m_inputMeter->setLevels(m_engine->inputPeakL(), m_engine->inputPeakR());
    m_outputMeter->setLevels(m_engine->outputPeakL(), m_engine->outputPeakR());
}

void MainWindow::onZoomInClicked() {
    m_zoomFactor = std::min(kMaxZoom, m_zoomFactor * 1.5f);
    refreshMasterAndScale(/*recaptureZoomBaseline=*/true);
    updatePlayhead();
}

void MainWindow::onZoomOutClicked() {
    m_zoomFactor = std::max(kMinZoom, m_zoomFactor / 1.5f);
    refreshMasterAndScale(/*recaptureZoomBaseline=*/true);
    updatePlayhead();
}

void MainWindow::onZoomResetClicked() {
    m_zoomFactor = 1.0f;
    refreshMasterAndScale(/*recaptureZoomBaseline=*/true);
    updatePlayhead();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Wheel) {
        auto* wheelEvent = static_cast<QWheelEvent*>(event);
        if (wheelEvent->modifiers() & Qt::ControlModifier) {
            if (wheelEvent->angleDelta().y() > 0) {
                onZoomInClicked();
            } else if (wheelEvent->angleDelta().y() < 0) {
                onZoomOutClicked();
            }
            return true; // consumed: don't also scroll the timeline
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::drainCaptureRing() {
    float tmp[4096];
    size_t n;

    if (m_activeRecordingClip) {
        while ((n = m_engine->captureRing().read(tmp, 4096)) > 0) {
            auto& samples = m_activeRecordingClip->buffer->samples;
            samples.insert(samples.end(), tmp, tmp + n);
        }
    }

    if (m_activeSystemAudioRecordingClip) {
        while ((n = m_engine->systemAudioCaptureRing().read(tmp, 4096)) > 0) {
            auto& samples = m_activeSystemAudioRecordingClip->buffer->samples;
            samples.insert(samples.end(), tmp, tmp + n);
        }
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

    auto before = m_activeTrack->clipsSnapshot();
    m_activeTrack->addClip(clip);
    m_commandStack.push(
        std::make_unique<TrackClipsCommand>(m_activeTrack, before, m_activeTrack->clipsSnapshot(), "Import"));
    updateStatusLabel();
    refreshWaveformFor(m_activeTrack);
    refreshMasterAndScale();
    updateUndoRedoButtons();
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
    // Silently resave to the known path (Ctrl+S / repeat saves); only prompt
    // the first time or after Close Session cleared it.
    QString path = m_currentSessionPath;
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, "Save Session", QString(),
                                             "Recording Studio Project (*.rsdproj)");
        if (path.isEmpty()) return;
        if (!path.endsWith(".rsdproj")) path += ".rsdproj";
    }

    QVector<LibraryEntry> libraryEntries;
    for (int i = 0; i < m_mediaLibrary->count(); ++i) {
        libraryEntries.append({m_mediaLibrary->nameAt(i), m_mediaLibrary->bufferAt(i)});
    }

    if (!SessionIO::saveSession(path, *m_session, libraryEntries)) {
        QMessageBox::warning(this, "Save Failed", "Could not save session to: " + path);
        return;
    }

    m_currentSessionPath = path;
    addToRecentSessions(path);
    m_statusLabel->setText("Saved to: " + path);
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
    refreshMasterAndScale();

    m_recordAction->setEnabled(true);
    m_playAction->setEnabled(true);
    m_playFromStartAction->setEnabled(true);
}

void MainWindow::onLoadSessionClicked() {
    QString path = QFileDialog::getOpenFileName(this, "Load Session", QString(),
                                                  "Recording Studio Project (*.rsdproj)");
    if (path.isEmpty()) return;
    loadSessionFromPath(path);
}

bool MainWindow::loadSessionFromPath(const QString& path, bool showSuccessMessage) {
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, "Load Failed", "File no longer exists: " + path);
        m_recentSessionPaths.removeAll(path);
        rebuildRecentSessionsMenu();
        return false;
    }

    onStopClicked(); // stop any playback/recording before swapping session state

    QVector<LibraryEntry> libraryEntries;
    if (!SessionIO::loadSession(path, *m_session, libraryEntries)) {
        QMessageBox::warning(this, "Load Failed", "Could not load session from: " + path);
        return false;
    }

    m_commandStack.clear();
    updateUndoRedoButtons();
    m_effectsRack->setSampleRate(m_session->sampleRate);

    m_mediaLibrary->resetLibrary();
    for (auto& entry : libraryEntries) {
        m_mediaLibrary->addEntry(entry.first, entry.second);
    }

    m_currentSessionPath = path;
    rebuildTimelineFromSession();
    addToRecentSessions(path);
    if (showSuccessMessage) QMessageBox::information(this, "Session Loaded", "Loaded: " + path);
    return true;
}

void MainWindow::onCloseSessionClicked() {
    auto reply = QMessageBox::question(
        this, "Close Session",
        "Close the current session and start fresh? Unsaved changes will be lost.",
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (reply != QMessageBox::Yes) return;

    onStopClicked(); // stop any playback/recording before discarding session state

    m_session->tracks.clear();
    m_commandStack.clear();
    updateUndoRedoButtons();
    m_trackCounter = 0;
    m_mediaLibrary->resetLibrary();
    m_currentSessionPath.clear();

    rebuildTimelineFromSession();
    onAddTrackClicked(); // start fresh with one blank track, matching app startup
}

void MainWindow::addToRecentSessions(const QString& path) {
    m_recentSessionPaths.removeAll(path);
    m_recentSessionPaths.prepend(path);
    while (m_recentSessionPaths.size() > kMaxRecentSessions) m_recentSessionPaths.removeLast();

    QSettings settings("RecordingStudio", "RecordingStudio");
    settings.setValue("recentSessions", m_recentSessionPaths);

    rebuildRecentSessionsMenu();
}

void MainWindow::rebuildRecentSessionsMenu() {
    m_recentSessionsMenu->clear();

    if (m_recentSessionPaths.isEmpty()) {
        auto* placeholder = m_recentSessionsMenu->addAction("No Recent Sessions");
        placeholder->setEnabled(false);
        return;
    }

    for (const QString& path : m_recentSessionPaths) {
        auto* action = m_recentSessionsMenu->addAction(QFileInfo(path).fileName());
        action->setToolTip(path);
        connect(action, &QAction::triggered, this, [this, path]() { loadSessionFromPath(path); });
    }
}

void MainWindow::rebuildTimelineFromSession() {
    m_timeline->clear();
    m_activeTrack.reset();
    m_trackWithClipSelection.reset();
    m_deleteClipAction->setEnabled(false);
    m_pianoRollPanel->setBpm(m_session->bpm);
    m_pianoRollPanel->setSampleRate(m_session->sampleRate);

    m_trackCounter = 0;
    for (auto& track : m_session->tracks) {
        m_timeline->addTrack(track);
        if (!m_activeTrack) m_activeTrack = track;
        ++m_trackCounter;
    }

    m_engine->transport().setPositionSamples(0);
    updateStatusLabel();
    refreshMasterAndScale();
    updatePlayhead();
    m_effectsRack->setTrack(m_activeTrack);
}

void MainWindow::onClipMovedToTrack(QUuid clipId, QUuid sourceTrackId, QUuid destTrackId) {
    auto findTrack = [&](const QUuid& id) -> std::shared_ptr<Track> {
        for (auto& t : m_session->tracks) {
            if (t->id == id) return t;
        }
        return nullptr;
    };
    auto sourceTrack = findTrack(sourceTrackId);
    auto destTrack = findTrack(destTrackId);
    if (!sourceTrack || !destTrack) return;

    std::shared_ptr<Clip> movedClip;
    for (auto& c : *sourceTrack->clipsSnapshot()) {
        if (c->id == clipId) { movedClip = c; break; }
    }
    if (!movedClip) return;

    auto sourceBefore = sourceTrack->clipsSnapshot();
    auto destBefore = destTrack->clipsSnapshot();

    destTrack->addClip(std::make_shared<Clip>(*movedClip));
    sourceTrack->removeClip(clipId);
    m_timeline->clearSelectionOn(sourceTrackId);

    std::vector<std::unique_ptr<Command>> subCommands;
    subCommands.push_back(std::make_unique<TrackClipsCommand>(
        sourceTrack, sourceBefore, sourceTrack->clipsSnapshot(), "Move Clip"));
    subCommands.push_back(
        std::make_unique<TrackClipsCommand>(destTrack, destBefore, destTrack->clipsSnapshot(), "Move Clip"));
    m_commandStack.push(std::make_unique<CompositeCommand>(std::move(subCommands), "Move Clip"));

    refreshWaveformFor(sourceTrack);
    refreshWaveformFor(destTrack);
    refreshMasterAndScale();
    updateUndoRedoButtons();
}

void MainWindow::onMediaDroppedOnTrack(QUuid trackId, int libraryIndex, int64_t sessionStartSample) {
    std::shared_ptr<Track> targetTrack;
    for (auto& t : m_session->tracks) {
        if (t->id == trackId) { targetTrack = t; break; }
    }
    if (!targetTrack) return;

    auto buffer = m_mediaLibrary->bufferAt(libraryIndex);
    if (!buffer) return;

    auto clip = std::make_shared<Clip>();
    clip->buffer = buffer;
    clip->name = m_mediaLibrary->nameAt(libraryIndex);
    clip->sessionStartSample = std::max<int64_t>(0, sessionStartSample);
    clip->sourceOffsetSamples = 0;
    clip->lengthSamples = buffer->frameCount();

    auto before = targetTrack->clipsSnapshot();
    targetTrack->addClip(clip);
    m_commandStack.push(std::make_unique<TrackClipsCommand>(
        targetTrack, before, targetTrack->clipsSnapshot(), "Drop Media"));
    refreshWaveformFor(targetTrack);
    refreshMasterAndScale();
    updateUndoRedoButtons();
}

void MainWindow::onUndoClicked() {
    if (!m_commandStack.canUndo()) return;
    onStopClicked(); // don't mutate session state while the audio thread is reading it
    m_commandStack.undo();
    rebuildTimelineFromSession();
    updateUndoRedoButtons();
}

void MainWindow::onRedoClicked() {
    if (!m_commandStack.canRedo()) return;
    onStopClicked();
    m_commandStack.redo();
    rebuildTimelineFromSession();
    updateUndoRedoButtons();
}

void MainWindow::updateUndoRedoButtons() {
    m_undoAction->setEnabled(m_commandStack.canUndo());
    m_redoAction->setEnabled(m_commandStack.canRedo());
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

std::shared_ptr<AudioBuffer> MainWindow::renderSessionToBuffer() const {
    bool anySoloed = false;
    for (auto& track : m_session->tracks) {
        if (track->soloed.load()) { anySoloed = true; break; }
    }

    int64_t totalFrames = 0;
    for (auto& track : m_session->tracks) {
        for (auto& clip : *track->clipsSnapshot()) {
            totalFrames = std::max(totalFrames, clip->sessionStartSample + clip->lengthSamples);
        }
    }

    auto out = std::make_shared<AudioBuffer>();
    out->channels = m_session->channels;
    out->sampleRate = m_session->sampleRate;
    out->samples.assign(static_cast<size_t>(totalFrames) * out->channels, 0.0f);

    for (auto& track : m_session->tracks) {
        bool soloed = track->soloed.load();
        bool muted = track->muted.load();
        bool audible = anySoloed ? soloed : !muted;
        if (!audible) continue;

        // Static volume/pan only — this offline render predates automation
        // and isn't part of its v1 scope (see PLAN.md); playback via the RT
        // engine is where automation curves actually apply.
        auto [gainL, gainR] = panToGains(track->volume.load(), track->pan.load());

        for (auto& clip : *track->clipsSnapshot()) {
            if (clip->muted || !clip->buffer) continue;
            for (int64_t i = 0; i < clip->lengthSamples; ++i) {
                int64_t sourceFrame = clip->sourceOffsetSamples + i;
                if (sourceFrame < 0 || sourceFrame >= clip->buffer->frameCount()) continue;
                int64_t destFrame = clip->sessionStartSample + i;

                for (int ch = 0; ch < out->channels; ++ch) {
                    int srcCh = ch % clip->buffer->channels;
                    float g = (ch % 2 == 0) ? gainL : gainR;
                    out->samples[destFrame * out->channels + ch] +=
                        clip->buffer->samples[sourceFrame * clip->buffer->channels + srcCh] * g;
                }
            }
        }
    }

    return out;
}

void MainWindow::refreshWaveformFor(const std::shared_ptr<Track>& track) {
    if (!track) return;
    m_timeline->refreshTrackWaveform(track->id);
}

void MainWindow::refreshMasterAndScale(bool recaptureZoomBaseline) {
    int64_t total = refreshTimelineScale(recaptureZoomBaseline);
    // Must match the scale just pushed to the ruler/lanes, or the master
    // strip's audio-populated region won't line up with where the tracks'
    // own clips actually sit (the bug this fixes: audio appeared to exist
    // under the playhead in the master strip when the tracks were empty
    // there, because the strip was auto-fitting its own shorter duration to
    // the full widget width instead of sharing this scale).
    m_masterWaveform->setTimelineLength(total);
    m_masterWaveform->setBuffer(renderSessionToBuffer());
    m_mediaLibrary->refresh(*m_session);
}

int64_t MainWindow::refreshTimelineScale(bool recaptureZoomBaseline) {
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
    // m_zoomFactor > 1 shows fewer seconds across the same widget width (zoomed
    // in); content past the visible window is simply not drawn — there's no
    // horizontal scrolling, so zooming in trades overview for detail.
    //
    // The content extent (maxEnd) is only used to *grow* m_zoomBaseSamples,
    // never to recompute it from scratch on every refresh — otherwise an
    // incidental clip edit (e.g. dragging a clip onto another track) would
    // silently stretch/shrink whatever zoom level the user dialed in. See
    // TimelineScaleMath.h.
    auto scale = computeTimelineScale(maxEnd, floor, headroom, m_zoomFactor, m_zoomBaseSamples,
                                       recaptureZoomBaseline);
    m_zoomBaseSamples = scale.pinnedBaseSamples;

    m_timeline->setSharedTimelineLength(scale.totalSamples);
    // Lets each track's own scrollbar know how far it's allowed to pan; the
    // ruler and master strip intentionally stay fixed to [0, totalSamples)
    // regardless of any track's scroll position (see TrackWidgets.cpp).
    m_timeline->setContentExtentSamples(scale.pinnedBaseSamples);
    m_ruler->setTimelineLength(scale.totalSamples);
    return scale.totalSamples;
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
