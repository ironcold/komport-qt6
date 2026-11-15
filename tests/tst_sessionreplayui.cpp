/***************************************************************************
                    tst_sessionreplayui.cpp  -  Komport Replay UI
                             -------------------
    begin                : 2026
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de

    M10 step 6a: the application wiring of the offline state (SPEC-M10 5.7 and
    5.7.1) in real offscreen KomportApp windows - the load action's
    non-interactive part, the persistent indicator, the disabling of every
    output-relevant control in every open window, and the offline refusal
    inside every guarded entry point, which these tests call directly (a
    disabled QAction is only an affordance). The modal file dialog itself is
    Qt's; the tests drive KomportApp::loadSessionForReplayFile(), the part it
    delegates to.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "komport.h"
#include "komportdoc.h"
#include "komportcellarray.h"
#include "komporthexview.h"
#include "komportmacrobar.h"
#include "komportserial.h"
#include "komportview.h"
#include "sessionrecordcodec.h"
#include "sessionreplayplayer.h"

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPointer>
#include <QScopeGuard>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolBar>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <pty.h>
#include <unistd.h>

namespace {

QString makePty(int *masterFd)
{
  int slaveFd = -1;
  char slaveName[256];
  if (::openpty(masterFd, &slaveFd, slaveName, nullptr, nullptr) != 0)
    return QString();
  ::close(slaveFd);
  return QString::fromLocal8Bit(slaveName);
}

SessionHeaderFacts headerFacts()
{
  QJsonObject requested;
  requested.insert(QStringLiteral("endpoint"), QStringLiteral("/dev/recorded-elsewhere"));
  requested.insert(QStringLiteral("baudRate"), QStringLiteral("115200"));
  requested.insert(QStringLiteral("dataBits"), QStringLiteral("8"));
  requested.insert(QStringLiteral("stopBits"), QStringLiteral("1"));
  requested.insert(QStringLiteral("parity"), QStringLiteral("NONE"));
  requested.insert(QStringLiteral("flowControl"), QStringLiteral("NONE"));

  QJsonObject buffering;
  buffering.insert(QStringLiteral("rxQueue"), 1024);
  buffering.insert(QStringLiteral("flushRate"), 256);

  QJsonObject compatibility;
  compatibility.insert(QStringLiteral("startBits"), QStringLiteral("1"));

  QJsonObject configuration;
  configuration.insert(QStringLiteral("requested"), requested);
  configuration.insert(QStringLiteral("effective"), requested);
  configuration.insert(QStringLiteral("localBuffering"), buffering);
  configuration.insert(QStringLiteral("compatibility"), compatibility);

  SessionHeaderFacts facts;
  facts.applicationName = QStringLiteral("Komport");
  facts.applicationVersion = QStringLiteral("test");
  facts.createdUtc = QDateTime::fromString(QStringLiteral("2026-09-18T12:00:00Z"), Qt::ISODate);
  facts.configuration = configuration;
  facts.referenceSourceTimestampNs = 123456789;
  return facts;
}

SessionEvent dataEvent(quint64 sequence, SessionDirection direction, const QByteArray &payload,
                       qint64 timestampNs)
{
  SessionEvent event;
  event.sequence = sequence;
  event.sourceId = 1;
  event.sourceTimestampNs = 1000 + timestampNs;
  event.timestampNs = timestampNs;
  event.type = SessionEventType::Data;
  event.direction = direction;
  event.payload = payload;
  return event;
}

/** Write a valid two-event `.kpsession` file and return its path (empty on failure). */
QString writeSessionFile(const QTemporaryDir &directory, const QString &name)
{
  const SessionRecordEncoding header = SessionRecordCodec::encodeHeader(headerFacts());
  const SessionRecordEncoding first = SessionRecordCodec::encodeRecord(
      dataEvent(1, SessionDirection::Rx, QByteArrayLiteral("hello\r\n"), 250));
  const SessionRecordEncoding second = SessionRecordCodec::encodeRecord(
      dataEvent(2, SessionDirection::Tx, QByteArrayLiteral("ls\r"), 500));
  if (!header.ok || !first.ok || !second.ok)
    return QString();
  QFile out(directory.filePath(name));
  if (!out.open(QIODevice::WriteOnly))
    return QString();
  out.write(header.bytes + first.bytes + second.bytes);
  return out.fileName();
}

/** Write a session of the given events (sequences and times are assigned in order). */
QString writeEvents(const QTemporaryDir &directory, const QString &name, QList<SessionEvent> events)
{
  const SessionRecordEncoding header = SessionRecordCodec::encodeHeader(headerFacts());
  if (!header.ok)
    return QString();
  QByteArray file = header.bytes;
  quint64 sequence = 1;
  qint64 time = 100;
  for (SessionEvent &event : events) {
    event.sequence = sequence++;
    event.sourceId = 1;
    event.timestampNs = time;
    event.sourceTimestampNs = 1000 + time;
    time += 150;
    const SessionRecordEncoding record = SessionRecordCodec::encodeRecord(event);
    if (!record.ok)
      return QString();
    file.append(record.bytes);
  }
  QFile out(directory.filePath(name));
  if (!out.open(QIODevice::WriteOnly))
    return QString();
  out.write(file);
  return out.fileName();
}

SessionEvent annotation()
{
  SessionEvent event;
  event.type = SessionEventType::Annotation;
  event.direction = SessionDirection::None;
  return event;
}

QString gridText(KomportView *view)
{
  KomportCellArray *cells = view->cellArray();
  QString text;
  for (int y = 0; y < cells->arrayHeight(); ++y) {
    for (int x = 0; x < cells->arrayWidth(); ++x) {
      KomportCell *cell = cells->cell(x, y);
      const QChar c = cell ? cell->character() : QChar();
      text.append(c.isNull() ? QChar(' ') : c);
    }
    text.append(QLatin1Char('\n'));
  }
  return text;
}

/** Catches a modal dialog that a guarded entry point must never open: a guard that
  * fails would otherwise block the test run on the dialog's nested event loop. */
class ModalWatch
{
public:
  ModalWatch()
  {
    mTimer.setInterval(20);
    QObject::connect(&mTimer, &QTimer::timeout, [this]() {
      if (QWidget *modal = QApplication::activeModalWidget()) {
        mOpened = true;
        modal->close();
      }
    });
    mTimer.start();
  }
  bool opened() const { return mOpened; }

private:
  QTimer mTimer;
  bool mOpened = false;
};

QLabel *statusLabel(KomportApp *window)
{
  return window->findChild<QLabel *>(QStringLiteral("hoverHintLabel"));
}

QAction *action(KomportApp *window, const char *name)
{
  return window->findChild<QAction *>(QString::fromLatin1(name));
}

int appWindowCount()
{
  int count = 0;
  for (QWidget *w : QApplication::topLevelWidgets())
    if (qobject_cast<KomportApp *>(w))
      ++count;
  return count;
}

/** The peer observation of SPEC-M10 5.8, asserted with a timeout: poll the master for
  * @p ms, fail on the first received byte and on any read error other than "nothing
  * there yet". The port must be open on the slave side while this runs (a master
  * whose slave is closed reports EIO, which is deliberately not silence). */
bool peerStaysSilent(int masterFd, int ms, QString *why)
{
  QElapsedTimer timer;
  timer.start();
  do {
    char buffer[64];
    const ssize_t n = ::read(masterFd, buffer, sizeof buffer);
    if (n > 0) {
      *why = QStringLiteral("the peer received %1 byte(s)").arg(n);
      return false;
    }
    if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
      *why = QStringLiteral("read on the pty master failed: %1").arg(QString::fromLocal8Bit(::strerror(errno)));
      return false;
    }
    QTest::qWait(10);
  } while (timer.elapsed() < ms);
  return true;
}

/** Read from the master until @p expected has arrived or the timeout ends. */
QByteArray readPeer(int masterFd, int ms, const QByteArray &expected)
{
  QByteArray received;
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < ms && !received.contains(expected)) {
    char buffer[256];
    const ssize_t n = ::read(masterFd, buffer, sizeof buffer);
    if (n > 0)
      received.append(buffer, n);
    else
      QTest::qWait(10);
  }
  return received;
}

const QString kOfflineReason = QStringLiteral("a loaded offline session is being replayed - close it first");

} // namespace

class TstSessionReplayUi : public QObject
{
  Q_OBJECT
private slots:
  void initTestCase();
  void cleanupTestCase();

  void theOfflineIndicatorAppearsExactlyWhileOffline();
  void aRefusedLoadReportsItsReasonAndEntersNoOfflineState();
  void loadingIsRefusedWhileTheSessionIsLive();
  void loadingIsRefusedWhileAnOfflineSessionIsLoaded();
  void everyOutputRelevantControlIsDisabledWhileOffline();
  void offlineStateDisablesControlsInEveryOpenWindow();
  void aClipboardChangeDoesNotReenablePasteWhileOffline();
  void newWindowIsRefusedWhileAnyWindowIsOffline();
  void invokingEachOfflineGuardedEntryPointReachesNoTransport();
  void offlineGuardRefusesInEveryWindow();
  void keyInputPasteAndMacrosWhileOfflineReachNoTransport();
  void closingTheOfflineSessionRestoresTheLiveControls();
  void closingTheWindowThatHoldsTheSessionRestoresTheOthers();
  void loadingIsNotPersistedAcrossRuns();

  void theTimingLadderIsPresentInOrder();
  void replayControlsFollowThePlayerAndConsumeItsResults();
  void refusedTimingSelectionRestoresTheSelectorAndReportsIt();
  void replayRendersIntoTheSameViewAndHexMonitor();
  void aStoppedReplayDisclosesTheNonDataEventsItDelivered();
  void aPartialStepDisclosesTheNonDataEventsItDelivered();
  void aReplayRunsToItsEndAndReportsIt();
  void noTransmitDuringReplay();
  void everyReplayControlReachesNoTransport();
  void replayingARecordedPtySessionTransmitsNothing();
  void replayedTerminalQueriesThroughTheAdapterProduceNoReply();
  void closingTheOfflineSessionStopsARunningReplay();
  void aReceiverThatDeletesTheWindowDuringAnAppSlotIsHarmless();

private:
  QString mTestConfigFile;
};

void TstSessionReplayUi::initTestCase()
{
  // KomportApp reads and writes real QSettings on construction; redirect it to a
  // test-only organization so this test cannot touch a real user's profiles.
  QCoreApplication::setOrganizationName(QStringLiteral("Komport-Qt6-Test"));
  QCoreApplication::setApplicationName(QStringLiteral("Komport-Qt6-Test-ReplayUi"));
  mTestConfigFile = QSettings().fileName();
}

void TstSessionReplayUi::cleanupTestCase()
{
  const QString dirPath = QFileInfo(mTestConfigFile).absolutePath();
  QFile::remove(mTestConfigFile);
  QDir().rmdir(dirPath);
}

void TstSessionReplayUi::theOfflineIndicatorAppearsExactlyWhileOffline()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));
  QVERIFY(!path.isEmpty());

  KomportApp window;
  QLabel *indicator = window.findChild<QLabel *>(QStringLiteral("offlineReplayStatusLabel"));
  QVERIFY(indicator != nullptr);
  QVERIFY(indicator->isHidden());
  QVERIFY(action(&window, "loadSessionForReplay") != nullptr);
  QVERIFY(action(&window, "loadSessionForReplay")->isEnabled());
  QCOMPARE(action(&window, "closeOfflineSession")->isEnabled(), false);

  QVERIFY(window.loadSessionForReplayFile(path));

  QCOMPARE(window.getDocument()->hasOfflineSession(), true);
  QVERIFY(!indicator->isHidden());
  QCOMPARE(indicator->text(), QStringLiteral("Offline Session — Passive Replay"));
  QCOMPARE(indicator->toolTip(), path);
  QVERIFY(action(&window, "closeOfflineSession")->isEnabled());
  // The outcome is reported: the path and the event count.
  const QString status = statusLabel(&window)->text();
  QVERIFY2(status.contains(path) && status.contains(QStringLiteral("2 event")), qPrintable(status));
  // The load opened no port and applied no recorded setting.
  QCOMPARE(window.getDocument()->getSerial()->isOpen(), false);
  QVERIFY(window.getDocument()->getSerial()->deviceName() != QStringLiteral("/dev/recorded-elsewhere"));

  window.slotCloseOfflineSession();
  QVERIFY(indicator->isHidden());
  QCOMPARE(window.getDocument()->hasOfflineSession(), false);
  QCOMPARE(action(&window, "closeOfflineSession")->isEnabled(), false);
}

void TstSessionReplayUi::aRefusedLoadReportsItsReasonAndEntersNoOfflineState()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString garbage = directory.filePath(QStringLiteral("garbage.kpsession"));
  {
    QFile out(garbage);
    QVERIFY(out.open(QIODevice::WriteOnly));
    out.write("this is not a session file at all, but it is long enough to read a prefix");
  }

  KomportApp window;
  QLabel *indicator = window.findChild<QLabel *>(QStringLiteral("offlineReplayStatusLabel"));
  QCOMPARE(window.loadSessionForReplayFile(garbage), false);
  QCOMPARE(window.loadSessionForReplayFile(directory.filePath(QStringLiteral("missing.kpsession"))), false);

  QCOMPARE(window.getDocument()->hasOfflineSession(), false);
  QVERIFY(indicator->isHidden());
  QVERIFY(action(&window, "fileOpen")->isEnabled());
  QVERIFY(action(&window, "showPreferences")->isEnabled());
  QVERIFY2(statusLabel(&window)->text().startsWith(QStringLiteral("Session not loaded: ")),
           qPrintable(statusLabel(&window)->text()));
}

void TstSessionReplayUi::loadingIsRefusedWhileTheSessionIsLive()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if (slaveName.isEmpty())
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));

  KomportApp window;
  KomportSerial *serial = window.getDocument()->getSerial();
  serial->close();
  serial->setDeviceName(slaveName);
  serial->setBaudRate(qint32(9600));
  QVERIFY(serial->open());

  QCOMPARE(window.loadSessionForReplayFile(path), false);

  QVERIFY2(statusLabel(&window)->text().contains(QStringLiteral("a live session is active")),
           qPrintable(statusLabel(&window)->text()));
  QCOMPARE(window.getDocument()->hasOfflineSession(), false);
  QVERIFY(window.findChild<QLabel *>(QStringLiteral("offlineReplayStatusLabel"))->isHidden());
  QCOMPARE(serial->isOpen(), true);   // the live session is untouched
  QVERIFY(action(&window, "fileOpen")->isEnabled());
}

void TstSessionReplayUi::loadingIsRefusedWhileAnOfflineSessionIsLoaded()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString first = writeSessionFile(directory, QStringLiteral("first.kpsession"));
  const QString second = writeSessionFile(directory, QStringLiteral("second.kpsession"));

  KomportApp window;
  QVERIFY(window.loadSessionForReplayFile(first));
  const SessionFilePtr loaded = window.getDocument()->offlineSession();

  QCOMPARE(window.loadSessionForReplayFile(second), false);

  QVERIFY2(statusLabel(&window)->text().contains(QStringLiteral("already loaded")),
           qPrintable(statusLabel(&window)->text()));
  QVERIFY(window.getDocument()->offlineSession() == loaded);
  QCOMPARE(window.findChild<QLabel *>(QStringLiteral("offlineReplayStatusLabel"))->toolTip(), first);
}

void TstSessionReplayUi::everyOutputRelevantControlIsDisabledWhileOffline()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));

  KomportApp window;
  window.getConfig();   // (construction already read the settings)
  const QByteArrayList actions = { "fileNewWindow", "fileOpen", "fileSave", "recordSession",
                                   "recordLiveSession", "profileSave", "profileDelete",
                                   "showPreferences" };
  const QList<QWidget *> widgets = { window.findChild<QComboBox *>(QStringLiteral("profileCombo")),
                                     window.findChild<KomportMacroBar *>(QStringLiteral("macroBar")) };
  for (const QByteArray &name : actions) {
    QVERIFY2(action(&window, name) != nullptr, name.constData());
    QVERIFY2(action(&window, name)->isEnabled(), name.constData());
  }
  for (QWidget *w : widgets) {
    QVERIFY(w != nullptr);
    QVERIFY(w->isEnabled());
  }

  QVERIFY(window.loadSessionForReplayFile(path));

  for (const QByteArray &name : actions)
    QVERIFY2(!action(&window, name)->isEnabled(), name.constData());
  for (QWidget *w : widgets)
    QVERIFY(!w->isEnabled());
  QMenu *recent = window.findChild<QMenu *>(QStringLiteral("fileOpenRecentMenu"));
  QVERIFY(recent != nullptr);
  QVERIFY(!recent->isEnabled());
  QVERIFY(!action(&window, "editPaste")->isEnabled());
  // The actions that remain usable offline.
  QVERIFY(action(&window, "loadSessionForReplay")->isEnabled());
  QVERIFY(action(&window, "closeOfflineSession")->isEnabled());

  // Key input is suppressed: a real key press produces no byte at the view's signal.
  KomportView *view = window.findChild<KomportView *>();
  QVERIFY(view != nullptr);
  QSignalSpy keys(view, &KomportView::keyPressed);
  QKeyEvent press(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
  QCoreApplication::sendEvent(view, &press);
  QCOMPARE(keys.count(), 0);
  QVERIFY(press.isAccepted());
}

void TstSessionReplayUi::offlineStateDisablesControlsInEveryOpenWindow()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));

  KomportApp a;
  KomportApp b;   // opened before the load
  QVERIFY(action(&b, "fileOpen")->isEnabled());

  QVERIFY(a.loadSessionForReplayFile(path));

  const char *names[] = { "fileNewWindow", "fileOpen", "fileSave", "recordSession",
                          "recordLiveSession", "profileSave", "profileDelete", "showPreferences" };
  for (const char *name : names)
    QVERIFY2(!action(&b, name)->isEnabled(), name);
  QVERIFY(!b.findChild<QComboBox *>(QStringLiteral("profileCombo"))->isEnabled());
  QVERIFY(!b.findChild<KomportMacroBar *>(QStringLiteral("macroBar"))->isEnabled());
  QVERIFY(!b.findChild<QMenu *>(QStringLiteral("fileOpenRecentMenu"))->isEnabled());
  // The indicator and Close Session appear only in the window that loaded the session.
  QVERIFY(!a.findChild<QLabel *>(QStringLiteral("offlineReplayStatusLabel"))->isHidden());
  QVERIFY(b.findChild<QLabel *>(QStringLiteral("offlineReplayStatusLabel"))->isHidden());
  QVERIFY(action(&a, "closeOfflineSession")->isEnabled());
  QVERIFY(!action(&b, "closeOfflineSession")->isEnabled());

  a.slotCloseOfflineSession();

  for (const char *name : names) {
    QVERIFY2(action(&a, name)->isEnabled(), name);
    QVERIFY2(action(&b, name)->isEnabled(), name);
  }
  QVERIFY(b.findChild<QComboBox *>(QStringLiteral("profileCombo"))->isEnabled());
  QVERIFY(b.findChild<KomportMacroBar *>(QStringLiteral("macroBar"))->isEnabled());
}

void TstSessionReplayUi::aClipboardChangeDoesNotReenablePasteWhileOffline()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));

  KomportApp window;
  KomportView *view = window.findChild<KomportView *>();
  QVERIFY(view != nullptr);
  QVERIFY(window.loadSessionForReplayFile(path));

  QApplication::clipboard()->setText(QStringLiteral("pasted"), QClipboard::Clipboard);
  window.slotViewModified(view);   // what a selection or clipboard change triggers

  QVERIFY(!action(&window, "editPaste")->isEnabled());
}

void TstSessionReplayUi::newWindowIsRefusedWhileAnyWindowIsOffline()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));

  KomportApp a;
  KomportApp b;
  QVERIFY(a.loadSessionForReplayFile(path));
  const int before = appWindowCount();

  a.slotFileNewWindow();
  b.slotFileNewWindow();

  QCOMPARE(appWindowCount(), before);   // no second window, hence no profile load, no port open
  QVERIFY2(statusLabel(&b)->text() == kOfflineReason, qPrintable(statusLabel(&b)->text()));
}

void TstSessionReplayUi::invokingEachOfflineGuardedEntryPointReachesNoTransport()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));
  const QString target = directory.filePath(QStringLiteral("must-not-exist.kpsession"));

  KomportApp window;
  QVERIFY(window.loadSessionForReplayFile(path));
  KomportSerial *serial = window.getDocument()->getSerial();
  QSignalSpy written(serial, &ITransport::bytesWritten);
  QApplication::clipboard()->setText(QStringLiteral("abc"), QClipboard::Clipboard);
  const int windows = appWindowCount();
  ModalWatch watch;

  using Call = std::function<void()>;
  const QList<QPair<const char *, Call>> calls = {
    { "slotFileNewWindow", [&] { window.slotFileNewWindow(); } },
    { "slotEditPaste", [&] { window.slotEditPaste(); } },
    { "slotFileOpen", [&] { window.slotFileOpen(); } },
    { "slotFileSave", [&] { window.slotFileSave(); } },
    { "slotFileSaveAs", [&] { window.slotFileSaveAs(); } },
    { "slotFileOpenRecent", [&] { window.slotFileOpenRecent(QUrl::fromLocalFile(path)); } },
    { "slotMacroTriggered", [&] { window.slotMacroTriggered(QStringLiteral("show version")); } },
    { "slotToggleSessionRecording", [&] { window.slotToggleSessionRecording(true); } },
    { "slotToggleRecording", [&] { window.slotToggleRecording(true); } },
    { "startSessionRecording", [&] { QCOMPARE(window.startSessionRecording(target), false); } },
    { "loadProfile", [&] { window.loadProfile(QStringLiteral("Default")); } },
    { "slotSaveProfile", [&] { window.slotSaveProfile(); } },
    { "slotDeleteProfile", [&] { window.slotDeleteProfile(); } },
    { "slotShowPreferences", [&] { window.slotShowPreferences(); } },
  };
  for (const auto &call : calls) {
    statusLabel(&window)->setText(QStringLiteral("untouched"));
    call.second();
    QVERIFY2(statusLabel(&window)->text() == kOfflineReason, call.first);
    QVERIFY2(!watch.opened(), call.first);
  }

  QCOMPARE(appWindowCount(), windows);
  QCOMPARE(QFile::exists(target), false);
  QCOMPARE(window.getDocument()->getSessionRecorder()->state(), SessionRecorder::State::Stopped);
  QCOMPARE(serial->isOpen(), false);
  QCOMPARE(written.count(), 0);
  QVERIFY(window.getDocument()->hasOfflineSession());   // nothing disturbed the session
}

void TstSessionReplayUi::offlineGuardRefusesInEveryWindow()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if (slaveName.isEmpty())
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });
  ::fcntl(masterFd, F_SETFL, ::fcntl(masterFd, F_GETFL, 0) | O_NONBLOCK);

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));
  const QString target = directory.filePath(QStringLiteral("must-not-exist.kpsession"));

  KomportApp a;
  KomportApp b;
  QVERIFY(a.loadSessionForReplayFile(path));
  // B's port is opened directly (a test seam: the user interface cannot do this while
  // a window is offline), so that a failing guard in B would reach a real pty.
  KomportSerial *serial = b.getDocument()->getSerial();
  serial->close();
  serial->setDeviceName(slaveName);
  serial->setBaudRate(qint32(9600));
  QVERIFY(serial->open());
  QSignalSpy written(serial, &ITransport::bytesWritten);
  QApplication::clipboard()->setText(QStringLiteral("abc"), QClipboard::Clipboard);
  ModalWatch watch;

  b.loadProfile(QStringLiteral("Default"));
  QVERIFY(statusLabel(&b)->text() == kOfflineReason);
  b.slotShowPreferences();
  QVERIFY(statusLabel(&b)->text() == kOfflineReason);
  statusLabel(&b)->setText(QStringLiteral("untouched"));
  b.slotEditPaste();
  QVERIFY(statusLabel(&b)->text() == kOfflineReason);
  statusLabel(&b)->setText(QStringLiteral("untouched"));
  b.slotMacroTriggered(QStringLiteral("show version"));
  QVERIFY(statusLabel(&b)->text() == kOfflineReason);
  statusLabel(&b)->setText(QStringLiteral("untouched"));
  b.slotFileOpen();
  QVERIFY(statusLabel(&b)->text() == kOfflineReason);
  statusLabel(&b)->setText(QStringLiteral("untouched"));
  b.slotFileSave();
  QVERIFY(statusLabel(&b)->text() == kOfflineReason);
  statusLabel(&b)->setText(QStringLiteral("untouched"));
  b.slotToggleSessionRecording(true);
  QVERIFY(statusLabel(&b)->text() == kOfflineReason);
  statusLabel(&b)->setText(QStringLiteral("untouched"));
  QCOMPARE(b.startSessionRecording(target), false);
  QVERIFY(statusLabel(&b)->text() == kOfflineReason);

  QVERIFY(!watch.opened());
  QCOMPARE(QFile::exists(target), false);
  QCOMPARE(written.count(), 0);
  QString why;
  QVERIFY2(peerStaysSilent(masterFd, 300, &why), qPrintable(why));
  QCOMPARE(serial->isOpen(), true);   // refused, not closed or reconfigured
  QCOMPARE(serial->deviceName(), slaveName);
}

void TstSessionReplayUi::keyInputPasteAndMacrosWhileOfflineReachNoTransport()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if (slaveName.isEmpty())
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });
  ::fcntl(masterFd, F_SETFL, ::fcntl(masterFd, F_GETFL, 0) | O_NONBLOCK);

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));

  KomportApp window;
  QVERIFY(window.loadSessionForReplayFile(path));
  KomportSerial *serial = window.getDocument()->getSerial();
  serial->close();
  serial->setDeviceName(slaveName);
  serial->setBaudRate(qint32(9600));
  QVERIFY(serial->open());   // test seam, as above
  QSignalSpy written(serial, &ITransport::bytesWritten);

  KomportView *view = window.findChild<KomportView *>();
  QVERIFY(view != nullptr);
  QKeyEvent press(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
  QCoreApplication::sendEvent(view, &press);
  QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
  QCoreApplication::sendEvent(view, &enter);
  QApplication::clipboard()->setText(QStringLiteral("pasted text"), QClipboard::Clipboard);
  window.slotEditPaste();
  window.slotMacroTriggered(QStringLiteral("show version"));
  QTest::qWait(50);

  QCOMPARE(written.count(), 0);
  QString why;
  QVERIFY2(peerStaysSilent(masterFd, 300, &why), qPrintable(why));
}

void TstSessionReplayUi::closingTheOfflineSessionRestoresTheLiveControls()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));

  KomportApp window;
  const QString device = window.getDocument()->getSerial()->deviceName();
  QVERIFY(window.loadSessionForReplayFile(path));

  window.slotCloseOfflineSession();

  QVERIFY(window.findChild<QLabel *>(QStringLiteral("offlineReplayStatusLabel"))->isHidden());
  QVERIFY(action(&window, "fileOpen")->isEnabled());
  QVERIFY(action(&window, "showPreferences")->isEnabled());
  QVERIFY(window.findChild<KomportMacroBar *>(QStringLiteral("macroBar"))->isEnabled());
  QCOMPARE(window.getDocument()->getSerial()->isOpen(), false);   // no port was opened
  QCOMPARE(window.getDocument()->getSerial()->deviceName(), device);
  // Key input is back: a key press reaches the view's signal again.
  KomportView *view = window.findChild<KomportView *>();
  QSignalSpy keys(view, &KomportView::keyPressed);
  QKeyEvent press(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
  QCoreApplication::sendEvent(view, &press);
  QCOMPARE(keys.count(), 1);
  // And a new load is possible again.
  QVERIFY(window.loadSessionForReplayFile(path));
}

void TstSessionReplayUi::closingTheWindowThatHoldsTheSessionRestoresTheOthers()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));

  KomportApp b;
  {
    // Deleted without a close event: the destructor's own safety net.
    auto *a = new KomportApp();
    QVERIFY(a->loadSessionForReplayFile(path));
    QVERIFY(!action(&b, "fileOpen")->isEnabled());
    delete a;
  }
  QVERIFY(action(&b, "fileOpen")->isEnabled());
  QVERIFY(b.findChild<KomportMacroBar *>(QStringLiteral("macroBar"))->isEnabled());

  {
    // Closed through the window: closeEvent() leaves the offline state.
    auto *a = new KomportApp();
    QPointer<KomportApp> guard(a);
    QVERIFY(a->loadSessionForReplayFile(path));
    QVERIFY(!action(&b, "fileOpen")->isEnabled());
    a->close();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(action(&b, "fileOpen")->isEnabled());
  }
}

void TstSessionReplayUi::loadingIsNotPersistedAcrossRuns()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));

  QSettings settings;
  settings.sync();
  const QStringList keysBefore = settings.allKeys();
  {
    KomportApp window;
    QVERIFY(window.loadSessionForReplayFile(path));
    window.slotCloseOfflineSession();
    window.close();
  }
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  settings.sync();
  for (const QString &key : settings.allKeys()) {
    QVERIFY2(!settings.value(key).toString().contains(path), qPrintable(key));
    QVERIFY2(!key.contains(QStringLiteral("ffline")) && !key.contains(QStringLiteral("eplay")), qPrintable(key));
  }
  Q_UNUSED(keysBefore);

  KomportApp fresh;   // a new window starts live
  QVERIFY(!fresh.getDocument()->hasOfflineSession());
  QVERIFY(action(&fresh, "fileOpen")->isEnabled());
}

void TstSessionReplayUi::theTimingLadderIsPresentInOrder()
{
  KomportApp window;
  auto *combo = window.findChild<QComboBox *>(QStringLiteral("replayTimingCombo"));
  QVERIFY(combo != nullptr);
  const QStringList expected = { QStringLiteral("Original"), QStringLiteral("Immediate"), QStringLiteral("0.1x"),
                                 QStringLiteral("0.25x"), QStringLiteral("0.5x"), QStringLiteral("1x"),
                                 QStringLiteral("2x"), QStringLiteral("5x"), QStringLiteral("10x"),
                                 QStringLiteral("Step") };
  QStringList actual;
  for (int i = 0; i < combo->count(); ++i)
    actual << combo->itemText(i);
  QCOMPARE(actual, expected);
  const SessionReplayTiming order[] = { SessionReplayTiming::Original, SessionReplayTiming::Immediate,
                                        SessionReplayTiming::Scale0_1, SessionReplayTiming::Scale0_25,
                                        SessionReplayTiming::Scale0_5, SessionReplayTiming::Scale1,
                                        SessionReplayTiming::Scale2, SessionReplayTiming::Scale5,
                                        SessionReplayTiming::Scale10, SessionReplayTiming::Step };
  for (int i = 0; i < combo->count(); ++i)
    QCOMPARE(combo->itemData(i).toInt(), int(order[i]));
  for (const char *name : { "replayPlay", "replayStop", "replayStepEvent", "replayStepRx", "replayStepTx" })
    QVERIFY2(action(&window, name) != nullptr, name);
  // Idle: the replay controls are hidden and disabled.
  QVERIFY(window.findChild<QToolBar *>(QStringLiteral("replayToolBar"))->isHidden());
  QVERIFY(!action(&window, "replayPlay")->isEnabled());
  QVERIFY(!combo->isEnabled());
}

void TstSessionReplayUi::replayControlsFollowThePlayerAndConsumeItsResults()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  SessionEvent rx1, tx, rx2;
  rx1.type = SessionEventType::Data; rx1.direction = SessionDirection::Rx; rx1.payload = "one";
  tx.type = SessionEventType::Data; tx.direction = SessionDirection::Tx; tx.payload = "two";
  rx2.type = SessionEventType::Data; rx2.direction = SessionDirection::Rx; rx2.payload = "three";
  const QString path = writeEvents(directory, QStringLiteral("steps.kpsession"), { annotation(), tx, rx1, rx2 });
  QVERIFY(!path.isEmpty());

  KomportApp window;
  QVERIFY(window.loadSessionForReplayFile(path));
  SessionReplayPlayer *player = window.getDocument()->getReplayPlayer();
  QVERIFY(player != nullptr);
  QVERIFY(!window.findChild<QToolBar *>(QStringLiteral("replayToolBar"))->isHidden());
  QCOMPARE(player->state(), SessionReplayState::Ready);   // nothing auto-plays
  QCOMPARE(player->deliveredCount(), quint64(0));
  QVERIFY(action(&window, "replayPlay")->isEnabled());
  QVERIFY(!action(&window, "replayStop")->isEnabled());

  // Step RX delivers the annotation, the TX and the first RX, and reports it.
  action(&window, "replayStepRx")->trigger();
  QCOMPARE(player->deliveredCount(), quint64(3));
  QVERIFY2(statusLabel(&window)->text().contains(QStringLiteral("3 event")), qPrintable(statusLabel(&window)->text()));
  // Step TX finds none ahead: reported as such, delivered up to the end.
  action(&window, "replayStepTx")->trigger();
  QVERIFY2(statusLabel(&window)->text().contains(QStringLiteral("No further TX")), qPrintable(statusLabel(&window)->text()));
  // A replay completed by steps alone still discloses the events no pane renders.
  QVERIFY2(statusLabel(&window)->text().contains(QStringLiteral("1 non-data event")), qPrintable(statusLabel(&window)->text()));
  QCOMPARE(player->state(), SessionReplayState::Finished);
  // Finished: the affordances follow, and a refused play is reported.
  QVERIFY(!action(&window, "replayPlay")->isEnabled());
  window.slotReplayPlay();
  QVERIFY2(statusLabel(&window)->text().startsWith(QStringLiteral("Replay not started: ")), qPrintable(statusLabel(&window)->text()));
  window.slotReplayStepEvent();
  QVERIFY2(statusLabel(&window)->text().startsWith(QStringLiteral("Step refused: ")), qPrintable(statusLabel(&window)->text()));
}

void TstSessionReplayUi::refusedTimingSelectionRestoresTheSelectorAndReportsIt()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));
  KomportApp window;
  QVERIFY(window.loadSessionForReplayFile(path));
  auto *combo = window.findChild<QComboBox *>(QStringLiteral("replayTimingCombo"));
  SessionReplayPlayer *player = window.getDocument()->getReplayPlayer();
  QCOMPARE(combo->currentData().toInt(), int(SessionReplayTiming::Original));

  // An accepted change shows the adopted policy.
  combo->setCurrentIndex(1);
  emit combo->activated(1);
  QCOMPARE(player->timing(), SessionReplayTiming::Immediate);
  QCOMPARE(combo->currentData().toInt(), int(SessionReplayTiming::Immediate));
  combo->setCurrentIndex(0);
  emit combo->activated(0);   // back to Original: Immediate would finish in one turn

  // Playing (nothing delivered yet: the first delivery waits for the event loop).
  window.slotReplayPlay();
  QCOMPARE(player->state(), SessionReplayState::Playing);
  const int stepIndex = combo->count() - 1;
  combo->setCurrentIndex(stepIndex);
  emit combo->activated(stepIndex);

  QCOMPARE(player->timing(), SessionReplayTiming::Original);               // refused
  QCOMPARE(combo->currentData().toInt(), int(SessionReplayTiming::Original));   // selector restored
  QCOMPARE(player->state(), SessionReplayState::Playing);                  // no implicit pause
  QVERIFY2(statusLabel(&window)->text().contains(QStringLiteral("pause the replay before selecting step mode")),
           qPrintable(statusLabel(&window)->text()));

  window.slotReplayStop();
  combo->setCurrentIndex(stepIndex);
  emit combo->activated(stepIndex);
  QCOMPARE(player->timing(), SessionReplayTiming::Step);
  QCOMPARE(combo->currentData().toInt(), int(SessionReplayTiming::Step));
}

void TstSessionReplayUi::replayRendersIntoTheSameViewAndHexMonitor()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  SessionEvent rx, tx, rx2;
  rx.type = SessionEventType::Data; rx.direction = SessionDirection::Rx;
  rx.payload = QByteArray("AB\0\xFF" "C", 5);
  tx.type = SessionEventType::Data; tx.direction = SessionDirection::Tx;
  tx.payload = QByteArray("typed\0", 6);
  rx2.type = SessionEventType::Data; rx2.direction = SessionDirection::Rx;
  rx2.payload = "\r\nSecond";
  const QString path = writeEvents(directory, QStringLiteral("render.kpsession"), { rx, annotation(), tx, rx2 });
  QVERIFY(!path.isEmpty());

  KomportApp window;
  KomportView *view = window.findChild<KomportView *>();
  auto *hex = window.findChild<KomportHexView *>();
  auto *log = hex->findChild<QPlainTextEdit *>();
  QVERIFY(view && hex && log);
  QVERIFY(window.loadSessionForReplayFile(path));
  QVERIFY(!gridText(view).contains(QLatin1Char('A')));   // loading renders nothing

  auto *combo = window.findChild<QComboBox *>(QStringLiteral("replayTimingCombo"));
  combo->setCurrentIndex(1);
  emit combo->activated(1);   // Immediate
  window.slotReplayPlay();
  SessionReplayPlayer *player = window.getDocument()->getReplayPlayer();
  QTRY_COMPARE(player->state(), SessionReplayState::Finished);

  // RX bytes appear as characters; TX bytes do not (a local TX is not echoed).
  const QString grid = gridText(view);
  QVERIFY2(grid.contains(QLatin1Char('A')) && grid.contains(QLatin1Char('B')), qPrintable(grid));
  QVERIFY(grid.contains(QStringLiteral("Second")));
  QVERIFY(!grid.contains(QStringLiteral("typed")));
  // The hex monitor shows every data byte, tagged by the stored direction, byte-exactly.
  QTRY_VERIFY_WITH_TIMEOUT(log->toPlainText().contains(QStringLiteral("Second")), 2000);
  const QString hexText = log->toPlainText();
  QVERIFY2(hexText.contains(QStringLiteral("RX 00000000  41 42 00 ff 43")), qPrintable(hexText));
  QVERIFY2(hexText.contains(QStringLiteral("TX 00000000  74 79 70 65 64 00")), qPrintable(hexText));
  // The finish is reported with the non-data count the panes do not render.
  QVERIFY2(statusLabel(&window)->text().contains(QStringLiteral("4 event")), qPrintable(statusLabel(&window)->text()));
  QVERIFY2(statusLabel(&window)->text().contains(QStringLiteral("1 non-data")), qPrintable(statusLabel(&window)->text()));
  // Nothing was written anywhere.
  QCOMPARE(window.getDocument()->getSerial()->isOpen(), false);
}

/** The adapter wiring end to end with a real pty as the peer: the stored terminal
  * queries are rendered but nothing leaves the process. (The live contrast and the
  * source audit of the adapter are SPEC-M10 step 7.) */
void TstSessionReplayUi::replayedTerminalQueriesThroughTheAdapterProduceNoReply()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if (slaveName.isEmpty())
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });
  ::fcntl(masterFd, F_SETFL, ::fcntl(masterFd, F_GETFL, 0) | O_NONBLOCK);

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  SessionEvent queries;
  queries.type = SessionEventType::Data;
  queries.direction = SessionDirection::Rx;
  queries.payload = QByteArrayLiteral("start\x1b[5n\x1b[6n\x1b[c\x1bZ" "end");
  const QString path = writeEvents(directory, QStringLiteral("queries.kpsession"), { queries });
  QVERIFY(!path.isEmpty());

  KomportApp window;
  QVERIFY(window.loadSessionForReplayFile(path));
  KomportSerial *serial = window.getDocument()->getSerial();
  serial->close();
  serial->setDeviceName(slaveName);
  serial->setBaudRate(qint32(9600));
  QVERIFY(serial->open());   // test seam: a live port that a failing adapter would write to
  QSignalSpy written(serial, &ITransport::bytesWritten);

  auto *combo = window.findChild<QComboBox *>(QStringLiteral("replayTimingCombo"));
  combo->setCurrentIndex(1);
  emit combo->activated(1);
  window.slotReplayPlay();
  SessionReplayPlayer *player = window.getDocument()->getReplayPlayer();
  QTRY_COMPARE(player->state(), SessionReplayState::Finished);
  QTest::qWait(100);

  KomportView *view = window.findChild<KomportView *>();
  QVERIFY2(gridText(view).contains(QStringLiteral("start")) && gridText(view).contains(QStringLiteral("end")),
           qPrintable(gridText(view)));
  QCOMPARE(written.count(), 0);
  QString why;
  QVERIFY2(peerStaysSilent(masterFd, 300, &why), qPrintable(why));
}

void TstSessionReplayUi::aStoppedReplayDisclosesTheNonDataEventsItDelivered()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  SessionEvent rx;
  rx.type = SessionEventType::Data; rx.direction = SessionDirection::Rx; rx.payload = "data";
  const QString path = writeEvents(directory, QStringLiteral("stop.kpsession"), { annotation(), annotation(), rx, rx });
  KomportApp window;
  QVERIFY(window.loadSessionForReplayFile(path));
  window.slotReplayStepEvent();   // delivers the first annotation
  window.slotReplayStepEvent();   // and the second
  window.slotReplayPlay();        // Original timing: nothing more is delivered before the loop turns
  SessionReplayPlayer *player = window.getDocument()->getReplayPlayer();
  QCOMPARE(player->state(), SessionReplayState::Playing);

  window.slotReplayStop();

  QVERIFY2(statusLabel(&window)->text().contains(QStringLiteral("2 non-data event")), qPrintable(statusLabel(&window)->text()));
  QVERIFY2(statusLabel(&window)->text().contains(QStringLiteral("2 remaining")), qPrintable(statusLabel(&window)->text()));
}

void TstSessionReplayUi::aPartialStepDisclosesTheNonDataEventsItDelivered()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  SessionEvent rx;
  rx.type = SessionEventType::Data; rx.direction = SessionDirection::Rx; rx.payload = "data";
  const QString path = writeEvents(directory, QStringLiteral("partial.kpsession"), { annotation(), rx, rx });
  KomportApp window;
  QVERIFY(window.loadSessionForReplayFile(path));

  window.slotReplayStepEvent();   // delivers the annotation only; two events remain

  SessionReplayPlayer *player = window.getDocument()->getReplayPlayer();
  QVERIFY(player->state() != SessionReplayState::Finished);
  QVERIFY2(statusLabel(&window)->text().contains(QStringLiteral("1 non-data event")), qPrintable(statusLabel(&window)->text()));
  // A step that delivers only data after that keeps the cumulative count visible.
  window.slotReplayStepEvent();
  QVERIFY2(statusLabel(&window)->text().contains(QStringLiteral("1 non-data event")), qPrintable(statusLabel(&window)->text()));
  window.slotCloseOfflineSession();
}

/** A full replay of a recorded TX/RX session with a live pty behind the port: the TX
  * payloads are delivered to the consumer, and nothing reaches the peer. */
void TstSessionReplayUi::noTransmitDuringReplay()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if (slaveName.isEmpty())
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });
  ::fcntl(masterFd, F_SETFL, ::fcntl(masterFd, F_GETFL, 0) | O_NONBLOCK);

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));   // RX "hello", TX "ls"
  KomportApp window;
  QVERIFY(window.loadSessionForReplayFile(path));
  KomportSerial *serial = window.getDocument()->getSerial();
  serial->close();
  serial->setDeviceName(slaveName);
  serial->setBaudRate(qint32(9600));
  QVERIFY(serial->open());   // test seam: a live port that a failing replay would write to
  QSignalSpy written(serial, &ITransport::bytesWritten);

  SessionReplayPlayer *player = window.getDocument()->getReplayPlayer();
  QByteArray deliveredTx;
  connect(player, &SessionReplayPlayer::eventDelivered, this, [&deliveredTx](const SessionEvent &e) {
    if (e.type == SessionEventType::Data && e.direction == SessionDirection::Tx)
      deliveredTx.append(e.payload);
  });
  auto *combo = window.findChild<QComboBox *>(QStringLiteral("replayTimingCombo"));
  combo->setCurrentIndex(1);
  emit combo->activated(1);
  window.slotReplayPlay();
  QTRY_COMPARE(player->state(), SessionReplayState::Finished);
  QTest::qWait(100);

  QCOMPARE(deliveredTx, QByteArrayLiteral("ls\r"));   // delivered to the consumer ...
  QCOMPARE(written.count(), 0);                       // ... and never written
  QString why;
  QVERIFY2(peerStaysSilent(masterFd, 300, &why), qPrintable(why));
}

/** Every replay handler of the window - each step, play, stop and the timing ladder -
  * driven with a live pty behind the port and a stream that holds terminal queries,
  * TX data and non-data events: nothing is written, nothing reaches the peer. */
void TstSessionReplayUi::everyReplayControlReachesNoTransport()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if (slaveName.isEmpty())
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });
  ::fcntl(masterFd, F_SETFL, ::fcntl(masterFd, F_GETFL, 0) | O_NONBLOCK);

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  SessionEvent query, tx, rx;
  query.type = SessionEventType::Data; query.direction = SessionDirection::Rx;
  query.payload = QByteArrayLiteral("\x1b[5n\x1b[6n\x1b[c\x1bZ");
  tx.type = SessionEventType::Data; tx.direction = SessionDirection::Tx; tx.payload = "typed\r";
  rx.type = SessionEventType::Data; rx.direction = SessionDirection::Rx; rx.payload = "tail";
  const QString path = writeEvents(directory, QStringLiteral("all.kpsession"),
                                   { annotation(), query, tx, rx, query, tx, rx, annotation() });
  KomportApp window;
  QVERIFY(window.loadSessionForReplayFile(path));
  KomportSerial *serial = window.getDocument()->getSerial();
  serial->close();
  serial->setDeviceName(slaveName);
  serial->setBaudRate(qint32(9600));
  QVERIFY(serial->open());   // test seam, as above
  QSignalSpy written(serial, &ITransport::bytesWritten);

  window.slotReplayStepEvent();
  window.slotReplayStepRx();
  window.slotReplayStepTx();
  window.slotReplayStepRx();
  auto *combo = window.findChild<QComboBox *>(QStringLiteral("replayTimingCombo"));
  for (int index = 0; index < combo->count(); ++index) {   // the whole ladder
    combo->setCurrentIndex(index);
    emit combo->activated(index);
  }
  combo->setCurrentIndex(1);
  emit combo->activated(1);
  window.slotReplayPlay();
  window.slotReplayStop();
  window.slotReplayPlay();
  SessionReplayPlayer *player = window.getDocument()->getReplayPlayer();
  QTRY_COMPARE(player->state(), SessionReplayState::Finished);
  window.slotCloseOfflineSession();

  QCOMPARE(written.count(), 0);
  QString why;
  QVERIFY2(peerStaysSilent(masterFd, 300, &why), qPrintable(why));
}

/** The same for a file the M9 recorder wrote end to end from a live pty session,
  * including a terminal query the live emulation answered while recording. */
void TstSessionReplayUi::replayingARecordedPtySessionTransmitsNothing()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if (slaveName.isEmpty())
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });
  ::fcntl(masterFd, F_SETFL, ::fcntl(masterFd, F_GETFL, 0) | O_NONBLOCK);

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("recorded.kpsession"));

  KomportApp window;
  KomportSerial *serial = window.getDocument()->getSerial();
  serial->close();
  serial->setDeviceName(slaveName);
  serial->setBaudRate(qint32(9600));
  QVERIFY(serial->open());
  QVERIFY2(window.startSessionRecording(path), "the recording was refused");

  // RX with a terminal query (the live emulation answers it: a recorded TX), then a TX.
  const QByteArray rx = QByteArrayLiteral("hi\x1b[5nthere\x00\xff");
  QCOMPARE(::write(masterFd, rx.constData(), rx.size()), qint64(rx.size()));
  QTest::qWait(300);
  serial->putStr("typed\r");
  QTest::qWait(300);
  // The live emulation answered the status query: the exact reply reached the peer.
  const QByteArray liveTraffic = readPeer(masterFd, 2000, QByteArrayLiteral("\x1b[0n") + QByteArrayLiteral("typed\r"));
  QVERIFY2(liveTraffic.contains(QByteArrayLiteral("\x1b[0n")), qPrintable(QString::fromLatin1(liveTraffic.toHex())));
  QVERIFY(liveTraffic.contains(QByteArrayLiteral("typed\r")));
  window.slotToggleSessionRecording(false);
  serial->close();

  QVERIFY2(window.loadSessionForReplayFile(path), qPrintable(statusLabel(&window)->text()));
  // The file really holds the injected RX, the emulation's live answer and the typed
  // TX, byte-exactly per direction (RX may have been read in several chunks).
  QByteArray recordedRx, recordedTx;
  for (const SessionEvent &e : window.getDocument()->offlineSession()->events) {
    if (e.type != SessionEventType::Data) continue;
    (e.direction == SessionDirection::Rx ? recordedRx : recordedTx).append(e.payload);
  }
  QCOMPARE(recordedRx, rx);
  QCOMPARE(recordedTx, QByteArrayLiteral("\x1b[0n") + QByteArrayLiteral("typed\r"));

  serial->setDeviceName(slaveName);
  QVERIFY(serial->open());   // test seam, as above
  QSignalSpy written(serial, &ITransport::bytesWritten);
  auto *combo = window.findChild<QComboBox *>(QStringLiteral("replayTimingCombo"));
  combo->setCurrentIndex(1);
  emit combo->activated(1);
  window.slotReplayPlay();
  SessionReplayPlayer *player = window.getDocument()->getReplayPlayer();
  QTRY_COMPARE(player->state(), SessionReplayState::Finished);
  QTest::qWait(150);

  QCOMPARE(written.count(), 0);
  QString why;
  QVERIFY2(peerStaysSilent(masterFd, 300, &why), qPrintable(why));
}

void TstSessionReplayUi::aReplayRunsToItsEndAndReportsIt()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));
  KomportApp window;
  QVERIFY(window.loadSessionForReplayFile(path));
  auto *combo = window.findChild<QComboBox *>(QStringLiteral("replayTimingCombo"));
  combo->setCurrentIndex(1);
  emit combo->activated(1);
  action(&window, "replayPlay")->trigger();
  SessionReplayPlayer *player = window.getDocument()->getReplayPlayer();
  QTRY_COMPARE(player->state(), SessionReplayState::Finished);
  QVERIFY2(statusLabel(&window)->text().startsWith(QStringLiteral("Replay finished: 2 event")),
           qPrintable(statusLabel(&window)->text()));
  QVERIFY(!action(&window, "replayPlay")->isEnabled());
  QVERIFY(!action(&window, "replayStop")->isEnabled());
}

void TstSessionReplayUi::closingTheOfflineSessionStopsARunningReplay()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));
  KomportApp window;
  QVERIFY(window.loadSessionForReplayFile(path));
  window.slotReplayPlay();
  SessionReplayPlayer *player = window.getDocument()->getReplayPlayer();
  QCOMPARE(player->state(), SessionReplayState::Playing);
  QPointer<SessionReplayPlayer> watch(player);

  window.slotCloseOfflineSession();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QTest::qWait(50);

  QVERIFY(window.getDocument()->getReplayPlayer() == nullptr);
  QVERIFY(watch.isNull());   // destroyed, nothing delivered afterwards
  // No replay report reached the user: only the close message.
  QCOMPARE(statusLabel(&window)->text(), QStringLiteral("Offline session closed."));
  QVERIFY(!action(&window, "replayStop")->isEnabled());
  QVERIFY(window.findChild<QToolBar *>(QStringLiteral("replayToolBar"))->isHidden());
}


// SPEC-M10 5.11 / review 2026-11-15 finding 2: the player's and the document's signals are
// synchronous, so a receiver can delete the whole window while an application slot is still
// inside its call. The slot must then neither touch the freed window nor report anything.
void TstSessionReplayUi::aReceiverThatDeletesTheWindowDuringAnAppSlotIsHarmless()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));
  QVERIFY(!path.isEmpty());

  enum class Operation { Load, Close, Play, Stop, StepEvent, StepRx, StepTx };
  const auto run = [&](Operation operation) {
    QObject receivers;
    auto *window = new KomportApp();
    QPointer<KomportApp> watch(window);
    // Once only: the window's own destructor leaves the offline state and emits again.
    bool deleting = false;
    const auto destroyWindow = [window, &watch, &deleting]() {
      if (watch && !deleting) { deleting = true; delete window; }
    };
    KomportDoc *doc = window->getDocument();
    if (operation == Operation::Load) {
      connect(doc, &KomportDoc::offlineSessionChanged, &receivers, destroyWindow);
      window->loadSessionForReplayFile(path);
      QVERIFY2(watch.isNull(), "the load must complete without touching the deleted window");
      return;
    }
    QVERIFY(window->loadSessionForReplayFile(path));
    SessionReplayPlayer *player = doc->getReplayPlayer();
    QVERIFY(player != nullptr);
    if (operation == Operation::Stop) {
      QVERIFY(player->play().ok);
      QCOMPARE(player->state(), SessionReplayState::Playing);
    }
    switch (operation) {
    case Operation::Close:
      connect(doc, &KomportDoc::offlineSessionChanged, &receivers, destroyWindow);
      window->slotCloseOfflineSession();
      break;
    case Operation::Play:
      connect(player, &SessionReplayPlayer::stateChanged, &receivers, destroyWindow);
      window->slotReplayPlay();
      break;
    case Operation::Stop:
      connect(player, &SessionReplayPlayer::stateChanged, &receivers, destroyWindow);
      window->slotReplayStop();
      break;
    case Operation::StepEvent:
      connect(player, &SessionReplayPlayer::eventDelivered, &receivers, destroyWindow);
      window->slotReplayStepEvent();
      break;
    case Operation::StepRx:
      connect(player, &SessionReplayPlayer::eventDelivered, &receivers, destroyWindow);
      window->slotReplayStepRx();
      break;
    case Operation::StepTx:
      connect(player, &SessionReplayPlayer::eventDelivered, &receivers, destroyWindow);
      window->slotReplayStepTx();
      break;
    case Operation::Load:
      break;
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY2(watch.isNull(), "the receiver deleted the window during the slot");
  };
  for (Operation operation : { Operation::Load, Operation::Close, Operation::Play, Operation::Stop,
                               Operation::StepEvent, Operation::StepRx, Operation::StepTx })
    run(operation);
}

QTEST_MAIN(TstSessionReplayUi)
#include "tst_sessionreplayui.moc"
