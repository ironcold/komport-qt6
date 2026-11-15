/***************************************************************************
                tst_sessionreplayendtoend.cpp  -  Komport M10 end to end
                             ---------------------
    begin                : 2026-10-17
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// M10 step 8 (SPEC-M10 section 7, section 11): the end-to-end proof over a real
// pty. A session recorded by M9's recorder beside a live window - RX, TX and a
// terminal query that the live emulation answered - is loaded by the M10 reader
// and replayed with `Immediate`; what the consumer receives is byte-exact what
// the file holds, the file also parses with M9's independent test-side reader,
// and the pty peer is silent during load and replay. A truncated recording
// follows ADR-006's recovery rule.

#include "komport.h"
#include "komportdoc.h"
#include "komportserial.h"
#include "sessioncontroller.h"
#include "sessionreader.h"
#include "sessionrecorder.h"
#include "sessionreplayplayer.h"

#include "sessionrecordreader.h"

#include <QApplication>
#include <QByteArray>
#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QJsonObject>
#include <QList>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <pty.h>
#include <unistd.h>

namespace {

QString makePty(int *masterFd)
{
  int master = -1;
  int slave = -1;
  char name[256] = { 0 };
  if (::openpty(&master, &slave, name, nullptr, nullptr) != 0)
    return QString();
  ::close(slave);
  *masterFd = master;
  return QString::fromLocal8Bit(name);
}

/** The peer helpers below are only bounded on a non-blocking descriptor, so a
  * failure to set that up must fail the test rather than hang it. */
bool makeNonBlocking(int fd)
{
  const int flags = ::fcntl(fd, F_GETFL, 0);
  return flags >= 0 && ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

QByteArray everyByteValue()
{
  QByteArray pattern;
  for (int value = 0; value < 256; ++value)
    pattern.append(char(value));
  return pattern;
}

/** Bounded poll of the pty master: fails on any byte or read error, passes only
  * when the whole window elapsed with EAGAIN. */
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

struct Recorded {
  QString path;
  QByteArray rx;          ///< what the peer sent
  QByteArray tx;          ///< what the window transmitted (query reply, typed bytes, binary pattern)
  qint32 baudRate = 0;    ///< the recorded port's baud rate
  QString failure;
};

const QByteArray kQuery = QByteArrayLiteral("\x1b[5n");
const QByteArray kQueryReply = QByteArrayLiteral("\x1b[0n");
const QByteArray kTyped = QByteArrayLiteral("typed\r");

/** Records a live pty session in a window; the file ends up at @p path. */
Recorded recordSession(const QString &path, int masterFd, const QString &slaveName)
{
  Recorded result;
  result.path = path;
  KomportApp window;
  KomportDoc *document = window.getDocument();
  KomportSerial *serial = document->getSerial();
  serial->setDeviceName(slaveName);
  serial->setBaudRate(qint32(115200));
  if (!serial->open()) {
    result.failure = QStringLiteral("the port did not open");
    return result;
  }
  SessionRecordingRequest request;
  request.path = path;
  request.applicationName = QStringLiteral("Komport");
  request.applicationVersion = QStringLiteral("test");
  request.configuration = serial->appliedConfigurationSnapshot();
  const SessionRecordingStart started = document->getSessionRecorder()->start(request);
  if (!started.ok) {
    result.failure = started.reason;
    return result;
  }

  const QByteArray pattern = everyByteValue();
  // Binary payload in one burst, then a terminal query between text.
  const QByteArray first = pattern + QByteArrayLiteral("before");
  const QByteArray second = QByteArrayLiteral("after");
  ::write(masterFd, first.constData(), first.size());
  QTest::qWait(150);
  ::write(masterFd, kQuery.constData(), kQuery.size());
  const QByteArray reply = readPeer(masterFd, 5000, kQueryReply);   // the live emulation answers
  if (!reply.contains(kQueryReply)) {
    result.failure = QStringLiteral("the live emulation did not answer the query");
    return result;
  }
  ::write(masterFd, second.constData(), second.size());
  QTest::qWait(150);
  serial->writeBytes(kTyped);
  const QByteArray typed = readPeer(masterFd, 5000, kTyped);
  if (!typed.contains(kTyped)) {
    result.failure = QStringLiteral("the typed bytes did not reach the peer");
    return result;
  }
  // Binary TX: every byte value, NUL and the high bytes included.
  const QByteArray binaryTx = everyByteValue();
  if (serial->writeBytes(binaryTx) != qint64(binaryTx.size())) {
    result.failure = QStringLiteral("the binary TX was not accepted whole");
    return result;
  }
  if (readPeer(masterFd, 5000, binaryTx) != binaryTx) {
    result.failure = QStringLiteral("the binary TX did not reach the peer unchanged");
    return result;
  }
  QTest::qWait(100);

  const SessionRecordingReport report = document->getSessionRecorder()->stop();
  if (report.damaged) {
    result.failure = report.reason;
    return result;
  }
  serial->close();
  result.baudRate = serial->baudRate();
  result.rx = first + kQuery + second;
  result.tx = kQueryReply + kTyped + binaryTx;
  return result;
}

/** The concatenation of the Data payloads of one direction. */
template <typename Events>
QByteArray concatenated(const Events &events, SessionDirection direction)
{
  QByteArray out;
  for (const auto &event : events)
    if (event.type == SessionEventType::Data && event.direction == direction)
      out.append(event.payload);
  return out;
}

} // namespace

class TstSessionReplayEndToEnd : public QObject
{
  Q_OBJECT

private slots:
  void recordPtySessionThenLoadAndReplayByteExactly();
  void replayingARecordedPtySessionTransmitsNothing();
  void theRecoveryRuleSurvivesARealRecording();
};

void TstSessionReplayEndToEnd::recordPtySessionThenLoadAndReplayByteExactly()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if (slaveName.isEmpty())
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });
  QVERIFY2(makeNonBlocking(masterFd), "the pty master could not be made non-blocking");

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const Recorded recorded = recordSession(directory.filePath(QStringLiteral("e2e.kpsession")), masterFd, slaveName);
  QVERIFY2(recorded.failure.isEmpty(), qPrintable(recorded.failure));

  // The independent M9 test-side reader parses the same file.
  QFile file(recorded.path);
  QVERIFY(file.open(QIODevice::ReadOnly));
  const SessionRecordFile parsed = readSessionRecordFile(file.readAll());
  file.close();
  QVERIFY2(parsed.loadable, qPrintable(parsed.error));
  QCOMPARE(parsed.truncatedFinalRecord, false);
  QByteArray parsedRx, parsedTx;
  for (const SessionRecordFileRecord &record : parsed.records) {
    if (record.eventType != quint16(SessionEventType::Data))
      continue;
    (record.direction == quint8(SessionDirection::Rx) ? parsedRx : parsedTx).append(record.payload);
  }
  QCOMPARE(parsedRx, recorded.rx);
  QCOMPARE(parsedTx, recorded.tx);

  // The M10 reader loads it into one offline session.
  const SessionLoadOutcome outcome = SessionReader().load(recorded.path);
  QVERIFY2(outcome.ok, qPrintable(outcome.reason));
  QCOMPARE(outcome.recoveredTruncatedFinalRecord, false);
  QCOMPARE(outcome.eventCount, quint64(parsed.records.size()));
  QCOMPARE(outcome.session->events.size(), parsed.records.size());
  for (qsizetype index = 0; index < parsed.records.size(); ++index) {
    const SessionEvent &event = outcome.session->events.at(index);
    const SessionRecordFileRecord &record = parsed.records.at(index);
    QCOMPARE(event.sequence, record.sequence);
    QCOMPARE(quint16(event.type), record.eventType);
    QCOMPARE(quint8(event.direction), record.direction);
    QCOMPARE(event.payload, record.payload);
    QCOMPARE(event.timestampNs, record.timestampNs);
  }

  // Replayed with Immediate: the consumer sees exactly the stored events.
  SessionReplayPlayer player;
  QList<SessionEvent> delivered;
  connect(&player, &SessionReplayPlayer::eventDelivered, this,
          [&delivered](const SessionEvent &event) { delivered.append(event); });
  QVERIFY(player.start(outcome.session).ok);
  QVERIFY(player.setTiming(SessionReplayTiming::Immediate).ok);
  QVERIFY(player.play().ok);
  QTRY_COMPARE_WITH_TIMEOUT(player.state(), SessionReplayState::Finished, 5000);

  QCOMPARE(delivered.size(), outcome.session->events.size());
  for (qsizetype index = 0; index < delivered.size(); ++index) {
    QCOMPARE(delivered.at(index).sequence, outcome.session->events.at(index).sequence);
    QCOMPARE(delivered.at(index).type, outcome.session->events.at(index).type);
    QCOMPARE(delivered.at(index).direction, outcome.session->events.at(index).direction);
    QCOMPARE(delivered.at(index).payload, outcome.session->events.at(index).payload);
  }
  QCOMPARE(concatenated(delivered, SessionDirection::Rx), recorded.rx);
  QCOMPARE(concatenated(delivered, SessionDirection::Tx), recorded.tx);
}

void TstSessionReplayEndToEnd::replayingARecordedPtySessionTransmitsNothing()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if (slaveName.isEmpty())
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });
  QVERIFY2(makeNonBlocking(masterFd), "the pty master could not be made non-blocking");

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const Recorded recorded = recordSession(directory.filePath(QStringLiteral("silent.kpsession")), masterFd, slaveName);
  QVERIFY2(recorded.failure.isEmpty(), qPrintable(recorded.failure));
  // This very file holds the query and the reply the live emulation gave it.
  const SessionLoadOutcome stored = SessionReader().load(recorded.path);
  QVERIFY2(stored.ok, qPrintable(stored.reason));
  QVERIFY(concatenated(stored.session->events, SessionDirection::Rx).contains(kQuery));
  QVERIFY(concatenated(stored.session->events, SessionDirection::Tx).contains(kQueryReply));
  QString why;

  // The test holds the slave open so that the master reports silence (EAGAIN)
  // rather than EIO, and a fresh window is configured for that pty but left
  // closed before it loads: a load that opened, configured or wrote to the
  // transport would show at the peer, in the spy or in the port state.
  const int slaveFd = ::open(qPrintable(slaveName), O_RDWR | O_NOCTTY);
  QVERIFY(slaveFd >= 0);
  auto slaveGuard = qScopeGuard([slaveFd]() { ::close(slaveFd); });
  KomportApp window;
  KomportSerial *serial = window.getDocument()->getSerial();
  serial->close();
  serial->setDeviceName(slaveName);
  serial->setBaudRate(qint32(9600));
  QCOMPARE(serial->baudRate(), qint32(9600));
  QVERIFY(recorded.baudRate != serial->baudRate());   // the recorded snapshot differs from this window's
  QSignalSpy written(serial, &ITransport::bytesWritten);
  QSignalSpy opened(serial, &ITransport::opened);
  QSignalSpy closed(serial, &ITransport::closed);
  QSignalSpy reconfigured(serial, &ITransport::configurationChanged);
  QSignalSpy failed(serial, &ITransport::transportError);
  const QJsonObject configurationBefore = serial->appliedConfigurationSnapshot();
  QVERIFY2(peerStaysSilent(masterFd, 50, &why), qPrintable(why));
  QVERIFY(window.loadSessionForReplayFile(recorded.path));
  QCOMPARE(serial->isOpen(), false);        // loading never opened the port
  QCOMPARE(serial->deviceName(), slaveName);
  QCOMPARE(serial->appliedConfigurationSnapshot(), configurationBefore);
  QCOMPARE(written.count(), 0);
  QCOMPARE(opened.count(), 0);              // not even transiently
  QCOMPARE(closed.count(), 0);
  QCOMPARE(reconfigured.count(), 0);
  QCOMPARE(failed.count(), 0);
  QVERIFY2(peerStaysSilent(masterFd, 200, &why), qPrintable(why));   // silent during/after load

  // Now a live port behind the window (test seam), so that a replay which wrote
  // anything would reach the peer.
  QVERIFY(serial->open());
  QVERIFY2(peerStaysSilent(masterFd, 50, &why), qPrintable(why));

  SessionReplayPlayer *player = window.getDocument()->getReplayPlayer();
  QVERIFY(player != nullptr);
  QCOMPARE(player->setTiming(SessionReplayTiming::Immediate).ok, true);
  window.slotReplayPlay();
  QTRY_COMPARE_WITH_TIMEOUT(player->state(), SessionReplayState::Finished, 5000);
  QTest::qWait(100);

  QCOMPARE(written.count(), 0);
  QVERIFY2(peerStaysSilent(masterFd, 300, &why), qPrintable(why));
  // Closing the offline session is the third phase of SPEC-M10 8's no-transport
  // claim: it must neither write to, close, reconfigure nor otherwise touch the port.
  const QJsonObject configurationBeforeClose = serial->appliedConfigurationSnapshot();
  const int openedBeforeClose = opened.count();
  const int reconfiguredBeforeClose = reconfigured.count();
  window.slotCloseOfflineSession();
  QCOMPARE(serial->isOpen(), true);          // the offline close leaves the transport as it was
  QCOMPARE(serial->appliedConfigurationSnapshot(), configurationBeforeClose);
  QCOMPARE(written.count(), 0);
  QCOMPARE(closed.count(), 0);
  QCOMPARE(opened.count(), openedBeforeClose);
  QCOMPARE(reconfigured.count(), reconfiguredBeforeClose);
  QCOMPARE(failed.count(), 0);
  QVERIFY2(peerStaysSilent(masterFd, 100, &why), qPrintable(why));
}

void TstSessionReplayEndToEnd::theRecoveryRuleSurvivesARealRecording()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if (slaveName.isEmpty())
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });
  QVERIFY2(makeNonBlocking(masterFd), "the pty master could not be made non-blocking");

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const Recorded recorded = recordSession(directory.filePath(QStringLiteral("whole.kpsession")), masterFd, slaveName);
  QVERIFY2(recorded.failure.isEmpty(), qPrintable(recorded.failure));

  const SessionLoadOutcome whole = SessionReader().load(recorded.path);
  QVERIFY2(whole.ok, qPrintable(whole.reason));
  QVERIFY(whole.eventCount >= 3);

  QFile file(recorded.path);
  QVERIFY(file.open(QIODevice::ReadOnly));
  const QByteArray bytes = file.readAll();
  file.close();

  // Cut inside the final record: the complete prefix is the session, the cut
  // record is reported and not delivered.
  const QString cutPath = directory.filePath(QStringLiteral("cut.kpsession"));
  QFile cut(cutPath);
  QVERIFY(cut.open(QIODevice::WriteOnly));
  QCOMPARE(cut.write(bytes.left(bytes.size() - 3)), qint64(bytes.size() - 3));
  cut.close();

  const SessionLoadOutcome recovered = SessionReader().load(cutPath);
  QVERIFY2(recovered.ok, qPrintable(recovered.reason));
  QCOMPARE(recovered.recoveredTruncatedFinalRecord, true);
  QCOMPARE(recovered.eventCount, whole.eventCount - 1);
  QCOMPARE(recovered.session->events.size(), whole.session->events.size() - 1);
  const auto sameEvent = [](const SessionEvent &a, const SessionEvent &b) {
    return a.sequence == b.sequence && a.sourceId == b.sourceId &&
           a.sourceTimestampNs == b.sourceTimestampNs && a.timestampNs == b.timestampNs &&
           a.type == b.type && a.direction == b.direction && a.payload == b.payload &&
           a.metadata == b.metadata;
  };
  for (qsizetype index = 0; index < recovered.session->events.size(); ++index)
    QVERIFY2(sameEvent(recovered.session->events.at(index), whole.session->events.at(index)),
             qPrintable(QStringLiteral("event %1 differs from the complete recording").arg(index)));

  // The prefix replays.
  SessionReplayPlayer player;
  QList<SessionEvent> delivered;
  connect(&player, &SessionReplayPlayer::eventDelivered, this,
          [&delivered](const SessionEvent &event) { delivered.append(event); });
  QVERIFY(player.start(recovered.session).ok);
  QVERIFY(player.setTiming(SessionReplayTiming::Immediate).ok);
  QVERIFY(player.play().ok);
  QTRY_COMPARE_WITH_TIMEOUT(player.state(), SessionReplayState::Finished, 5000);
  QCOMPARE(quint64(delivered.size()), recovered.eventCount);
  for (qsizetype index = 0; index < delivered.size(); ++index)
    QVERIFY2(sameEvent(delivered.at(index), whole.session->events.at(index)),
             qPrintable(QStringLiteral("delivered event %1 differs from the complete prefix").arg(index)));

  // The window reports the recovery to the user.
  KomportApp window;
  QVERIFY(window.loadSessionForReplayFile(cutPath));
  QVERIFY(window.getDocument()->hasOfflineSession());
  QCOMPARE(window.getDocument()->offlineSession()->events.size(), qsizetype(recovered.eventCount));
  QLabel *status = window.findChild<QLabel *>(QStringLiteral("hoverHintLabel"));
  QVERIFY(status != nullptr);
  QVERIFY2(status->text().contains(QStringLiteral("truncated final record was dropped")),
           qPrintable(status->text()));
  window.slotCloseOfflineSession();
}

QTEST_MAIN(TstSessionReplayEndToEnd)
#include "tst_sessionreplayendtoend.moc"
