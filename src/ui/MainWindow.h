#pragma once

#include <QAction>
#include <QIcon>
#include <QMainWindow>
#include <QLabel>
#include <QTimer>
#include <QToolBar>
#include <memory>
#include <vector>

#include "audio/AudioEngine.h"
#include "model/Session.h"
#include "ui/LevelMeterWidget.h"
#include "ui/TimelineView.h"
#include "ui/TimeRulerWidget.h"
#include "ui/SettingsDialog.h"

namespace rsd {

// Lightweight snapshot of editable session state (not audio sample data,
// which is immutable and shared by pointer) used for undo/redo.
struct ClipSnapshot {
    QUuid id;
    std::shared_ptr<AudioBuffer> buffer;
    int64_t sessionStartSample = 0;
    int64_t sourceOffsetSamples = 0;
    int64_t lengthSamples = 0;
    QString name;
    bool muted = false;
};

struct TrackSnapshot {
    QUuid id;
    QString name;
    float gain = 1.0f;
    bool muted = false;
    bool soloed = false;
    bool recordArmed = false;
    std::vector<ClipSnapshot> clips;
};

using SessionSnapshot = std::vector<TrackSnapshot>;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onRecordClicked();
    void onPlayClicked();
    void onPlayFromStartClicked();
    void onStopClicked();
    void onImportClicked();
    void onExportClicked();
    void onSaveSessionClicked();
    void onLoadSessionClicked();
    void onSettingsClicked();
    void onAddTrackClicked();
    void onRemoveTrackClicked();
    void onTrackSelected(std::shared_ptr<Track> track);
    void onClipSelectionChanged(std::shared_ptr<Track> track, bool hasSelection);
    void onDeleteClipClicked();
    void onSeekRequested(int64_t sample);
    void onClipEditStarted();
    void onUndoClicked();
    void onRedoClicked();
    void drainCaptureRing();
    void updatePlayhead();
    void updateMeters();

private:
    void updateStatusLabel();
    void refreshWaveformFor(const std::shared_ptr<Track>& track);
    void refreshTimelineScale();
    void startPlayback();
    void rebuildTimelineFromSession();
    SessionSnapshot captureSnapshot() const;
    void restoreSnapshot(const SessionSnapshot& snapshot);
    void pushUndoSnapshot();
    void updateUndoRedoButtons();
    static QIcon recordIcon();
    std::shared_ptr<AudioBuffer> renderTrackToBuffer(const Track& track) const;

    std::unique_ptr<AudioEngine> m_engine;
    std::unique_ptr<Session> m_session;
    std::shared_ptr<Track> m_activeTrack;
    std::shared_ptr<Clip> m_activeRecordingClip;
    std::vector<std::shared_ptr<Track>> m_recordTargetTracks;
    std::shared_ptr<Track> m_trackWithClipSelection;

    QAction* m_recordAction = nullptr;
    QAction* m_playAction = nullptr;
    QAction* m_playFromStartAction = nullptr;
    QAction* m_stopAction = nullptr;
    QAction* m_deleteClipAction = nullptr;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QAction* m_addTrackAction = nullptr;
    QAction* m_removeTrackAction = nullptr;
    std::vector<SessionSnapshot> m_undoStack;
    std::vector<SessionSnapshot> m_redoStack;
    static constexpr size_t kMaxUndoDepth = 50;
    QLabel* m_statusLabel = nullptr;
    QTimer* m_ringDrainTimer = nullptr;
    QTimer* m_playheadTimer = nullptr;
    QTimer* m_meterTimer = nullptr;
    TimelineView* m_timeline = nullptr;
    TimeRulerWidget* m_ruler = nullptr;
    LevelMeterWidget* m_inputMeter = nullptr;
    LevelMeterWidget* m_outputMeter = nullptr;
    int m_trackCounter = 0;
};

} // namespace rsd
