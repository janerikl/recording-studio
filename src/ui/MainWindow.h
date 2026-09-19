#pragma once

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <memory>

#include "audio/AudioEngine.h"
#include "model/Session.h"

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
    void drainCaptureRing();

private:
    void updateStatusLabel();
    std::shared_ptr<AudioBuffer> renderTrackToBuffer(const Track& track) const;

    std::unique_ptr<AudioEngine> m_engine;
    std::unique_ptr<Session> m_session;
    std::shared_ptr<Track> m_track;
    std::shared_ptr<Clip> m_activeRecordingClip;

    QPushButton* m_recordButton = nullptr;
    QPushButton* m_playButton = nullptr;
    QPushButton* m_stopButton = nullptr;
    QLabel* m_statusLabel = nullptr;
    QTimer* m_ringDrainTimer = nullptr;
};

} // namespace rsd
