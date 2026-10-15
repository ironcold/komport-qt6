/***************************************************************************
                  tst_sessionofflinedocument.cpp  -  Komport Offline Session
                             -------------------
    begin                : 2026
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de

    M10 step 5: the document-level offline state of SPEC-M10 5.7 and 5.9 -
    ownership of the loaded session and its replay player, the process-wide
    loading preconditions (a live session, a running recording or a loaded
    session anywhere in the application refuses the load without disturbing
    what it refuses to replace), and the close path. No window chrome is
    needed for the preconditions, which is the point of enforcing them in
    KomportDoc::loadOfflineSession(); two tests build real offscreen
    windows to prove the walk is process-wide.
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
#include "sessionrecordcodec.h"

#include <QCoreApplication>
#include <QEvent>
#include <QPointer>
#include <QScopeGuard>
#include <QSettings>
#include <QSignalSpy>
#include <QTest>

#include <QByteArray>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QTemporaryDir>

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

/** A complete, conformant local configuration snapshot (ADR-010 D7). */
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
QString writeSessionFile(const QTemporaryDir &directory, const QString &name,
                         const QByteArray &prefixOverride = QByteArray())
{
  const SessionRecordEncoding header = SessionRecordCodec::encodeHeader(headerFacts());
  if (!header.ok)
    return QString();
  QByteArray file = prefixOverride.isEmpty() ? header.bytes : prefixOverride;
  const SessionRecordEncoding first = SessionRecordCodec::encodeRecord(
      dataEvent(1, SessionDirection::Rx, QByteArrayLiteral("hello\r\n"), 250));
  const SessionRecordEncoding second = SessionRecordCodec::encodeRecord(
      dataEvent(2, SessionDirection::Tx, QByteArrayLiteral("ls\r"), 500));
  if (!first.ok || !second.ok)
    return QString();
  file.append(first.bytes);
  file.append(second.bytes);
  const QString path = directory.filePath(name);
  QFile out(path);
  if (!out.open(QIODevice::WriteOnly))
    return QString();
  out.write(file);
  return path;
}

QByteArray fileBytes(const QString &path)
{
  QFile in(path);
  return in.open(QIODevice::ReadOnly) ? in.readAll() : QByteArray();
}

} // namespace

class TstSessionOfflineDocument : public QObject
{
  Q_OBJECT
private slots:
  void initTestCase();
  void cleanupTestCase();

  void aValidFileEntersTheOfflineStateWithoutReplayingOrOpeningAPort();
  void aRefusedLoadReportsItsReasonAndEntersNoOfflineState();
  void loadingIsRefusedWhileTheSessionIsLive();
  void loadingIsRefusedWhileARecordingRuns();
  void loadingIsRefusedWhileAnOfflineSessionIsLoaded();
  void loadingIsRefusedWhileAnotherWindowsSessionIsLive();
  void loadingIsRefusedWhileAnotherWindowHasASessionLoaded();
  void recordedSettingsAreNeverApplied();
  void theFileIsUnchangedByLoading();
  void closingTheOfflineSessionReleasesEverythingAndAllowsALoadAgain();
  void closingTheOfflineSessionStopsARunningReplay();
  void destroyingTheDocumentWhileOfflineIsSilent();
  void aReceiverThatClosesDuringTheCloseIsHarmless();
  void aReceiverThatReloadsAfterTheCloseKeepsItsSession();
  void aReceiverThatDeletesTheDocumentDuringThePlayerCloseIsHarmless();
  void aReceiverThatDeletesTheDocumentOnTheClosedNotificationIsHarmless();
  void closingFromTheStateChangedPlayingSignalOfPlayIsHarmless();
  void aReceiverOfTheClosedNotificationCannotRestartTheRetiredPlayer();
  void closingThenSpinningANestedLoopInsidePlayIsHarmless();
  void closingFromAnEventDeliveredReceiverIsHarmless();
  void closingFromTheReplayFinishedReceiverIsHarmless();
  void deletingTheDocumentFromAnEventDeliveredReceiverIsHarmless();

private:
  QString mTestConfigFile;
};

void TstSessionOfflineDocument::initTestCase()
{
  // The two window tests build real KomportApp windows, which read and write
  // QSettings on construction: keep that away from a real user's profiles.
  QCoreApplication::setOrganizationName(QStringLiteral("Komport-Qt6-Test"));
  QCoreApplication::setApplicationName(QStringLiteral("Komport-Qt6-Test"));
  mTestConfigFile = QSettings().fileName();
}

void TstSessionOfflineDocument::cleanupTestCase()
{
  QFile::remove(mTestConfigFile);
}

void TstSessionOfflineDocument::aValidFileEntersTheOfflineStateWithoutReplayingOrOpeningAPort()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));
  QVERIFY(!path.isEmpty());

  KomportDoc document(nullptr);
  QSignalSpy changed(&document, &KomportDoc::offlineSessionChanged);
  QCOMPARE(document.hasOfflineSession(), false);
  QVERIFY(document.getReplayPlayer() == nullptr);

  const SessionLoadOutcome outcome = document.loadOfflineSession(path);
  QVERIFY2(outcome.ok, qPrintable(outcome.reason));
  QCOMPARE(outcome.eventCount, quint64(2));

  QCOMPARE(document.hasOfflineSession(), true);
  QCOMPARE(document.isAnyOfflineSession(), true);
  QVERIFY(document.offlineSession() != nullptr);
  QCOMPARE(changed.count(), 1);
  QCOMPARE(changed.at(0).at(0).toBool(), true);

  // No replay starts automatically on load (SPEC-M10 5.7): the player is attached
  // and positioned at the first event, nothing is delivered.
  SessionReplayPlayer *player = document.getReplayPlayer();
  QVERIFY(player != nullptr);
  QCOMPARE(static_cast<int>(player->state()), static_cast<int>(SessionReplayState::Ready));
  QCOMPARE(player->eventCount(), quint64(2));
  QCOMPARE(player->position(), quint64(0));
  QCOMPARE(player->deliveredCount(), quint64(0));

  // The player and the document share the one immutable session.
  QCOMPARE(document.offlineSession().use_count() >= 2, true);

  // The live session machinery is untouched: the transport is closed, the
  // controller idle.
  QCOMPARE(document.getSerial()->isOpen(), false);
  QCOMPARE(static_cast<int>(document.getSessionController()->state()),
           static_cast<int>(SessionController::State::Idle));
}

void TstSessionOfflineDocument::aRefusedLoadReportsItsReasonAndEntersNoOfflineState()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  KomportDoc document(nullptr);
  QSignalSpy changed(&document, &KomportDoc::offlineSessionChanged);

  // Missing file, bad magic, and a multi-source-free but non-session file.
  const QString missing = directory.filePath(QStringLiteral("missing.kpsession"));
  const SessionLoadOutcome missingOutcome = document.loadOfflineSession(missing);
  QCOMPARE(missingOutcome.ok, false);
  QVERIFY(!missingOutcome.reason.isEmpty());
  QVERIFY(missingOutcome.session == nullptr);

  const QString garbage = directory.filePath(QStringLiteral("garbage.kpsession"));
  {
    QFile out(garbage);
    QVERIFY(out.open(QIODevice::WriteOnly));
    out.write(QByteArray("this is not a session file at all, not even close"));
  }
  const SessionLoadOutcome garbageOutcome = document.loadOfflineSession(garbage);
  QCOMPARE(garbageOutcome.ok, false);
  QVERIFY(!garbageOutcome.reason.isEmpty());

  QCOMPARE(document.hasOfflineSession(), false);
  QVERIFY(document.getReplayPlayer() == nullptr);
  QCOMPARE(changed.count(), 0);
}

void TstSessionOfflineDocument::loadingIsRefusedWhileTheSessionIsLive()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if (slaveName.isEmpty())
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));

  KomportDoc document(nullptr);
  document.getSerial()->setDeviceName(slaveName);
  document.getSerial()->setBaudRate(qint32(9600));
  QVERIFY(document.getSerial()->open());

  const SessionLoadOutcome outcome = document.loadOfflineSession(path);
  QCOMPARE(outcome.ok, false);
  QVERIFY2(outcome.reason.contains(QStringLiteral("live session")), qPrintable(outcome.reason));
  QCOMPARE(document.hasOfflineSession(), false);

  // The refusal disturbed nothing: the live session is still live and open.
  QCOMPARE(document.getSerial()->isOpen(), true);
  QCOMPARE(static_cast<int>(document.getSessionController()->state()),
           static_cast<int>(SessionController::State::Live));
}

void TstSessionOfflineDocument::loadingIsRefusedWhileARecordingRuns()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if (slaveName.isEmpty())
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));

  KomportDoc document(nullptr);
  document.getSerial()->setDeviceName(slaveName);
  document.getSerial()->setBaudRate(qint32(9600));
  QVERIFY(document.getSerial()->open());

  SessionRecordingRequest request;
  request.path = directory.filePath(QStringLiteral("running.kpsession"));
  request.applicationName = QStringLiteral("Komport");
  request.applicationVersion = QStringLiteral("test");
  request.configuration = document.getSerial()->appliedConfigurationSnapshot();
  const SessionRecordingStart started = document.getSessionRecorder()->start(request);
  QVERIFY2(started.ok, qPrintable(started.reason));

  // Close only the transport: the controller is idle again, the recording still
  // runs. This isolates the recording precondition from the live one.
  document.getSerial()->close();
  QCOMPARE(static_cast<int>(document.getSessionController()->state()),
           static_cast<int>(SessionController::State::Idle));
  QVERIFY(document.getSessionRecorder()->state() != SessionRecorder::State::Stopped);

  const SessionLoadOutcome outcome = document.loadOfflineSession(path);
  QCOMPARE(outcome.ok, false);
  QVERIFY2(outcome.reason.contains(QStringLiteral("recording is running")),
           qPrintable(outcome.reason));
  QCOMPARE(document.hasOfflineSession(), false);
  // The refusal did not stop the recording.
  QVERIFY(document.getSessionRecorder()->state() != SessionRecorder::State::Stopped);

  document.getSessionRecorder()->stop();
}

void TstSessionOfflineDocument::loadingIsRefusedWhileAnOfflineSessionIsLoaded()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));
  const QString other = writeSessionFile(directory, QStringLiteral("other.kpsession"));

  KomportDoc document(nullptr);
  QVERIFY(document.loadOfflineSession(path).ok);
  SessionReplayPlayer *player = document.getReplayPlayer();
  const SessionFilePtr loaded = document.offlineSession();
  QVERIFY(player->stepNextEvent().ok);
  QCOMPARE(player->position(), quint64(1));

  QSignalSpy changed(&document, &KomportDoc::offlineSessionChanged);
  const SessionLoadOutcome second = document.loadOfflineSession(other);
  QCOMPARE(second.ok, false);
  QVERIFY2(second.reason.contains(QStringLiteral("already loaded")), qPrintable(second.reason));

  // The loaded session, its player and its position are unchanged.
  QCOMPARE(changed.count(), 0);
  QVERIFY(document.getReplayPlayer() == player);
  QVERIFY(document.offlineSession() == loaded);
  QCOMPARE(player->position(), quint64(1));
  QCOMPARE(player->deliveredCount(), quint64(1));
}

void TstSessionOfflineDocument::loadingIsRefusedWhileAnotherWindowsSessionIsLive()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if (slaveName.isEmpty())
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));

  // Window A holds the live session; the loading document is a different one.
  auto *windowA = new KomportApp();
  windowA->setAttribute(Qt::WA_DeleteOnClose, false);
  auto windowGuard = qScopeGuard([windowA]() { windowA->getDocument()->closeSession(); delete windowA; });
  KomportDoc *docA = windowA->getDocument();
  docA->getSerial()->close();
  docA->getSerial()->setDeviceName(slaveName);
  docA->getSerial()->setBaudRate(qint32(9600));
  QVERIFY(docA->getSerial()->open());
  QCOMPARE(static_cast<int>(docA->getSessionController()->state()),
           static_cast<int>(SessionController::State::Live));

  KomportDoc documentB(nullptr);
  const SessionLoadOutcome outcome = documentB.loadOfflineSession(path);
  QCOMPARE(outcome.ok, false);
  QVERIFY2(outcome.reason.contains(QStringLiteral("live session")), qPrintable(outcome.reason));
  QCOMPARE(documentB.hasOfflineSession(), false);

  // A's session is untouched.
  QCOMPARE(docA->getSerial()->isOpen(), true);
  QCOMPARE(static_cast<int>(docA->getSessionController()->state()),
           static_cast<int>(SessionController::State::Live));
}

void TstSessionOfflineDocument::loadingIsRefusedWhileAnotherWindowHasASessionLoaded()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));

  auto *windowA = new KomportApp();
  windowA->setAttribute(Qt::WA_DeleteOnClose, false);
  auto windowGuard = qScopeGuard([windowA]() { delete windowA; });
  KomportDoc *docA = windowA->getDocument();
  // A window opens its configured port on construction in some environments;
  // make sure the precondition under test is the offline one.
  docA->getSerial()->close();
  QVERIFY2(docA->loadOfflineSession(path).ok, "window A could not load");

  KomportDoc documentB(nullptr);
  QCOMPARE(documentB.hasOfflineSession(), false);
  QCOMPARE(documentB.isAnyOfflineSession(), true);   // the process-wide predicate sees A
  const SessionLoadOutcome outcome = documentB.loadOfflineSession(path);
  QCOMPARE(outcome.ok, false);
  QVERIFY2(outcome.reason.contains(QStringLiteral("already loaded")), qPrintable(outcome.reason));
  QCOMPARE(documentB.hasOfflineSession(), false);
  QCOMPARE(docA->hasOfflineSession(), true);

  docA->closeOfflineSession();
  QCOMPARE(documentB.isAnyOfflineSession(), false);
}

void TstSessionOfflineDocument::recordedSettingsAreNeverApplied()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));

  KomportDoc document(nullptr);
  document.getSerial()->setDeviceName(QStringLiteral("/dev/not-the-recorded-one"));
  document.getSerial()->setBaudRate(qint32(9600));
  QVERIFY(document.loadOfflineSession(path).ok);

  // The file recorded another endpoint and 115200 baud; neither was applied, and the
  // port was neither opened nor reconfigured.
  QCOMPARE(document.getSerial()->isOpen(), false);
  QCOMPARE(document.getSerial()->deviceName(), QStringLiteral("/dev/not-the-recorded-one"));
  QCOMPARE(document.getSerial()->baudRate(), qint32(9600));

  document.closeOfflineSession();
  QCOMPARE(document.getSerial()->isOpen(), false);
  QCOMPARE(document.getSerial()->deviceName(), QStringLiteral("/dev/not-the-recorded-one"));
  QCOMPARE(document.getSerial()->baudRate(), qint32(9600));
}

void TstSessionOfflineDocument::theFileIsUnchangedByLoading()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));
  const QByteArray before = fileBytes(path);
  const QDateTime modifiedBefore = QFileInfo(path).lastModified();
  QVERIFY(!before.isEmpty());

  KomportDoc document(nullptr);
  QVERIFY(document.loadOfflineSession(path).ok);
  QCOMPARE(fileBytes(path), before);
  QCOMPARE(QFileInfo(path).lastModified(), modifiedBefore);

  // The reader keeps no handle (ADR-011 D10): the file can be removed while the
  // session stays loaded.
  QVERIFY(QFile::remove(path));
  QCOMPARE(document.hasOfflineSession(), true);
  QCOMPARE(document.getReplayPlayer()->eventCount(), quint64(2));
}

void TstSessionOfflineDocument::closingTheOfflineSessionReleasesEverythingAndAllowsALoadAgain()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));

  KomportDoc document(nullptr);
  document.closeOfflineSession();   // a no-op while nothing is loaded
  QVERIFY(document.loadOfflineSession(path).ok);

  std::weak_ptr<const SessionFile> weak = document.offlineSession();
  QPointer<SessionReplayPlayer> playerGuard(document.getReplayPlayer());
  QSignalSpy changed(&document, &KomportDoc::offlineSessionChanged);
  document.closeOfflineSession();

  // No player call was on the stack, so the player is gone when the close returns -
  // without any event-loop turn (SPEC-M10 5.11 rule 11).
  QVERIFY(playerGuard.isNull());
  QCOMPARE(changed.count(), 1);
  QCOMPARE(changed.at(0).at(0).toBool(), false);
  QCOMPARE(document.hasOfflineSession(), false);
  QCOMPARE(document.isAnyOfflineSession(), false);
  QVERIFY(document.getReplayPlayer() == nullptr);
  QVERIFY(document.offlineSession() == nullptr);
  QVERIFY2(weak.expired(), "the shared session must be released once the player is gone");

  // Idempotent, and a load is permitted again.
  document.closeOfflineSession();
  QCOMPARE(changed.count(), 1);
  QVERIFY(document.loadOfflineSession(path).ok);
}

void TstSessionOfflineDocument::closingTheOfflineSessionStopsARunningReplay()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));

  KomportDoc document(nullptr);
  QVERIFY(document.loadOfflineSession(path).ok);
  SessionReplayPlayer *player = document.getReplayPlayer();
  QVERIFY(player->setTiming(SessionReplayTiming::Original).ok);
  QVERIFY(player->play().ok);
  QCOMPARE(static_cast<int>(player->state()), static_cast<int>(SessionReplayState::Playing));

  QSignalSpy delivered(player, &SessionReplayPlayer::eventDelivered);
  QSignalSpy finished(player, &SessionReplayPlayer::replayFinished);
  document.closeOfflineSession();

  // No report to the user, and nothing is delivered afterwards, however long the
  // event loop runs (the pending delivery was cancelled).
  QTest::qWait(600);
  QCOMPARE(delivered.count(), 0);
  QCOMPARE(finished.count(), 0);
  QCOMPARE(document.hasOfflineSession(), false);
}

void TstSessionOfflineDocument::destroyingTheDocumentWhileOfflineIsSilent()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));

  auto *document = new KomportDoc(nullptr);
  QVERIFY(document->loadOfflineSession(path).ok);
  QVERIFY(document->getReplayPlayer()->play().ok);
  std::weak_ptr<const SessionFile> weak = document->offlineSession();
  QSignalSpy changed(document, &KomportDoc::offlineSessionChanged);

  delete document;   // player -> session -> transport; nothing is emitted
  QTest::qWait(600);
  QVERIFY(weak.expired());
  QCOMPARE(changed.count(), 0);
}

void TstSessionOfflineDocument::aReceiverThatClosesDuringTheCloseIsHarmless()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));
  const QString other = writeSessionFile(directory, QStringLiteral("other.kpsession"));

  KomportDoc document(nullptr);
  QVERIFY(document.loadOfflineSession(path).ok);
  SessionReplayPlayer *player = document.getReplayPlayer();
  QVERIFY(player->play().ok);

  // The player's close() announces Idle synchronously; the receiver closes again
  // and tries a load, both from inside the outer close.
  int stateChanges = 0;
  bool nestedLoadOk = true;
  QObject::connect(player, &SessionReplayPlayer::stateChanged, &document,
                   [&](SessionReplayState state) {
                     if (state != SessionReplayState::Idle)
                       return;
                     ++stateChanges;
                     document.closeOfflineSession();                      // must be a no-op
                     nestedLoadOk = document.loadOfflineSession(other).ok;   // still "loaded"
                   });
  QSignalSpy changed(&document, &KomportDoc::offlineSessionChanged);

  document.closeOfflineSession();

  QCOMPARE(stateChanges, 1);
  QCOMPARE(nestedLoadOk, false);
  QCOMPARE(changed.count(), 1);                // exactly one false transition
  QCOMPARE(changed.at(0).at(0).toBool(), false);
  QCOMPARE(document.hasOfflineSession(), false);
  QVERIFY(document.getReplayPlayer() == nullptr);
}

void TstSessionOfflineDocument::aReceiverThatReloadsAfterTheCloseKeepsItsSession()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));
  const QString other = writeSessionFile(directory, QStringLiteral("other.kpsession"));

  KomportDoc document(nullptr);
  QVERIFY(document.loadOfflineSession(path).ok);

  // The close announces its completion last; a receiver loading another session
  // from that notification must end with that session loaded, not discarded by the
  // close that is still unwinding.
  bool reloadOk = false;
  QList<bool> transitions;
  QObject::connect(&document, &KomportDoc::offlineSessionChanged, &document,
                   [&](bool offline) {
                     transitions.append(offline);
                     if (!offline && !reloadOk)
                       reloadOk = document.loadOfflineSession(other).ok;
                   });

  document.closeOfflineSession();

  QVERIFY(reloadOk);
  QCOMPARE(transitions, (QList<bool>{false, true}));
  QCOMPARE(document.hasOfflineSession(), true);
  QVERIFY(document.getReplayPlayer() != nullptr);
  QCOMPARE(document.getReplayPlayer()->eventCount(), quint64(2));
}

void TstSessionOfflineDocument::aReceiverThatDeletesTheDocumentDuringThePlayerCloseIsHarmless()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));

  auto *document = new KomportDoc(nullptr);
  QPointer<KomportDoc> guard(document);
  QVERIFY(document->loadOfflineSession(path).ok);
  SessionReplayPlayer *player = document->getReplayPlayer();
  QVERIFY(player->play().ok);

  // The player announces Idle from inside its own close(); the receiver deletes the
  // owning document right there. The player must survive its close() frame and the
  // call must return without touching the freed document (run under ASan to see it).
  int stateChanges = 0;
  QObject::connect(player, &SessionReplayPlayer::stateChanged, this,
                   [&](SessionReplayState state) {
                     if (state != SessionReplayState::Idle)
                       return;
                     ++stateChanges;
                     delete document;
                   });
  document->closeOfflineSession();

  QCOMPARE(stateChanges, 1);
  QVERIFY(guard.isNull());
  QTest::qWait(300);   // nothing pending touches the deleted document
}

void TstSessionOfflineDocument::aReceiverThatDeletesTheDocumentOnTheClosedNotificationIsHarmless()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));

  auto *document = new KomportDoc(nullptr);
  QPointer<KomportDoc> guard(document);
  QVERIFY(document->loadOfflineSession(path).ok);

  int notifications = 0;
  QObject::connect(document, &KomportDoc::offlineSessionChanged, this,
                   [&](bool offline) {
                     if (!offline) {
                       ++notifications;
                       delete document;
                     }
                   });
  document->closeOfflineSession();

  QCOMPARE(notifications, 1);
  QVERIFY(guard.isNull());
}

// The player's own call resumes after each of its synchronous signals. Closing the
// document (which retires the player) or deleting it from inside such a signal must
// not free the player under that frame; the player is deleted when the stack has
// unwound. These are meant to be run under ASan as well.
void TstSessionOfflineDocument::closingFromTheStateChangedPlayingSignalOfPlayIsHarmless()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));

  KomportDoc document(nullptr);
  QVERIFY(document.loadOfflineSession(path).ok);
  SessionReplayPlayer *rawPlayer = document.getReplayPlayer();
  QPointer<SessionReplayPlayer> player(rawPlayer);
  QVERIFY(rawPlayer->setTiming(SessionReplayTiming::Immediate).ok);

  int closes = 0;
  QObject::connect(rawPlayer, &SessionReplayPlayer::stateChanged, this,
                   [&](SessionReplayState state) {
                     if (state == SessionReplayState::Playing) {
                       ++closes;
                       document.closeOfflineSession();
                     }
                   });
  rawPlayer->play();   // continues in play() after the receiver closed the document

  QCOMPARE(closes, 1);
  QCOMPARE(document.hasOfflineSession(), false);
  QTRY_VERIFY_WITH_TIMEOUT(player.isNull(), 2000);
}

void TstSessionOfflineDocument::aReceiverOfTheClosedNotificationCannotRestartTheRetiredPlayer()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));

  KomportDoc document(nullptr);
  QVERIFY(document.loadOfflineSession(path).ok);
  SessionReplayPlayer *player = document.getReplayPlayer();
  QPointer<SessionReplayPlayer> guard(player);
  const SessionFilePtr session = document.offlineSession();
  QVERIFY(player->play().ok);

  // The Idle receiver holds the public player pointer and the session: it must not
  // be able to attach the session again or arm a delivery.
  bool anyAccepted = false;
  QObject::connect(player, &SessionReplayPlayer::stateChanged, this,
                   [&](SessionReplayState state) {
                     if (state == SessionReplayState::Idle)
                       anyAccepted = player->start(session).ok || player->play().ok;
                   });
  QSignalSpy delivered(player, &SessionReplayPlayer::eventDelivered);
  document.closeOfflineSession();

  QVERIFY(!anyAccepted);
  QTest::qWait(300);                     // an armed delivery would have fired by now
  QCOMPARE(delivered.count(), 0);
  QVERIFY(guard.isNull());               // and the retired player was deleted
}

void TstSessionOfflineDocument::closingThenSpinningANestedLoopInsidePlayIsHarmless()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));

  KomportDoc document(nullptr);
  QVERIFY(document.loadOfflineSession(path).ok);
  SessionReplayPlayer *player = document.getReplayPlayer();
  QPointer<SessionReplayPlayer> guard(player);

  bool aliveInside = false;
  QObject::connect(player, &SessionReplayPlayer::stateChanged, this,
                   [&](SessionReplayState state) {
                     if (state != SessionReplayState::Playing)
                       return;
                     document.closeOfflineSession();
                     QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                     aliveInside = !guard.isNull();   // play() is still on the stack
                   });
  player->play();   // resumes in play() after the receiver and its nested flush
  QVERIFY(aliveInside);
  QTRY_VERIFY_WITH_TIMEOUT(guard.isNull(), 2000);
}

void TstSessionOfflineDocument::closingFromAnEventDeliveredReceiverIsHarmless()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));

  KomportDoc document(nullptr);
  QVERIFY(document.loadOfflineSession(path).ok);
  SessionReplayPlayer *rawPlayer = document.getReplayPlayer();
  QPointer<SessionReplayPlayer> player(rawPlayer);
  QVERIFY(rawPlayer->setTiming(SessionReplayTiming::Immediate).ok);

  int delivered = 0;
  QObject::connect(rawPlayer, &SessionReplayPlayer::eventDelivered, this,
                   [&](const SessionEvent &) {
                     ++delivered;
                     document.closeOfflineSession();
                   });
  QVERIFY(rawPlayer->play().ok);
  QTRY_VERIFY_WITH_TIMEOUT(player.isNull(), 2000);

  // The close is final: no second event follows it.
  QCOMPARE(delivered, 1);
  QCOMPARE(document.hasOfflineSession(), false);
}

void TstSessionOfflineDocument::closingFromTheReplayFinishedReceiverIsHarmless()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));

  KomportDoc document(nullptr);
  QVERIFY(document.loadOfflineSession(path).ok);
  SessionReplayPlayer *rawPlayer = document.getReplayPlayer();
  QPointer<SessionReplayPlayer> player(rawPlayer);
  QVERIFY(rawPlayer->setTiming(SessionReplayTiming::Immediate).ok);

  int finished = 0;
  QObject::connect(rawPlayer, &SessionReplayPlayer::replayFinished, this,
                   [&](const SessionReplayReport &) {
                     ++finished;
                     document.closeOfflineSession();
                   });
  QVERIFY(rawPlayer->play().ok);
  QTRY_VERIFY_WITH_TIMEOUT(player.isNull(), 2000);

  QCOMPARE(finished, 1);
  QCOMPARE(document.hasOfflineSession(), false);
}

void TstSessionOfflineDocument::deletingTheDocumentFromAnEventDeliveredReceiverIsHarmless()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = writeSessionFile(directory, QStringLiteral("ok.kpsession"));

  auto *document = new KomportDoc(nullptr);
  QPointer<KomportDoc> documentGuard(document);
  QVERIFY(document->loadOfflineSession(path).ok);
  SessionReplayPlayer *rawPlayer = document->getReplayPlayer();
  QPointer<SessionReplayPlayer> player(rawPlayer);
  QVERIFY(rawPlayer->setTiming(SessionReplayTiming::Immediate).ok);

  int delivered = 0;
  QObject::connect(rawPlayer, &SessionReplayPlayer::eventDelivered, this,
                   [&](const SessionEvent &) {
                     ++delivered;
                     delete document;   // destroys the transport while the player's frame is live
                   });
  QVERIFY(rawPlayer->play().ok);
  QTRY_VERIFY_WITH_TIMEOUT(player.isNull(), 2000);

  QCOMPARE(delivered, 1);
  QVERIFY(documentGuard.isNull());
}

QTEST_MAIN(TstSessionOfflineDocument)
#include "tst_sessionofflinedocument.moc"
