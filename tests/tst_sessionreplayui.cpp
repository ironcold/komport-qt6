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
#include "komportmacrobar.h"
#include "komportserial.h"
#include "komportview.h"
#include "sessionrecordcodec.h"

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QScopeGuard>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

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
  char buffer[64];
  QCOMPARE(::read(masterFd, buffer, sizeof buffer) <= 0, true);   // the peer received nothing
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
  char buffer[64];
  QCOMPARE(::read(masterFd, buffer, sizeof buffer) <= 0, true);
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

QTEST_MAIN(TstSessionReplayUi)
#include "tst_sessionreplayui.moc"
