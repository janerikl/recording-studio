#pragma once

#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

namespace rsd {

// RtAudio's PulseAudio backend doesn't enumerate PipeWire ".monitor"
// sources (it only lists real sink/source nodes), so there is nothing to
// pick as a "system audio" device out of the box. This spawns `pw-loopback`
// to capture the default sink's monitor and re-expose it as a plain
// Audio/Source node under a fixed, recognizable name — which RtAudio *does*
// enumerate like any other input device, so it shows up in SettingsDialog's
// "System Audio Device" dropdown with no RtAudio-side changes needed.
class SystemAudioLoopback {
public:
    static constexpr const char* kNodeName = "rsd_sysaudio_src";
    static constexpr const char* kNodeDescription = "Recording Studio System Audio";

    // Exposed for testing without spawning a real process.
    static QStringList buildArgs() {
        return {"-C", "@DEFAULT_SINK@",
                "--playback-props",
                QString("media.class=Audio/Source node.name=%1 node.description=\"%2\"")
                    .arg(kNodeName, kNodeDescription)};
    }

    ~SystemAudioLoopback() { stop(); }

    // No-op if pw-loopback isn't on PATH, or already running. Fire-and-forget:
    // callers don't need the device to exist immediately (SettingsDialog's
    // device list is only read when the user opens it, by which point this
    // has had time to register with PipeWire).
    bool start() {
        if (m_process.state() != QProcess::NotRunning) return true;
        QString exe = QStandardPaths::findExecutable("pw-loopback");
        if (exe.isEmpty()) return false;
        m_process.start(exe, buildArgs());
        return m_process.waitForStarted(2000);
    }

    void stop() {
        if (m_process.state() == QProcess::NotRunning) return;
        m_process.terminate();
        if (!m_process.waitForFinished(1000)) m_process.kill();
    }

private:
    QProcess m_process;
};

} // namespace rsd
