#pragma once

#include <QAction>
#include <QCheckBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QIcon>
#include <QMainWindow>
#include <QLabel>
#include <QMenu>
#include <QStringList>
#include <QTimer>
#include <QToolBar>
#include <memory>
#include <vector>

#include "audio/AudioEngine.h"
#include "audio/PunchRegion.h"
#include "audio/SystemAudioLoopback.h"
#include "command/CommandStack.h"
#include "model/Session.h"
#include "ui/LevelMeterWidget.h"
#include "ui/TimelineView.h"
#include "ui/TimeRulerWidget.h"
#include "ui/SettingsDialog.h"
#include "ui/ShortcutManager.h"
#include "ui/WaveformWidget.h"
#include "ui/MediaBrowserPanel.h"
#include "ui/EffectsPopoverWidget.h"
#include "ui/InstrumentPanel.h"
#include "ui/PianoRollPanel.h"
#include "ui/ExportDialog.h"
#include "ui/MixerPanel.h"

#include <unordered_map>

class QCloseEvent;

namespace rsd {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onRecordClicked();
    void onPlayClicked();
    void onPlayFromStartClicked();
    void onStopClicked();
    void onImportClicked();
    void onExportClicked();
    void onSaveSessionClicked();
    void onLoadSessionClicked();
    void onCloseSessionClicked();
    void onSettingsClicked();
    void onUsageGuideClicked();
    void onAddTrackClicked();
    void onAddInstrumentTrackClicked();
    void onAddBusTrackClicked();
    void onRemoveTrackClicked();
    void onTrackSelected(std::shared_ptr<Track> track);
    void onSetMarker(int slot);
    void onJumpToMarker(int slot);
    void onLoopRegionSet(int64_t startSample, int64_t endSample, bool enable);
    void onSelectTrackByIndex(int index);
    void onEffectsPanelRequested(std::shared_ptr<Track> track, QRect globalAnchorRect);
    void onMasterEffectsPanelRequested(QRect globalAnchorRect);
    void onInstrumentNoteOn(int pitch, float velocity);
    void onInstrumentNoteOff(int pitch);
    void onTakeSelected(std::shared_ptr<Track> track, std::shared_ptr<Clip> take);
    void onClipSelectionChanged(std::shared_ptr<Track> track, bool hasSelection);
    void onDeleteClipClicked();
    void onSeekRequested(int64_t sample);
    void onClipMovedToTrack(QUuid clipId, QUuid sourceTrackId, QUuid destTrackId);
    void onMediaDroppedOnTrack(QUuid trackId, int libraryIndex, int64_t sessionStartSample);
    void onExternalFileDroppedOnTrack(QUuid trackId, QString filePath, int64_t sessionStartSample);
    void onMediaBrowserPreviewRequested(LibraryItem item);
    void onSaveToLoopBrowserRequested(std::shared_ptr<Track> track);
    void onUndoClicked();
    void onRedoClicked();
    void drainCaptureRing();
    void updatePlayhead();
    void updateMeters();
    void onZoomInClicked();
    void onZoomOutClicked();
    void onZoomResetClicked();
    void onPunchRegionEditedOnRuler(PunchRegion region);
    void onPunchFieldsChanged();

private:
    void updateStatusLabel();
    void refreshWaveformFor(const std::shared_ptr<Track>& track);
    int64_t refreshTimelineScale(bool recaptureZoomBaseline = false);
    int64_t sessionContentEndSamples() const;
    void refreshMasterAndScale(bool recaptureZoomBaseline = false);
    void startPlayback();
    void rebuildTimelineFromSession();
    bool loadSessionFromPath(const QString& path, bool showSuccessMessage = true);
    void addToRecentSessions(const QString& path);
    void rebuildRecentSessionsMenu();
    void updateUndoRedoButtons();
    static QIcon recordIcon();
    std::shared_ptr<AudioBuffer> renderSessionToBuffer() const;

    std::unique_ptr<AudioEngine> m_engine;
    SystemAudioLoopback m_systemAudioLoopback;
    std::unique_ptr<Session> m_session;
    // Remembered from the last successful save/load, so Ctrl+S/Save resaves
    // silently to the same file instead of re-prompting every time.
    QString m_currentSessionPath;
    QMenu* m_recentSessionsMenu = nullptr;
    QStringList m_recentSessionPaths;
    static constexpr int kMaxRecentSessions = 5;
    std::shared_ptr<Track> m_activeTrack;
    std::shared_ptr<Clip> m_activeRecordingClip;
    std::vector<std::shared_ptr<Track>> m_recordTargetTracks;
    // System-audio counterparts: a separate clip/target list fed by the
    // engine's second (system-audio) capture ring, so a Mic-armed track and
    // a SystemAudio-armed track record onto independent clips in the same
    // pass. See RecordRouting.h for how armed tracks get split between them.
    std::shared_ptr<Clip> m_activeSystemAudioRecordingClip;
    std::vector<std::shared_ptr<Track>> m_systemAudioRecordTargetTracks;
    // True while the in-progress recording is punch/loop mode (single target
    // track, gated to the punch region, one undo entry on stop) rather than
    // the plain whole-transport recording path.
    bool m_punchRecordingActive = false;
    static constexpr double kPunchPreRollSeconds = 2.0;
    std::shared_ptr<Track> m_trackWithClipSelection;
    // MIDI note capture while an Instrument track is armed+recording:
    // GUI-thread-only bookkeeping (pitch -> start sample when the on-screen
    // key went down), finalized into completed MidiNotes on note-up, then
    // pushed to the track's midiClips as one undo step on Stop. Deliberately
    // separate from the RT-thread live-audition NoteEventQueue.
    std::unordered_map<int, int64_t> m_pendingNoteStarts;
    std::vector<std::shared_ptr<MidiNote>> m_pendingRecordedNotes;

    QAction* m_recordAction = nullptr;
    QAction* m_playAction = nullptr;
    QAction* m_playFromStartAction = nullptr;
    QAction* m_stopAction = nullptr;
    QAction* m_deleteClipAction = nullptr;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QAction* m_addTrackAction = nullptr;
    QAction* m_addInstrumentTrackAction = nullptr;
    QAction* m_addBusTrackAction = nullptr;
    QAction* m_removeTrackAction = nullptr;
    QAction* m_zoomInAction = nullptr;
    QAction* m_zoomOutAction = nullptr;
    QAction* m_zoomResetAction = nullptr;
    ShortcutManager m_shortcutManager;
    // >1 = zoomed in (fewer seconds visible, clips appear wider); clamped to
    // a sane range. Applied on top of the content-based floor/headroom scale
    // in refreshTimelineScale().
    float m_zoomFactor = 1.0f;
    static constexpr float kMinZoom = 0.25f;
    static constexpr float kMaxZoom = 8.0f;
    // Content-extent baseline pinned by refreshTimelineScale(); only grows
    // when content genuinely exceeds it, so incidental clip edits (e.g.
    // dragging a clip to another track) don't stretch/shrink a zoom level
    // the user dialed in. See TimelineScaleMath.h.
    int64_t m_zoomBaseSamples = 0;
    CommandStack m_commandStack;
    QLabel* m_statusLabel = nullptr;
    QTimer* m_ringDrainTimer = nullptr;
    QTimer* m_playheadTimer = nullptr;
    QTimer* m_meterTimer = nullptr;
    TimelineView* m_timeline = nullptr;
    TimeRulerWidget* m_ruler = nullptr;
    QCheckBox* m_loopRecordCheckBox = nullptr;
    QDoubleSpinBox* m_punchInSpin = nullptr;
    QDoubleSpinBox* m_punchOutSpin = nullptr;
    QDoubleSpinBox* m_bpmSpin = nullptr;
    QCheckBox* m_metronomeCheckBox = nullptr;
    WaveformWidget* m_masterWaveform = nullptr;
    MediaBrowserPanel* m_mediaBrowser = nullptr;
    EffectsPopoverWidget* m_effectsPopover = nullptr;
    InstrumentPanel* m_instrumentPanel = nullptr;
    QDockWidget* m_instrumentDock = nullptr;
    PianoRollPanel* m_pianoRollPanel = nullptr;
    MixerPanel* m_mixer = nullptr;
    QDockWidget* m_pianoRollDock = nullptr;
    LevelMeterWidget* m_inputMeter = nullptr;
    LevelMeterWidget* m_outputMeter = nullptr;
    int m_trackCounter = 0;
    int m_busCounter = 0;
};

} // namespace rsd
