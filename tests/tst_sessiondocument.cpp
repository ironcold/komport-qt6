/***************************************************************************
                        tst_sessiondocument.cpp  -  Komport Document Session
                             -------------------
    begin                : 2026
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de

    M8 integration tests for the document-owned session (SPEC-M8 6.2/6.3 and the
    acceptance criterion "a document-owned controller emits ordered, byte-exact
    live events"): the controller of a KomportDoc observes the document's own
    serial transport, and destroying the document ends the session without an
    event and without touching the by-value transport afterwards.

    Unlike tst_sessioncontroller this test uses the real transport and a pty
    pair, so it links libutil like tst_serial does.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "komportdoc.h"
#include "sessioncontroller.h"
#include "sessionrecorder.h"

#include "sessionrecordreader.h"

#include <QScopeGuard>
#include <QTest>

#include <QByteArray>
#include <QFile>
#include <QJsonObject>
#include <QList>
#include <QTemporaryDir>

#include <fcntl.h>
#include <pty.h>
#include <unistd.h>

namespace {

/** Create a pty pair and return the slave path, or an empty string when the
  * sandbox has no openpty(). The caller keeps @p masterFd open for the duration
  * of the test (closing it would end the slave side). */
QString makePty(int *masterFd)
{
  int slaveFd = -1;
  char slaveName[256];
  if (::openpty(masterFd, &slaveFd, slaveName, nullptr, nullptr) != 0)
    return QString();
  ::close(slaveFd);   // QSerialPort opens its own fd on the slave path
  return QString::fromLocal8Bit(slaveName);
}

/** A recording request for @p path whose configuration snapshot comes from its
  * production source (ADR-010 D7): the transport's read-only accessor, not a
  * shape assembled here and not a filtered transaction result. */
SessionRecordingRequest recordingRequest(const QString &_path, KomportSerial *_serial)
{
  SessionRecordingRequest request;
  request.path = _path;
  request.applicationName = QStringLiteral("Komport");
  request.applicationVersion = QStringLiteral("test");
  request.configuration = _serial->appliedConfigurationSnapshot();
  return request;
}

} // namespace

class TstSessionDocument : public QObject
{
  Q_OBJECT
private slots:
  void theDocumentOwnerControllerObservesTheDocumentTransport();
  void destroyingTheDocumentEndsTheSessionWithoutEvents();
  void aFailedLiveEndpointChangeIsNotRetried();
  void theDocumentOwnsARecorderBesideTheController();
  void closingTheTransportBeforeStoppingRecordsTheTerminalClosedEvent();
};

void TstSessionDocument::theDocumentOwnerControllerObservesTheDocumentTransport()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if (slaveName.isEmpty())
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });
  ::fcntl(masterFd, F_SETFL, ::fcntl(masterFd, F_GETFL, 0) | O_NONBLOCK);

  KomportDoc document(nullptr);
  SessionController *controller = document.getSessionController();
  QVERIFY(controller != nullptr);
  QCOMPARE(controller->sourceId(), quint32(1));
  QCOMPARE(controller->state() == SessionController::State::Idle, true);

  QList<SessionEvent> events;
  QObject::connect(controller, &SessionController::eventObserved,
                   [&events](const SessionEvent &_event) { events.append(_event); });

  // The controller observes the document's own serial transport, so opening it
  // is the activation boundary and its open-and-configure result is the second
  // event of the session.
  document.getSerial()->setDeviceName(slaveName);
  document.getSerial()->setBaudRate(qint32(9600));
  QVERIFY(document.getSerial()->open());
  QCOMPARE(events.size(), 2);
  QCOMPARE(events.at(0).type == SessionEventType::TransportOpened, true);
  QCOMPARE(events.at(0).sourceId, quint32(1));
  QCOMPARE(events.at(1).type == SessionEventType::TransportConfigChanged, true);
  QCOMPARE(controller->state() == SessionController::State::Live, true);
  QCOMPARE(controller->currentActivationId(), quint64(1));

  // Bytes arriving from the peer reach the session byte-exact, every value
  // 0x00..0xFF included (SPEC-M8 8).
  QByteArray payload;
  payload.reserve(256);
  for ( int value = 0; value <= 0xFF; ++value )
    payload.append(char(value));
  QCOMPARE(::write(masterFd, payload.constData(), payload.size()), qint64(payload.size()));

  QTRY_VERIFY_WITH_TIMEOUT(events.size() >= 3, 5000);
  QByteArray observed;
  for ( int i = 2; i < events.size(); ++i ) {
    QCOMPARE(events.at(i).direction == SessionDirection::Rx, true);
    QVERIFY(!events.at(i).payload.isEmpty());   // no event is empty (SPEC-M8 13)
    observed += events.at(i).payload;
  }
  QCOMPARE(observed, payload);

  // TX through the same session: byte-exact, non-empty TX events, in order.
  const int eventsBeforeTx = events.size();
  const QByteArray outgoing("TX\0data", 7);
  QCOMPARE(document.getSerial()->writeBytes(outgoing), qint64(outgoing.size()));
  QTRY_VERIFY_WITH_TIMEOUT(events.size() > eventsBeforeTx, 5000);
  QByteArray observedTx;
  for ( int i = eventsBeforeTx; i < events.size(); ++i ) {
    QCOMPARE(events.at(i).direction == SessionDirection::Tx, true);
    QVERIFY(!events.at(i).payload.isEmpty());
    observedTx += events.at(i).payload;
  }
  QCOMPARE(observedTx, outgoing);

  document.getSerial()->close();
  QTRY_VERIFY_WITH_TIMEOUT(events.last().type == SessionEventType::TransportClosed, true);
  QCOMPARE(controller->state() == SessionController::State::Idle, true);
  QCOMPARE(controller->currentActivationId(), quint64(0));
}

void TstSessionDocument::destroyingTheDocumentEndsTheSessionWithoutEvents()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if (slaveName.isEmpty())
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });

  int events = 0;
  std::unique_ptr<KomportDoc> document = std::make_unique<KomportDoc>(nullptr);
  document->getSerial()->setDeviceName(slaveName);
  QObject::connect(document->getSessionController(), &SessionController::eventObserved,
                   [&events](const SessionEvent &) { ++events; });
  QVERIFY(document->getSerial()->open());
  QVERIFY(document->getSerial()->isOpen());
  QVERIFY(events > 0);   // the session is live and already produced its events

  // Destroying the document destroys the controller first (SPEC-M8 6.2) and the
  // by-value transport afterwards; neither the session nor the transport emits an
  // event for that, and the already-running timer of the transport cannot reach a
  // dead controller.
  const int eventsBeforeDestruction = events;
  document.reset();
  QCOMPARE(events, eventsBeforeDestruction);
}

void TstSessionDocument::aFailedLiveEndpointChangeIsNotRetried()
{
  // Regression for the preferences path (KomportApp::slotShowPreferences): its
  // trailing "open if the port is closed" must not become a second attempt when a
  // *live* endpoint change already consumed one and failed. The GUI method itself
  // cannot be driven from a unit test (it runs a modal dialog), so this test
  // replays its exact sequence and guard against the real document, transport and
  // controller - the property under test is the one the guard depends on.
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if (slaveName.isEmpty())
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });

  std::unique_ptr<KomportDoc> document = std::make_unique<KomportDoc>(nullptr);
  KomportSerial *serial = document->getSerial();
  serial->setDeviceName(slaveName);
  QVERIFY(serial->open());
  QVERIFY(serial->isOpen());

  int failedOpenEvents = 0;
  QObject::connect(document->getSessionController(), &SessionController::eventObserved,
                   [&failedOpenEvents](const SessionEvent &_event) {
                     if (_event.type == SessionEventType::Error
                         && _event.metadata.value(QStringLiteral("kind")).toString()
                                == QStringLiteral("open")) {
                       ++failedOpenEvents;
                     }
                   });

  // The one request of the dialog, with an endpoint that cannot be opened: the
  // transaction closes the live activation, consumes one attempt and reports it.
  TransportConfiguration request = serial->requestedConfiguration();
  request.endpoint = QStringLiteral("/dev/komport-m8-endpoint-does-not-exist");
  const ConfigurationResult result = serial->applyConfiguration(request);

  QCOMPARE(serial->isOpen(), false);
  QVERIFY(!result.storedOnly);   // the attempt happened - this was not a store-only call
  QCOMPARE(failedOpenEvents, 1);

  // The application's guard, verbatim: open only when the request was stored on a
  // closed port. With this result it must not start another attempt.
  if (!serial->isOpen() && result.storedOnly)
    serial->open();
  QCOMPARE(failedOpenEvents, 1);   // exactly one attempt and one error per action
  QCOMPARE(serial->isOpen(), false);
  QCOMPARE(document->getSessionController()->state() == SessionController::State::Idle, true);
}

/** The document owns a recorder beside the controller (SPEC-M9 5.8), created
  *  after it and destroyed before it. */
void TstSessionDocument::theDocumentOwnsARecorderBesideTheController()
{
  KomportDoc document(nullptr);
  SessionRecorder *recorder = document.getSessionRecorder();

  QVERIFY(recorder != nullptr);
  QCOMPARE(static_cast<int>(recorder->state()), static_cast<int>(SessionRecorder::State::Stopped));
  QVERIFY(recorder->lastReport().path.isEmpty());

  // The recorder observes the document's controller, so an idle session has no
  // clock-domain anchor: the start is refused and the target is not even touched.
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  SessionRecordingRequest request;
  request.path = directory.filePath(QStringLiteral("never-written.kpsession"));
  const SessionRecordingStart refused = recorder->start(request);
  QCOMPARE(refused.ok, false);
  QVERIFY(refused.reason.contains(QStringLiteral("live")));
  QCOMPARE(QFile::exists(request.path), false);
}

/** The close ordering of SPEC-M9 5.8 is normative: the transport is closed first,
  *  while controller and recorder are still alive, so the terminal
  *  `TransportClosed` event still lands in the file; only then is the recording
  *  finalised. This is the order KomportApp::closeEvent() performs. */
void TstSessionDocument::closingTheTransportBeforeStoppingRecordsTheTerminalClosedEvent()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if (slaveName.isEmpty())
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });
  ::fcntl(masterFd, F_SETFL, ::fcntl(masterFd, F_GETFL, 0) | O_NONBLOCK);

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("close-order.kpsession"));

  KomportDoc document(nullptr);
  SessionController *controller = document.getSessionController();
  SessionRecorder *recorder = document.getSessionRecorder();
  QVERIFY(controller != nullptr);
  QVERIFY(recorder != nullptr);

  QList<SessionEvent> events;
  QObject::connect(controller, &SessionController::eventObserved,
                   [&events](const SessionEvent &_event) { events.append(_event); });

  document.getSerial()->setDeviceName(slaveName);
  document.getSerial()->setBaudRate(qint32(9600));
  QVERIFY(document.getSerial()->open());
  QCOMPARE(controller->state() == SessionController::State::Live, true);

  // Recording starts mid-session, the way the application starts it: the live state
  // is what makes the domain's anchor available for the header.
  const int eventsBeforeStart = events.size();
  const SessionRecordingStart started = recorder->start(recordingRequest(path, document.getSerial()));
  QVERIFY2(started.ok, qPrintable(started.reason));
  // Reading the applied configuration for the header is not a transaction: it
  // changes nothing and produces no observation.
  QCOMPARE(events.size(), eventsBeforeStart);
  QCOMPARE(controller->state() == SessionController::State::Live, true);

  // Some traffic, so the file holds data before the close.
  const QByteArray payload("document close ordering");
  QCOMPARE(::write(masterFd, payload.constData(), payload.size()), qint64(payload.size()));
  QTRY_VERIFY_WITH_TIMEOUT(events.size() > eventsBeforeStart, 5000);

  // The production close path of SPEC-M9 5.8 - the same call KomportApp::closeEvent()
  // makes. It closes the transport first, while the controller and the recorder are
  // still alive, and only then finalises the recording. Reversing the two steps
  // inside it would drop the terminal event from the file and fail the assertions
  // below.
  const SessionRecordingReport report = document.closeSession();
  QVERIFY2(!report.damaged, qPrintable(report.reason));
  QCOMPARE(report.path, path);
  QCOMPARE(events.last().type == SessionEventType::TransportClosed, true);
  QCOMPARE(controller->state() == SessionController::State::Idle, true);

  QFile file(path);
  QVERIFY(file.open(QIODevice::ReadOnly));
  const SessionRecordFile recorded = readSessionRecordFile(file.readAll());
  QVERIFY2(recorded.loadable, qPrintable(recorded.error));

  bool sawRx = false;
  bool sawOpened = false;
  int closedRecords = 0;
  for (const SessionRecordFileRecord &record : recorded.records) {
    if (record.eventType == quint16(SessionEventType::TransportClosed))
      ++closedRecords;
    if (record.eventType == quint16(SessionEventType::TransportOpened))
      sawOpened = true;
    if (record.eventType == quint16(SessionEventType::Data)
        && record.direction == quint8(SessionDirection::Rx))
      sawRx = true;
  }
  QVERIFY2(sawRx, "the traffic before the close was not recorded");
  QVERIFY2(closedRecords == 1, "the terminal TransportClosed event is missing from the file");
  // The recording began after the session was opened, so its file starts with the
  // traffic and ends with the terminal event - which is the whole point of the order.
  QVERIFY(!sawOpened);
  QCOMPARE(recorded.records.last().eventType, quint16(SessionEventType::TransportClosed));
  QCOMPARE(report.records, quint64(recorded.records.size()));
}

QTEST_MAIN(TstSessionDocument)

#include "tst_sessiondocument.moc"
