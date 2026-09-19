#include "MainWindow.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>

#include "io/AudioFileIO.h"

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
    m_stopButton = new QPushButton("Stop", buttonRow);
    auto* importButton = new QPushButton("Import...", buttonRow);
    auto* exportButton = new QPushButton("Export...", buttonRow);
    auto* addTrackButton = new QPushButton("Add Track", buttonRow);
    auto* removeTrackButton = new QPushButton("Remove Track", buttonRow);
    m_deleteClipButton = new QPushButton("Delete Selected Clip", buttonRow);
    m_stopButton->setEnabled(false);
    m_deleteClipButton->setEnabled(false);

    connect(m_recordButton, &QPushButton::clicked, this, &MainWindow::onRecordClicked);
    connect(m_playButton, &QPushButton::clicked, this, &MainWindow::onPlayClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(importButton, &QPushButton::clicked, this, &MainWindow::onImportClicked);
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::onExportClicked);
    connect(addTrackButton, &QPushButton::clicked, this, &MainWindow::onAddTrackClicked);
    connect(removeTrackButton, &QPushButton::clicked, this, &MainWindow::onRemoveTrackClicked);
    connect(m_deleteClipButton, &QPushButton::clicked, this, &MainWindow::onDeleteClipClicked);

    buttonLayout->addWidget(m_recordButton);
    buttonLayout->addWidget(m_playButton);
    buttonLayout->addWidget(m_stopButton);
    buttonLayout->addWidget(importButton);
    buttonLayout->addWidget(exportButton);
    buttonLayout->addWidget(addTrackButton);
    buttonLayout->addWidget(removeTrackButton);
    buttonLayout->addWidget(m_deleteClipButton);
    layout->addWidget(buttonRow);

    m_statusLabel = new QLabel("Stopped — 0 tracks, 0 clips", central);
    layout->addWidget(m_statusLabel);

    m_timeline = new TimelineView(central);
    connect(m_timeline, &TimelineView::trackSelected, this, &MainWindow::onTrackSelected);
    connect(m_timeline, &TimelineView::clipSelectionChanged, this,
            &MainWindow::onClipSelectionChanged);
    layout->addWidget(m_timeline, 1);

    setCentralWidget(central);

    m_ringDrainTimer = new QTimer(this);
    m_ringDrainTimer->setInterval(30);
    connect(m_ringDrainTimer, &QTimer::timeout, this, &MainWindow::drainCaptureRing);

    if (!m_engine->start()) {
        m_statusLabel->setText("Failed to start audio engine — check console");
        m_recordButton->setEnabled(false);
        m_playButton->setEnabled(false);
    }

    onAddTrackClicked(); // start with one track
}

void MainWindow::onAddTrackClicked() {
    ++m_trackCounter;
    auto track = m_session->addTrack(QString("Track %1").arg(m_trackCounter));
    m_timeline->addTrack(track);
    if (!m_activeTrack) m_activeTrack = track;
    updateStatusLabel();
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
}

void MainWindow::onRecordClicked() {
    if (!m_activeTrack) {
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

    m_recordButton->setEnabled(false);
    m_playButton->setEnabled(false);
    m_stopButton->setEnabled(true);
    m_statusLabel->setText("Recording into " + m_activeTrack->name + "...");
}

void MainWindow::onPlayClicked() {
    m_engine->transport().setPositionSamples(0);
    m_engine->transport().setState(TransportState::Playing);

    m_recordButton->setEnabled(false);
    m_playButton->setEnabled(false);
    m_stopButton->setEnabled(true);
    m_statusLabel->setText("Playing...");
}

void MainWindow::onStopClicked() {
    const bool wasRecording = m_engine->transport().state() == TransportState::Recording;
    m_engine->transport().setState(TransportState::Stopped);
    m_ringDrainTimer->stop();

    if (wasRecording && m_activeRecordingClip && m_activeTrack) {
        drainCaptureRing(); // flush any remaining samples
        m_activeRecordingClip->lengthSamples = m_activeRecordingClip->buffer->frameCount();
        m_activeTrack->addClip(m_activeRecordingClip);
        refreshWaveformFor(m_activeTrack);
        m_activeRecordingClip.reset();
    }

    m_recordButton->setEnabled(true);
    m_playButton->setEnabled(true);
    m_stopButton->setEnabled(false);
    updateStatusLabel();
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

void MainWindow::updateStatusLabel() {
    int totalClips = 0;
    for (auto& track : m_session->tracks) {
        totalClips += static_cast<int>(track->clipsSnapshot()->size());
    }
    m_statusLabel->setText(
        QString("Stopped — %1 track(s), %2 clip(s)").arg(m_session->tracks.size()).arg(totalClips));
}

} // namespace rsd
