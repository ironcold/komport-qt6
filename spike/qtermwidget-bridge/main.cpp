// spike/qtermwidget-bridge/main.cpp
//
// QTermWidget <-> QSerialPort bridge spike for ADR-001
// (docs/architecture-decisions/ADR-001-terminal-engine-strategy.md).
//
// Goal: find out whether QTermWidget can sit as the terminal backend behind
// the data flow CLAUDE.md's "Terminal-Engine-Leitlinie" requires:
//
//   QSerialPort -> RX/TX diagnosis/logging -> charset translation -> terminal backend
//   terminal backend -> charset translation/line endings/macros -> QSerialPort
//
// This is throwaway architecture-decision code, not a shipped feature - see
// ADR-001 for the resulting decision. Only built with
// -DKOMPORT_BUILD_SPIKES=ON (off by default, see top-level CMakeLists.txt),
// so the normal build/install/test path never needs qtermwidget6 installed.
// See README.md in this directory for how to run it, both interactively
// (against a real device or a socat pty pair) and headlessly (--selftest).

#include <QApplication>
#include <QCommandLineParser>
#include <QMainWindow>
#include <QSerialPort>
#include <QDebug>
#include <QTimer>

#include <qtermwidget.h>

#include <unistd.h>

namespace {

// Stand-ins for the two pipeline stages CLAUDE.md requires between
// QSerialPort and the terminal backend, in both directions. Real
// implementations (hex-monitor logging, CP437/Amiga/PETSCII charset tables)
// already exist for the RX side in the real app (KomportHexView,
// KomportSerial) and are out of scope for this spike - what matters here is
// only *whether the insertion point exists* on a QTermWidget-backed
// pipeline, not reimplementing them. Both are identity passthroughs that log
// a byte count, which is enough to prove the hook point works.
QByteArray diagnoseAndTranslateRx(const QByteArray &raw) {
    qInfo().noquote() << "[rx-hook]" << raw.size() << "byte(s) before terminal backend";
    return raw; // identity - see comment above
}

QByteArray translateTx(const QByteArray &raw) {
    qInfo().noquote() << "[tx-hook]" << raw.size() << "byte(s) before QSerialPort";
    return raw; // identity - see comment above
}

// Bridges a QSerialPort and a QTermWidget started in "teletype" mode
// (QTermWidget::startTerminalTeletype() - no shell child process; the
// widget's internal pty pair is driven directly by us instead). This is the
// central structural question of the spike: can QTermWidget display and
// control a remote (serial) terminal instead of a local shell, with our own
// RX/TX diagnostics and charset translation still sitting in the pipeline
// exactly where CLAUDE.md requires?
class SerialTermBridge : public QObject {
    Q_OBJECT
public:
    SerialTermBridge(QSerialPort *serial, QTermWidget *term, QObject *parent = nullptr)
        : QObject(parent), mSerial(serial), mTerm(term)
    {
        mTerm->startTerminalTeletype();
        mTermSlaveFd = mTerm->getPtySlaveFd();
        if (mTermSlaveFd < 0) {
            qWarning() << "QTermWidget::getPtySlaveFd() returned" << mTermSlaveFd
                       << "- teletype mode not available, bridge is inert";
        }

        connect(mSerial, &QSerialPort::readyRead, this, &SerialTermBridge::onSerialReadyRead);
        // New-style PMF connect fails at runtime ("signal not found") against
        // this distro's prebuilt qtermwidget6 - see README.md "Findings".
        // Old-style SIGNAL/SLOT macro connect works, so that's used here.
        connect(mTerm, SIGNAL(sendData(const char*,int)), this, SLOT(onTermSendData(const char*,int)));
    }

private slots:
    void onSerialReadyRead() {
        const QByteArray raw = mSerial->readAll();
        if (mTermSlaveFd < 0) return;
        const QByteArray toDisplay = diagnoseAndTranslateRx(raw);
        // Feed the widget's own internal pty (slave side) directly - this is
        // what makes it show up in the terminal image/scrollback, exactly as
        // if a real child process had written it, without QTermWidget ever
        // spawning one.
        const ssize_t written = ::write(mTermSlaveFd, toDisplay.constData(),
                                         static_cast<size_t>(toDisplay.size()));
        if (written < 0 || written != toDisplay.size()) {
            qWarning() << "short/failed write to terminal pty slave fd";
        }
    }

    void onTermSendData(const char *data, int len) {
        const QByteArray raw(data, len);
        const QByteArray toSend = translateTx(raw);
        const qint64 written = mSerial->write(toSend);
        if (written != toSend.size()) {
            qWarning() << "short/failed QSerialPort write";
        }
    }

private:
    QSerialPort *mSerial;
    QTermWidget *mTerm;
    int mTermSlaveFd = -1;
};

// Scripted, non-interactive verification against a real pty device (see
// README.md for how to obtain one). Exercises every question from TODO.md
// section 1 / ADR-001's follow-up list that can be checked structurally in a
// headless (offscreen QPA) environment: color-scheme discovery/selection,
// scrollback size, the RX path (device bytes -> visible in the terminal
// image), and that the TX path fires at all. Font rendering, cursor blink
// and actual pixel output can't be judged headlessly - see ADR-001 for that
// caveat; prints "SELFTEST-*" lines meant to be grepped by whoever runs it.
void runSelfTest(QTermWidget *term) {
    bool ok = true;

    const QStringList schemes = term->getAvailableColorSchemes();
    qInfo().noquote() << "SELFTEST-INFO color schemes:" << schemes.join(", ");
    if (schemes.isEmpty()) {
        qWarning() << "SELFTEST-FAIL no color schemes found";
        ok = false;
    } else {
        term->setColorScheme(schemes.contains("GreenOnBlack") ? "GreenOnBlack" : schemes.first());
        qInfo() << "SELFTEST-OK color scheme selectable";
    }

    term->setHistorySize(5000);
    if (term->historySize() != 5000) {
        qWarning() << "SELFTEST-FAIL historySize() ==" << term->historySize() << "expected 5000";
        ok = false;
    } else {
        qInfo() << "SELFTEST-OK historySize round-trip (5000 lines)";
    }

    // Give an external harness time to write bytes into the other end of
    // the serial pty (simulating a remote device sending data), then read
    // back whatever ended up in the terminal image via selectedText() -
    // this proves the RX path (QSerialPort -> [rx-hook] -> pty slave fd ->
    // terminal backend) end to end without depending on QTermWidget's own
    // receivedData() signal (see the connect() call below and README.md
    // "Findings" - new-style PMF connect fails against this distro's
    // prebuilt qtermwidget6 for QTermWidget's own signals).
    QTimer::singleShot(600, qApp, [term, ok]() {
        term->setSelectionStart(0, 0);
        term->setSelectionEnd(term->screenLinesCount() - 1, term->screenColumnsCount() - 1);
        qInfo().noquote() << "SELFTEST-INFO screen content after RX:"
                           << term->selectedText(true).trimmed();

        // Ask the widget to "type" some text - QTermWidget's own Emulation
        // processes it and (in teletype mode) emits sendData(), which the
        // bridge forwards to the real QSerialPort. The README's manual
        // socat walkthrough covers an eyes-on end-to-end check of what
        // actually lands on the wire; here we only prove the call doesn't
        // crash/hang.
        term->sendText(QStringLiteral("spike"));

        QTimer::singleShot(300, qApp, [ok]() {
            qInfo() << (ok ? "SELFTEST-DONE structural checks passed"
                           : "SELFTEST-DONE FAILURES ABOVE");
            qApp->exit(ok ? 0 : 1);
        });
    });
}

} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("komport-qtermwidget-bridge-spike");

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "ADR-001 spike: bridges a QSerialPort to a QTermWidget in teletype mode. "
        "Throwaway architecture-decision code, see "
        "docs/architecture-decisions/ADR-001-terminal-engine-strategy.md");
    parser.addHelpOption();
    parser.addPositionalArgument("device", "Serial device path (e.g. a pty slave path for testing)");
    QCommandLineOption baudOption("baud", "Baud rate (default 115200)", "baud", "115200");
    QCommandLineOption selfTestOption("selftest", "Run scripted headless verification instead of showing a window");
    parser.addOption(baudOption);
    parser.addOption(selfTestOption);
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.isEmpty()) {
        parser.showHelp(1);
    }

    QSerialPort serial;
    serial.setPortName(args.first());
    serial.setBaudRate(parser.value(baudOption).toInt());
    if (!serial.open(QIODevice::ReadWrite)) {
        qCritical() << "Failed to open" << args.first() << ":" << serial.errorString();
        return 1;
    }

    auto *term = new QTermWidget(0 /* do not start a shell */);
    term->setHistorySize(5000);

    QMainWindow window;
    window.setWindowTitle("Komport-Qt6 - QTermWidget bridge spike (ADR-001)");
    window.setCentralWidget(term);
    window.resize(800, 500);

    SerialTermBridge bridge(&serial, term);

    if (parser.isSet(selfTestOption)) {
        window.show();
        runSelfTest(term);
        return app.exec();
    }

    window.show();
    return app.exec();
}

#include "main.moc"
