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

#include <QScopeGuard>
#include <QTest>

#include <QByteArray>
#include <QList>

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

} // namespace

class TstSessionDocument : public QObject
{
  Q_OBJECT
private slots:
  void theDocumentOwnerControllerObservesTheDocumentTransport();
  void destroyingTheDocumentEndsTheSessionWithoutEvents();
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
    observed += events.at(i).payload;
  }
  QCOMPARE(observed, payload);

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

QTEST_MAIN(TstSessionDocument)

#include "tst_sessiondocument.moc"
