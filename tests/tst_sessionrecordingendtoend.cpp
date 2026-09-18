/***************************************************************************
                tst_sessionrecordingendtoend.cpp  -  Komport M9 end to end
                             ---------------------
    begin                : 2026-09-18
    copyright            : (C) 2026 by Mike Sharkey
    email                : mike@mrjdesigns.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// M9 step 6 (SPEC-M9 section 11, section 8): the end-to-end proof over a real
// pty. A recording running beside a live session must capture exactly the bytes
// readAll() returned - one record per accepted unit of bytes, never one per
// character - and the result must be a loadable v1 container whose payloads are
// byte-exact in both directions.
//
// The test reads the file back with the test-side reader, which shares no code
// with the production codec, so a writer defect cannot be hidden by a reader that
// repeats it.

#include "komportdoc.h"
#include "komportserial.h"
#include "sessioncontroller.h"
#include "sessionrecorder.h"

#include "sessionrecordreader.h"

#include <QScopeGuard>
#include <QTest>

#include <QByteArray>
#include <QFile>
#include <QList>
#include <QTemporaryDir>

#include <fcntl.h>
#include <pty.h>
#include <unistd.h>

namespace {

/** Opens a pty and returns the slave name, or an empty string when unavailable. */
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

/** Every byte value once, so a translation or trimming defect cannot pass. */
QByteArray everyByteValue()
{
  QByteArray pattern;
  pattern.reserve(256);
  for (int value = 0; value < 256; ++value)
    pattern.append(char(value));
  return pattern;
}

} // namespace

class TstSessionRecordingEndToEnd : public QObject
{
  Q_OBJECT

private slots:
  void aRecordedPtySessionIsByteExactAndChunked();
};

void TstSessionRecordingEndToEnd::aRecordedPtySessionIsByteExactAndChunked()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if (slaveName.isEmpty())
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });
  ::fcntl(masterFd, F_SETFL, ::fcntl(masterFd, F_GETFL, 0) | O_NONBLOCK);

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("end-to-end.kpsession"));

  KomportDoc document(nullptr);
  SessionController *controller = document.getSessionController();
  SessionRecorder *recorder = document.getSessionRecorder();

  QList<SessionEvent> events;
  QObject::connect(controller, &SessionController::eventObserved,
                   [&events](const SessionEvent &_event) { events.append(_event); });

  document.getSerial()->setDeviceName(slaveName);
  document.getSerial()->setBaudRate(qint32(9600));
  QVERIFY(document.getSerial()->open());
  QCOMPARE(controller->state() == SessionController::State::Live, true);

  SessionRecordingRequest request;
  request.path = path;
  request.applicationName = QStringLiteral("Komport");
  request.applicationVersion = QStringLiteral("test");
  request.configuration = document.getSerial()->appliedConfigurationSnapshot();
  const SessionRecordingStart started = recorder->start(request);
  QVERIFY2(started.ok, qPrintable(started.reason));

  // 1. RX, byte-exact: every byte value once, written in one burst.
  const QByteArray rxPattern = everyByteValue();
  QCOMPARE(::write(masterFd, rxPattern.constData(), rxPattern.size()), qint64(rxPattern.size()));

  // 2. RX, chunked rather than per character: a long burst of single bytes must not
  //    turn into one record per byte, which is what the legacy character signals
  //    would produce if the recorder consumed them instead of the event stream.
  const int burstBytes = 2000;
  QByteArray burst;
  burst.reserve(burstBytes);
  for (int index = 0; index < burstBytes; ++index)
    burst.append(char('a' + (index % 26)));
  QCOMPARE(::write(masterFd, burst.constData(), burst.size()), qint64(burst.size()));

  // Let the transport read and the events flow. The number of observations is not
  // asserted - the pty may deliver both bursts as one chunk or as several - only
  // that every byte written to the master arrived as an event.
  const auto rxBytes = [&events]() {
    qint64 total = 0;
    for (const SessionEvent &event : events)
      if (event.type == SessionEventType::Data && event.direction == SessionDirection::Rx)
        total += event.payload.size();
    return total;
  };
  QTRY_COMPARE_WITH_TIMEOUT(rxBytes(), qint64(rxPattern.size() + burst.size()), 5000);

  // 3. TX: the accepted bytes must be recorded exactly once, as one unit.
  const QByteArray txPattern = QByteArrayLiteral("line one\r\nline two\r\n");
  const qint64 accepted = document.getSerial()->writeBytes(txPattern);
  QCOMPARE(accepted, qint64(txPattern.size()));
  QTRY_VERIFY_WITH_TIMEOUT(events.last().type == SessionEventType::Data &&
                               events.last().direction == SessionDirection::Tx,
                           5000);

  const SessionRecordingReport report = recorder->stop();
  QVERIFY2(!report.damaged, qPrintable(report.reason));

  QFile file(path);
  QVERIFY(file.open(QIODevice::ReadOnly));
  const SessionRecordFile recorded = readSessionRecordFile(file.readAll());
  QVERIFY2(recorded.loadable, qPrintable(recorded.error));
  QCOMPARE(report.records, quint64(recorded.records.size()));

  QByteArray rxPayloads;
  QByteArray txPayloads;
  quint64 lastSequence = 0;
  for (const SessionRecordFileRecord &record : recorded.records) {
    QVERIFY2(record.sequence > lastSequence, "record sequences must increase");
    lastSequence = record.sequence;
    if (record.eventType == quint16(SessionEventType::Data) &&
        record.direction == quint8(SessionDirection::Rx)) {
      QVERIFY(!record.payload.isEmpty());
      rxPayloads.append(record.payload);
    } else if (record.eventType == quint16(SessionEventType::Data) &&
               record.direction == quint8(SessionDirection::Tx)) {
      txPayloads.append(record.payload);
    }
  }

  // RX is byte-exact and in order: exactly what the pty delivered, no byte lost,
  // added or translated - including NUL and every high byte.
  QCOMPARE(rxPayloads, rxPattern + burst);
  // TX is exactly what write() accepted.
  QCOMPARE(txPayloads, txPattern);

  // The chunking property: thousands of bytes arrived as a handful of records,
  // because the recorder consumes the event stream and not the character signals.
  int rxRecords = 0;
  for (const SessionRecordFileRecord &record : recorded.records)
    if (record.eventType == quint16(SessionEventType::Data) &&
        record.direction == quint8(SessionDirection::Rx))
      ++rxRecords;
  QVERIFY2(rxRecords * 16 < rxPayloads.size(), qPrintable(QStringLiteral("records %1 for %2 bytes")
                                                              .arg(rxRecords)
                                                              .arg(rxPayloads.size())));

  // The header describes the session it recorded: one source, the local serial
  // profile, and the anchor the controller reported.
  const QJsonArray sources = recorded.header.value(QStringLiteral("sources")).toArray();
  QCOMPARE(sources.size(), 1);
  QCOMPARE(sources.at(0).toObject().value(QStringLiteral("sourceId")).toInt(), 1);
  QCOMPARE(recorded.header.value(QStringLiteral("application")).toObject()
               .value(QStringLiteral("version")).toString(),
           QStringLiteral("test"));
}

QTEST_MAIN(TstSessionRecordingEndToEnd)
#include "tst_sessionrecordingendtoend.moc"
