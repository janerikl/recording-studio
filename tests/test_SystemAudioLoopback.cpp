#include <QtTest>

#include "audio/SystemAudioLoopback.h"

using namespace rsd;

class TestSystemAudioLoopback : public QObject {
    Q_OBJECT

private slots:
    void argsCaptureDefaultSinkMonitor() {
        auto args = SystemAudioLoopback::buildArgs();
        QVERIFY(args.contains("@DEFAULT_SINK@"));
    }

    void argsExposeAFixedRecognizableSourceName() {
        auto args = SystemAudioLoopback::buildArgs();
        QString playbackProps = args.last();
        QVERIFY(playbackProps.contains("media.class=Audio/Source"));
        QVERIFY(playbackProps.contains(QString("node.name=%1").arg(SystemAudioLoopback::kNodeName)));
    }
};

QTEST_MAIN(TestSystemAudioLoopback)
#include "test_SystemAudioLoopback.moc"
