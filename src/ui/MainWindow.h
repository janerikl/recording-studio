#pragma once

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <memory>

#include "audio/AudioEngine.h"
#include "model/Session.h"
#include "ui/TimelineView.h"
#include "ui/TimeRulerWidget.h"

namespace rsd {

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
    void onAddTrackClicked();
    void onRemoveTrackClicked();
    void onTrackSelected(std::shared_ptr<Track> track);
    void onClipSelectionChanged(std::shared_ptr<Track> track, bool hasSelection);
    void onDeleteClipClicked();
    void onSeekRequested(int64_t sample);
    void drainCaptureRing();
    void updatePlayhead();

private:
    void updateStatusLabel();
    void refreshWaveformFor(const std::shared_ptr<Track>& track);
    void refreshTimelineScale();
    void startPlayback();
    void rebuildTimelineFromSession();
    std::shared_ptr<AudioBuffer> renderTrackToBuffer(const Track& track) const;

    std::unique_ptr<AudioEngine> m_engine;
    std::unique_ptr<Session> m_session;
    std::shared_ptr<Track> m_activeTrack;
    std::shared_ptr<Clip> m_activeRecordingClip;
    std::shared_ptr<Track> m_trackWithClipSelection;

    QPushButton* m_recordButton = nullptr;
    QPushButton* m_playButton = nullptr;
    QPushButton* m_playFromStartButton = nullptr;
    QPushButton* m_stopButton = nullptr;
    QPushButton* m_deleteClipButton = nullptr;
    QLabel* m_statusLabel = nullptr;
    QTimer* m_ringDrainTimer = nullptr;
    QTimer* m_playheadTimer = nullptr;
    TimelineView* m_timeline = nullptr;
    TimeRulerWidget* m_ruler = nullptr;
    int m_trackCounter = 0;
};

} // namespace rsd
