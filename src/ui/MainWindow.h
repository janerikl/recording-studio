#pragma once

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <memory>

#include "audio/AudioEngine.h"
#include "model/Session.h"
#include "ui/TimelineView.h"

namespace rsd {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onRecordClicked();
    void onPlayClicked();
    void onStopClicked();
    void onImportClicked();
    void onExportClicked();
    void onAddTrackClicked();
    void onRemoveTrackClicked();
    void onTrackSelected(std::shared_ptr<Track> track);
    void onClipSelectionChanged(std::shared_ptr<Track> track, bool hasSelection);
    void onDeleteClipClicked();
    void drainCaptureRing();

private:
    void updateStatusLabel();
    void refreshWaveformFor(const std::shared_ptr<Track>& track);
    std::shared_ptr<AudioBuffer> renderTrackToBuffer(const Track& track) const;

    std::unique_ptr<AudioEngine> m_engine;
    std::unique_ptr<Session> m_session;
    std::shared_ptr<Track> m_activeTrack;
    std::shared_ptr<Clip> m_activeRecordingClip;
    std::shared_ptr<Track> m_trackWithClipSelection;

    QPushButton* m_recordButton = nullptr;
    QPushButton* m_playButton = nullptr;
    QPushButton* m_stopButton = nullptr;
    QPushButton* m_deleteClipButton = nullptr;
    QLabel* m_statusLabel = nullptr;
    QTimer* m_ringDrainTimer = nullptr;
    TimelineView* m_timeline = nullptr;
    int m_trackCounter = 0;
};

} // namespace rsd
