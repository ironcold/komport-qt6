/***************************************************************************
                        tst_sessioncontroller.cpp  -  Komport Session Controller
                             -------------------
    begin                : 2026
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de

    M8 controller tests against SPEC-M8 (section 13): one observation -> one
    event, sequence continuity across activations, the activation filter, the
    ADR-005 mapping with its single anomaly diagnostic, source ID 1, byte-exact
    payloads and the FIFO's ordered, non-reentrant delivery.

    The transport is a deterministic test double that emits scripted
    observations (SPEC-M8 section 13), never a real serial port.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "sessioncontroller.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QTest>

namespace {

/** A deterministic ITransport double: every observation is scripted by the test,
  * so nothing depends on a device, timing or chunk boundaries. */
class ScriptedTransport : public ITransport
{
public:
  using ITransport::ITransport;

  bool open() override
  {
    mOpen = true;
    return true;
  }
  void close() override { mOpen = false; }
  bool isOpen() const override { return mOpen; }
  qint64 writeBytes(const QByteArray &_bytes) override
  {
    if (!mOpen)
      return -1;
    // A write the transport accepted is reported as one TX observation, exactly
    // like the real transport's single write primitive.
    emit bytesWritten(mCurrentActivationId, _bytes, mNextWriteTimestampNs);
    return _bytes.size();
  }

  void scriptOpened(quint64 _activationId, qint64 _sourceTimestampNs,
                    const QJsonObject &_metadata = QJsonObject())
  {
    mOpen = true;
    mCurrentActivationId = _activationId;
    emit opened(_activationId, _sourceTimestampNs, _metadata);
  }
  void scriptClosed(quint64 _activationId, qint64 _sourceTimestampNs,
                    const QJsonObject &_metadata = QJsonObject())
  {
    mOpen = false;
    mCurrentActivationId = 0;
    emit closed(_activationId, _sourceTimestampNs, _metadata);
  }
  void scriptRx(quint64 _activationId, const QByteArray &_bytes, qint64 _sourceTimestampNs)
  {
    emit bytesReceived(_activationId, _bytes, _sourceTimestampNs);
  }
  void scriptTx(quint64 _activationId, const QByteArray &_bytes, qint64 _sourceTimestampNs)
  {
    emit bytesWritten(_activationId, _bytes, _sourceTimestampNs);
  }
  void scriptConfigurationChanged(quint64 _activationId, qint64 _sourceTimestampNs,
                                  const QJsonObject &_metadata)
  {
    emit configurationChanged(_activationId, _sourceTimestampNs, _metadata);
  }
  void scriptLineStateChanged(quint64 _activationId, qint64 _sourceTimestampNs,
                              const QJsonObject &_metadata)
  {
    emit lineStateChanged(_activationId, _sourceTimestampNs, _metadata);
  }
  void scriptError(quint64 _activationId, qint64 _sourceTimestampNs, const QJsonObject &_metadata)
  {
    emit transportError(_activationId, _sourceTimestampNs, _metadata);
  }

  /** the source time the double stamps on bytes it writes itself */
  void setNextWriteTimestampNs(qint64 _ns) { mNextWriteTimestampNs = _ns; }

private:
  bool mOpen = false;
  quint64 mCurrentActivationId = 0;
  qint64 mNextWriteTimestampNs = 0;
};

/** The contract enums have no QTest toString overload, so the assertions compare
  * integer codes - which is what they read like anyway. */
int typeCode(const SessionEvent &_event) { return static_cast<int>(_event.type); }
int directionCode(const SessionEvent &_event) { return static_cast<int>(_event.direction); }

/** Test-only collector (SPEC-M8 section 13): records what a subscriber received
  * and can react to an RX event by writing - the emulation consumer's behaviour.
  * It is a QObject so Qt drops the connection when the collector dies. */
class EventCollector : public QObject
{
public:
  EventCollector(SessionController *_controller, ScriptedTransport *_transport,
                 char _replyByte = 0)
  : mTransport(_transport)
  , mReplyByte(_replyByte)
  {
    QObject::connect(_controller, &SessionController::eventObserved, this,
                     [this](const SessionEvent &_event) { collect(_event); });
  }

  const QList<SessionEvent> &events() const { return mEvents; }
  /** true if an event was delivered while another delivery was still running */
  bool sawNestedDelivery() const { return mSawNestedDelivery; }
  int deliveryDepthMax() const { return mDeliveryDepthMax; }

  QList<int> typeCodes() const
  {
    QList<int> result;
    for (const SessionEvent &event : mEvents)
      result.append(typeCode(event));
    return result;
  }

private:
  void collect(const SessionEvent &_event)
  {
    if (mDeliveryDepth > 0)
      mSawNestedDelivery = true;
    ++mDeliveryDepth;
    if (mDeliveryDepth > mDeliveryDepthMax)
      mDeliveryDepthMax = mDeliveryDepth;

    mEvents.append(_event);

    // A consumer that writes from its handler: the reply must be delivered after
    // this delivery returns, never inside it (SPEC-M8 section 10).
    if (mReplyByte != 0 && _event.type == SessionEventType::Data
        && _event.direction == SessionDirection::Rx && mTransport != nullptr) {
      mTransport->writeBytes(QByteArray(1, mReplyByte));
    }

    --mDeliveryDepth;
  }

  ScriptedTransport *mTransport;
  char mReplyByte;
  QList<SessionEvent> mEvents;
  int mDeliveryDepth = 0;
  int mDeliveryDepthMax = 0;
  bool mSawNestedDelivery = false;
};

/** Metadata of the one error a failed open produces. */
QJsonObject openErrorMetadata()
{
  return QJsonObject{
    { QStringLiteral("kind"), QStringLiteral("open") },
    { QStringLiteral("code"), QStringLiteral("open_failed") },
    { QStringLiteral("message"), QStringLiteral("No such file or directory") },
  };
}

/** Records the diagnostics SPEC-M8 7 asks for (see the accepted clarification):
  * exactly one Qt *warning* per rejected observation, naming the activation id
  * and the reason. The controller emits no event for a drop and adds no signal,
  * so the test observes the message channel and forwards every message to the
  * previously installed handler to keep test failures visible. */
QList<QPair<QtMsgType, QString>> *g_diagnostics = nullptr;
QtMessageHandler g_forwardedHandler = nullptr;

void countingMessageHandler(QtMsgType _type, const QMessageLogContext &_context,
                            const QString &_message)
{
  if (g_diagnostics != nullptr)
    g_diagnostics->append(qMakePair(_type, _message));
  if (g_forwardedHandler != nullptr)
    g_forwardedHandler(_type, _context, _message);
}

class DiagnosticLog
{
public:
  DiagnosticLog()
  {
    g_diagnostics = &mMessages;
    g_forwardedHandler = qInstallMessageHandler(&countingMessageHandler);
  }
  ~DiagnosticLog()
  {
    qInstallMessageHandler(g_forwardedHandler);
    g_diagnostics = nullptr;
    g_forwardedHandler = nullptr;
  }

  /** the Qt warnings recorded so far, in order */
  QStringList warnings() const
  {
    QStringList recorded;
    for (const QPair<QtMsgType, QString> &message : mMessages) {
      if (message.first == QtWarningMsg)
        recorded.append(message.second);
    }
    return recorded;
  }
  int warningCount() const { return warnings().size(); }
  /** true when a warning at or after @p _fromIndex names exactly this activation
    * id and this reason - the contract of C4, checked per rejection instead of by
    * fragments. @p _fromIndex makes the check specific to the warning a single
    * observation added, so an earlier warning for the same id cannot satisfy it. */
  bool hasWarning(quint64 _activationId, const QString &_reason, int _fromIndex = 0) const
  {
    const QStringList recorded = warnings();
    const QString idPrefix = QStringLiteral("activation %1 -").arg(_activationId);
    for (int index = _fromIndex; index < recorded.size(); ++index) {
      if (recorded.at(index).contains(idPrefix) && recorded.at(index).contains(_reason))
        return true;
    }
    return false;
  }
  /** true when at least one warning contains @p _needle */
  bool hasWarningContaining(const QString &_needle) const
  {
    for (const QString &warning : warnings()) {
      if (warning.contains(_needle))
        return true;
    }
    return false;
  }

private:
  QList<QPair<QtMsgType, QString>> mMessages;
};

} // namespace

class TstSessionController : public QObject
{
  Q_OBJECT
private slots:
  void oneObservationProducesExactlyOneEvent();
  void sequenceContinuesAcrossActivationsWithoutReset();
  void emptyReadsAndTheNoOpProduceNothing();
  void configurationTransactionBecomesOneEventPerObservationInOrder();
  void activationFilterDropsForeignObservationsWithOneDiagnosticEach();
  void failedOpenIsReportedOnceAndKeepsTheSessionIdle();
  void adr005AnchorCarriesSessionTimeZero();
  void adr005NonDecreasingTimeAndOneAnomalyDiagnosticPerViolation();
  void everyEventCarriesSourceIdOneAndKeepsRawSourceTime();
  void payloadsStayByteExactIncludingNulAndAllByteValues();
  void fifoDeliveryIsOrderedAndNeverReentrant();
  void destroyingTheControllerEmitsNothingAndStopsObservations();
  void everyEmittedEventSatisfiesTheStructuralContract();
  void failedOpenIdentitiesAreRememberedAndSuppressed();
  void failedOpenErrorIsTheDomainAnchor();
  void lateClosedAndRepeatedOpenedAreFiltered();
  void staleAndZeroActivationIdsAreRejectedByTheWatermark();
  void operationTableRowsCrossTheControllerUnchanged();
  void clockDomainReferenceStartsInvalidAndCarriesTheAnchor();
  void clockDomainReferenceUsesAFailedOpenAsTheAnchor();
};

void TstSessionController::oneObservationProducesExactlyOneEvent()
{
  ScriptedTransport transport;
  SessionController controller(&transport);
  EventCollector collector(&controller, &transport);

  QCOMPARE(controller.state() == SessionController::State::Idle, true);
  QCOMPARE(controller.sourceId(), quint32(1));
  QCOMPARE(collector.events().size(), 0);

  transport.scriptOpened(1, 1000);
  QCOMPARE(controller.state() == SessionController::State::Live, true);
  QCOMPARE(controller.currentActivationId(), quint64(1));
  QCOMPARE(collector.events().size(), 1);
  QCOMPARE(typeCode(collector.events().at(0)), static_cast<int>(SessionEventType::TransportOpened));
  QCOMPARE(directionCode(collector.events().at(0)), static_cast<int>(SessionDirection::None));

  transport.scriptRx(1, QByteArrayLiteral("AB"), 1500);
  QCOMPARE(collector.events().size(), 2);
  QCOMPARE(typeCode(collector.events().at(1)), static_cast<int>(SessionEventType::Data));
  QCOMPARE(directionCode(collector.events().at(1)), static_cast<int>(SessionDirection::Rx));
  QCOMPARE(collector.events().at(1).payload, QByteArrayLiteral("AB"));

  transport.scriptTx(1, QByteArrayLiteral("Z"), 1600);
  QCOMPARE(collector.events().size(), 3);
  QCOMPARE(directionCode(collector.events().at(2)), static_cast<int>(SessionDirection::Tx));

  transport.scriptClosed(1, 1700);
  QCOMPARE(controller.state() == SessionController::State::Idle, true);
  QCOMPARE(controller.currentActivationId(), quint64(0));
  QCOMPARE(collector.events().size(), 4);
  QCOMPARE(typeCode(collector.events().at(3)), static_cast<int>(SessionEventType::TransportClosed));
}

void TstSessionController::sequenceContinuesAcrossActivationsWithoutReset()
{
  ScriptedTransport transport;
  SessionController controller(&transport);
  EventCollector collector(&controller, &transport);

  transport.scriptOpened(1, 1000);
  transport.scriptRx(1, QByteArrayLiteral("a"), 1100);
  transport.scriptClosed(1, 1200);
  transport.scriptOpened(2, 1300);         // a second activation, same session
  transport.scriptRx(2, QByteArrayLiteral("b"), 1400);

  const QList<SessionEvent> &events = collector.events();
  QCOMPARE(events.size(), 5);
  for ( int i = 0; i < events.size(); ++i )
    QCOMPARE(events.at(i).sequence, quint64(i + 1));   // strictly increasing
  QCOMPARE(controller.lastEmittedSequence(), quint64(5));
  QCOMPARE(typeCode(events.at(3)), static_cast<int>(SessionEventType::TransportOpened));
  QCOMPARE(controller.currentActivationId(), quint64(2));
}

void TstSessionController::emptyReadsAndTheNoOpProduceNothing()
{
  ScriptedTransport transport;
  SessionController controller(&transport);
  EventCollector collector(&controller, &transport);
  DiagnosticLog log;

  transport.scriptOpened(1, 1000);
  QCOMPARE(collector.events().size(), 1);

  // An empty read is not an observation (SPEC-M8 section 9), and the documented
  // configuration no-op emits no observation at all - neither one is a drop.
  transport.scriptRx(1, QByteArray(), 1100);
  QCOMPARE(collector.events().size(), 1);
  QCOMPARE(log.warningCount(), 0);
}

void TstSessionController::configurationTransactionBecomesOneEventPerObservationInOrder()
{
  ScriptedTransport transport;
  SessionController controller(&transport);
  EventCollector collector(&controller, &transport);

  transport.scriptOpened(1, 1000);

  // A partial apply: the transport emits exactly two observations, and each one
  // becomes exactly one event, in the stated order (SPEC-M8 section 6.2).
  const QJsonObject partialMetadata{
    { QStringLiteral("changedGroups"), QJsonArray{ QStringLiteral("hardware") } },
    { QStringLiteral("applyStatus"), QStringLiteral("partial") },
    { QStringLiteral("effective"), QJsonObject{ { QStringLiteral("endpoint"), QStringLiteral("/dev/pts/3") } } },
  };
  transport.scriptConfigurationChanged(1, 1100, partialMetadata);
  transport.scriptError(1, 1101, QJsonObject{
    { QStringLiteral("kind"), QStringLiteral("apply") },
    { QStringLiteral("code"), QStringLiteral("apply_partial") },
  });

  const QList<SessionEvent> &events = collector.events();
  QCOMPARE(events.size(), 3);
  QCOMPARE(typeCode(events.at(1)), static_cast<int>(SessionEventType::TransportConfigChanged));
  QCOMPARE(directionCode(events.at(1)), static_cast<int>(SessionDirection::None));
  QCOMPARE(typeCode(events.at(2)), static_cast<int>(SessionEventType::Error));
  QCOMPARE(directionCode(events.at(2)), static_cast<int>(SessionDirection::None));
  // The metadata crosses the controller unchanged, down to the read-back values.
  QCOMPARE(events.at(1).metadata, partialMetadata);
  QCOMPARE(events.at(1).metadata.value(QStringLiteral("applyStatus")).toString(),
           QStringLiteral("partial"));
  QCOMPARE(events.at(2).metadata.value(QStringLiteral("code")).toString(),
           QStringLiteral("apply_partial"));

  // A startBits-only change is a compatibility result with its own group.
  const QJsonObject compatibilityMetadata{
    { QStringLiteral("changedGroups"), QJsonArray{ QStringLiteral("compatibility") } },
    { QStringLiteral("applyStatus"), QStringLiteral("full") },
  };
  transport.scriptConfigurationChanged(1, 1200, compatibilityMetadata);
  QCOMPARE(collector.events().size(), 4);
  QCOMPARE(collector.events().at(3).metadata.value(QStringLiteral("changedGroups")).toArray().at(0).toString(),
           QStringLiteral("compatibility"));
}

void TstSessionController::activationFilterDropsForeignObservationsWithOneDiagnosticEach()
{
  ScriptedTransport transport;
  SessionController controller(&transport);
  EventCollector collector(&controller, &transport);

  DiagnosticLog log;

  transport.scriptOpened(1, 1000);
  QCOMPARE(collector.events().size(), 1);

  // An observation of an activation that is not live produces no event, exactly
  // one diagnostic, and is never trusted by arrival order (SPEC-M8 section 7).
  // The diagnostic is a Qt warning naming the id and the reason.
  transport.scriptRx(2, QByteArrayLiteral("late"), 1100);
  QCOMPARE(collector.events().size(), 1);
  QCOMPARE(log.warningCount(), 1);
  QVERIFY(log.hasWarning(2, QStringLiteral("the observation does not belong to the live activation")));

  transport.scriptTx(0, QByteArrayLiteral("no-id"), 1200);
  QCOMPARE(log.warningCount(), 2);
  QVERIFY(log.hasWarning(0, QStringLiteral("the observation carries no activation id")));
  QCOMPARE(collector.events().size(), 1);

  // The live activation still works afterwards.
  transport.scriptRx(1, QByteArrayLiteral("ok"), 1300);
  QCOMPARE(collector.events().size(), 2);
  QCOMPARE(log.warningCount(), 2);

  // A second activation supersedes the first; the old one is then foreign.
  transport.scriptClosed(1, 1400);
  transport.scriptOpened(2, 1500);
  transport.scriptRx(1, QByteArrayLiteral("stale"), 1600);
  QCOMPARE(log.warningCount(), 3);
  QVERIFY(log.hasWarning(1, QStringLiteral("the observation does not belong to the live activation")));
  transport.scriptRx(2, QByteArrayLiteral("fresh"), 1700);
  QCOMPARE(collector.events().size(), 5);
  QCOMPARE(collector.events().at(4).payload, QByteArrayLiteral("fresh"));
}

void TstSessionController::failedOpenIsReportedOnceAndKeepsTheSessionIdle()
{
  ScriptedTransport transport;
  SessionController controller(&transport);
  EventCollector collector(&controller, &transport);

  DiagnosticLog log;

  // The transport reports its own failed attempt exactly once; the controller
  // converts that error, does not enter Live and creates no live activation - the
  // attempt id is consumed all the same and never reused (ADR-003).
  transport.scriptError(1, 1000, openErrorMetadata());
  QCOMPARE(collector.events().size(), 1);
  QCOMPARE(typeCode(collector.events().at(0)), static_cast<int>(SessionEventType::Error));
  QCOMPARE(collector.events().at(0).metadata.value(QStringLiteral("kind")).toString(),
           QStringLiteral("open"));
  QCOMPARE(collector.events().at(0).sourceId, quint32(1));
  QCOMPARE(controller.state() == SessionController::State::Idle, true);
  QCOMPARE(controller.currentActivationId(), quint64(0));
  QCOMPARE(log.warningCount(), 0);   // the failure itself is an event, not a drop

  // Further observations carrying the failed id are suppressed: the failure is
  // reported once, not once per observation, and each drop is one warning.
  transport.scriptError(1, 1001, openErrorMetadata());
  QVERIFY(log.hasWarning(1, QStringLiteral("this error repeats an attempt that is already known")));
  transport.scriptRx(1, QByteArrayLiteral("ghost"), 1002);
  QVERIFY(log.hasWarning(1, QStringLiteral("the observation does not belong to the live activation")));
  QCOMPARE(collector.events().size(), 1);
  QCOMPARE(log.warningCount(), 2);

  // A later successful open with a new id opens the session normally.
  transport.scriptOpened(2, 1100);
  QCOMPARE(controller.state() == SessionController::State::Live, true);
  QCOMPARE(controller.currentActivationId(), quint64(2));
  QCOMPARE(collector.events().size(), 2);
  QCOMPARE(typeCode(collector.events().at(1)), static_cast<int>(SessionEventType::TransportOpened));
}

void TstSessionController::adr005AnchorCarriesSessionTimeZero()
{
  ScriptedTransport transport;
  SessionController controller(&transport);
  EventCollector collector(&controller, &transport);

  // The anchor is the first accepted event of the domain; the mapping is the
  // identity where the anchor is that event (ADR-005).
  transport.scriptOpened(1, 5000);
  transport.scriptRx(1, QByteArrayLiteral("a"), 5001);
  transport.scriptRx(1, QByteArrayLiteral("b"), 5007);

  const QList<SessionEvent> &events = collector.events();
  QCOMPARE(events.size(), 3);
  QCOMPARE(events.at(0).timestampNs, qint64(0));
  QCOMPARE(events.at(1).timestampNs, qint64(1));
  QCOMPARE(events.at(2).timestampNs, qint64(7));
  // Raw source time is preserved unchanged for every event.
  QCOMPARE(events.at(0).sourceTimestampNs, qint64(5000));
  QCOMPARE(events.at(1).sourceTimestampNs, qint64(5001));
  QCOMPARE(events.at(2).sourceTimestampNs, qint64(5007));

  // A later activation does not re-anchor the domain.
  transport.scriptClosed(1, 5100);
  transport.scriptOpened(2, 6000);
  QCOMPARE(collector.events().at(4).timestampNs, qint64(1000));
}

void TstSessionController::adr005NonDecreasingTimeAndOneAnomalyDiagnosticPerViolation()
{
  ScriptedTransport transport;
  SessionController controller(&transport);
  EventCollector collector(&controller, &transport);

  transport.scriptOpened(1, 1000);
  transport.scriptRx(1, QByteArrayLiteral("a"), 1500);    // session time 500
  transport.scriptRx(1, QByteArrayLiteral("late"), 1200); // would be 200 -> anomaly
  transport.scriptRx(1, QByteArrayLiteral("c"), 1800);    // session time 800

  const QList<SessionEvent> &events = collector.events();
  // opened, a, late, the one anomaly diagnostic, c
  QCOMPARE(events.size(), 5);
  QCOMPARE(events.at(2).payload, QByteArrayLiteral("late"));   // byte evidence kept
  QCOMPARE(events.at(2).sourceTimestampNs, qint64(1200));      // raw time not rewritten
  QCOMPARE(events.at(2).timestampNs, qint64(500));             // last emitted time
  QCOMPARE(typeCode(events.at(3)), static_cast<int>(SessionEventType::Error));
  QCOMPARE(events.at(3).metadata.value(QStringLiteral("kind")).toString(),
           QStringLiteral("runtime"));
  QCOMPARE(events.at(3).metadata.value(QStringLiteral("code")).toString(),
           QStringLiteral("non_monotonic_observation"));
  QCOMPARE(events.at(4).timestampNs, qint64(800));

  // Exactly one anomaly diagnostic for that one violation.
  int anomalies = 0;
  for ( const SessionEvent &event : events ) {
    if (event.type == SessionEventType::Error
        && event.metadata.value(QStringLiteral("code")).toString()
               == QStringLiteral("non_monotonic_observation")) {
      ++anomalies;
    }
  }
  QCOMPARE(anomalies, 1);

  // Session time never decreases within the domain.
  for ( int i = 1; i < events.size(); ++i )
    QVERIFY(events.at(i).timestampNs >= events.at(i - 1).timestampNs);
}

void TstSessionController::everyEventCarriesSourceIdOneAndKeepsRawSourceTime()
{
  ScriptedTransport transport;
  SessionController controller(&transport);
  EventCollector collector(&controller, &transport);

  transport.scriptOpened(1, 1000, QJsonObject{ { QStringLiteral("endpoint"), QStringLiteral("/dev/pts/7") } });
  transport.scriptConfigurationChanged(1, 1100, QJsonObject{ { QStringLiteral("applyStatus"), QStringLiteral("full") } });
  transport.scriptLineStateChanged(1, 1200, QJsonObject{ { QStringLiteral("line"), QStringLiteral("CTS") } });
  transport.scriptError(1, 1300, QJsonObject{ { QStringLiteral("kind"), QStringLiteral("runtime") } });
  transport.scriptClosed(1, 1400);

  const QList<SessionEvent> &events = collector.events();
  QCOMPARE(events.size(), 5);
  for ( const SessionEvent &event : events ) {
    QCOMPARE(event.sourceId, quint32(1));
    QVERIFY(event.sourceTimestampNs >= 0);
    QVERIFY(event.timestampNs >= 0);
    QCOMPARE(directionCode(event), static_cast<int>(SessionDirection::None));
  }
  QCOMPARE(events.at(0).metadata.value(QStringLiteral("endpoint")).toString(),
           QStringLiteral("/dev/pts/7"));      // opened metadata is preserved
  QCOMPARE(typeCode(events.at(2)), static_cast<int>(SessionEventType::LineStateChanged));
}

void TstSessionController::payloadsStayByteExactIncludingNulAndAllByteValues()
{
  ScriptedTransport transport;
  SessionController controller(&transport);
  EventCollector collector(&controller, &transport);

  QByteArray allBytes;
  for ( int value = 0; value <= 0xFF; ++value )
    allBytes.append(char(value));
  QByteArray withNul = QByteArrayLiteral("a");
  withNul.append('\0');
  withNul.append(QByteArrayLiteral("b"));

  transport.scriptOpened(1, 1000);
  transport.scriptRx(1, allBytes, 1100);
  transport.scriptTx(1, withNul, 1200);

  QCOMPARE(collector.events().size(), 3);
  QCOMPARE(collector.events().at(1).payload, allBytes);
  QCOMPARE(collector.events().at(2).payload, withNul);
  QCOMPARE(collector.events().at(2).payload.size(), 3);
}

void TstSessionController::fifoDeliveryIsOrderedAndNeverReentrant()
{
  ScriptedTransport transport;
  SessionController controller(&transport);
  // Two subscribers; the first reacts to an RX event by writing one byte, exactly
  // as the emulation answers device-status requests.
  EventCollector writing(&controller, &transport, 'R');
  EventCollector watching(&controller, &transport);

  transport.scriptOpened(1, 1000);
  transport.setNextWriteTimestampNs(1200);
  transport.scriptRx(1, QByteArrayLiteral("S"), 1100);

  // Both subscribers must see RX then TX, and neither may see the TX inside its
  // RX callback (SPEC-M8 section 10).
  const QList<int> expected{
    static_cast<int>(SessionEventType::TransportOpened),
    static_cast<int>(SessionEventType::Data),
    static_cast<int>(SessionEventType::Data),
  };
  QCOMPARE(writing.typeCodes(), expected);
  QCOMPARE(watching.typeCodes(), expected);
  QCOMPARE(directionCode(writing.events().at(1)), static_cast<int>(SessionDirection::Rx));
  QCOMPARE(directionCode(writing.events().at(2)), static_cast<int>(SessionDirection::Tx));
  QCOMPARE(writing.events().at(2).payload, QByteArrayLiteral("R"));
  QVERIFY(!writing.sawNestedDelivery());
  QVERIFY(!watching.sawNestedDelivery());
  QCOMPARE(writing.deliveryDepthMax(), 1);
  QCOMPARE(watching.deliveryDepthMax(), 1);

  // Both subscribers observe the same sequence numbers, and the TX event created
  // while delivering the RX event got the next sequence after it.
  QCOMPARE(writing.events().size(), watching.events().size());
  for ( int i = 0; i < writing.events().size(); ++i )
    QCOMPARE(writing.events().at(i).sequence, watching.events().at(i).sequence);
  QCOMPARE(writing.events().at(1).sequence, quint64(2));
  QCOMPARE(writing.events().at(2).sequence, quint64(3));
}

void TstSessionController::destroyingTheControllerEmitsNothingAndStopsObservations()
{
  ScriptedTransport transport;
  int received = 0;
  {
    SessionController controller(&transport);
    QObject::connect(&controller, &SessionController::eventObserved,
                     [&received](const SessionEvent &) { ++received; });
    transport.scriptOpened(1, 1000);
    QCOMPARE(received, 1);
  }   // the controller is destroyed before the transport, as the document does

  // The transport keeps reporting; nothing is delivered and nothing crashes.
  transport.scriptRx(1, QByteArrayLiteral("after"), 1100);
  transport.scriptClosed(1, 1200);
  QCOMPARE(received, 1);
}

void TstSessionController::everyEmittedEventSatisfiesTheStructuralContract()
{
  ScriptedTransport transport;
  SessionController controller(&transport);
  EventCollector collector(&controller, &transport);

  // The attempted ids of one transport strictly increase, so the failed attempt is
  // the anchor and the following successful activation has the next id.
  transport.scriptError(9, 900, openErrorMetadata());      // failed-open anchor
  transport.scriptOpened(10, 1000);
  transport.scriptRx(10, QByteArrayLiteral("x"), 1100);
  transport.scriptRx(10, QByteArrayLiteral("y"), 1000);    // anomaly
  transport.scriptConfigurationChanged(10, 1200, QJsonObject{ { QStringLiteral("applyStatus"), QStringLiteral("partial") } });
  transport.scriptError(10, 1300, QJsonObject{ { QStringLiteral("kind"), QStringLiteral("apply") } });
  transport.scriptClosed(10, 1400);

  QCOMPARE(collector.events().size(), 8);   // incl. the one anomaly diagnostic
  for ( const SessionEvent &event : collector.events() ) {
    QString reason;
    QVERIFY2(isValidSessionEvent(event, &reason), qPrintable(reason));
  }
}

void TstSessionController::failedOpenIdentitiesAreRememberedAndSuppressed()
{
  ScriptedTransport transport;
  SessionController controller(&transport);
  EventCollector collector(&controller, &transport);
  DiagnosticLog log;

  // Two failed attempts (SPEC-M8 7): each is reported once, and both ids stay
  // suppressed - including the older one, which a single remembered id forgets.
  transport.scriptError(1, 1000, openErrorMetadata());
  transport.scriptError(2, 2000, openErrorMetadata());
  QCOMPARE(collector.events().size(), 2);
  QCOMPARE(log.warningCount(), 0);

  // A duplicate of the first failure is suppressed, not reported again.
  transport.scriptError(1, 1001, openErrorMetadata());
  QCOMPARE(collector.events().size(), 2);
  QCOMPARE(log.warningCount(), 1);
  QVERIFY(log.hasWarning(1, QStringLiteral("this error repeats an attempt that is already known")));

  // Every further observation of a failed activation is suppressed as well - also
  // an `opened` for that id, because the attempt is known to have failed.
  transport.scriptRx(2, QByteArrayLiteral("ghost"), 2001);
  transport.scriptOpened(1, 1002);
  transport.scriptConfigurationChanged(1, 1003,
      QJsonObject{ { QStringLiteral("applyStatus"), QStringLiteral("full") } });
  QCOMPARE(collector.events().size(), 2);
  QCOMPARE(log.warningCount(), 4);
  QVERIFY(log.hasWarning(2, QStringLiteral("the observation does not belong to the live activation")));
  QVERIFY(log.hasWarning(1, QStringLiteral("this activation id was already used")));
  QVERIFY(log.hasWarning(1, QStringLiteral("the observation does not belong to the live activation")));
  QCOMPARE(controller.state() == SessionController::State::Idle, true);
  QCOMPARE(controller.currentActivationId(), quint64(0));

  // A genuinely new activation opens the session; the failed ids stay closed.
  transport.scriptOpened(3, 3000);
  QCOMPARE(controller.state() == SessionController::State::Live, true);
  QCOMPARE(controller.currentActivationId(), quint64(3));
  QCOMPARE(collector.events().size(), 3);
  transport.scriptRx(3, QByteArrayLiteral("live"), 3100);
  QCOMPARE(collector.events().size(), 4);
  // ... and the failed ones are still suppressed afterwards.
  transport.scriptRx(1, QByteArrayLiteral("still ghost"), 3200);
  QCOMPARE(collector.events().size(), 4);
  QCOMPARE(log.warningCount(), 5);
  QVERIFY(log.hasWarning(1, QStringLiteral("the observation does not belong to the live activation")));
}

void TstSessionController::failedOpenErrorIsTheDomainAnchor()
{
  ScriptedTransport transport;
  SessionController controller(&transport);
  EventCollector collector(&controller, &transport);

  // ADR-005: a failed-open Error is a legitimate anchor, so the session timeline
  // starts there and the mapping is the identity after it.
  transport.scriptError(1, 100000, openErrorMetadata());
  QCOMPARE(collector.events().size(), 1);
  QCOMPARE(collector.events().at(0).timestampNs, qint64(0));
  QCOMPARE(collector.events().at(0).sourceTimestampNs, qint64(100000));
  QCOMPARE(typeCode(collector.events().at(0)), static_cast<int>(SessionEventType::Error));

  // The later successful activation does not re-anchor: session time continues.
  transport.scriptOpened(2, 100500);
  QCOMPARE(collector.events().at(1).timestampNs, qint64(500));
  transport.scriptRx(2, QByteArrayLiteral("x"), 100750);
  QCOMPARE(collector.events().at(2).timestampNs, qint64(750));
}

void TstSessionController::lateClosedAndRepeatedOpenedAreFiltered()
{
  ScriptedTransport transport;
  SessionController controller(&transport);
  EventCollector collector(&controller, &transport);
  DiagnosticLog log;

  transport.scriptOpened(1, 1000);
  transport.scriptClosed(1, 1100);
  transport.scriptOpened(2, 1200);
  QCOMPARE(collector.events().size(), 3);

  // A late `closed` of the old activation must not end the live one: the id
  // decides, not the arrival order (SPEC-M8 7).
  transport.scriptClosed(1, 1300);
  QCOMPARE(collector.events().size(), 3);
  QCOMPARE(log.warningCount(), 1);
  QVERIFY(log.hasWarning(1, QStringLiteral("no live activation carries this id")));
  QCOMPARE(controller.state() == SessionController::State::Live, true);
  QCOMPARE(controller.currentActivationId(), quint64(2));

  // A repeated `opened` for an id that was already used is not a second activation
  // boundary - the watermark of admitted attempts rejects it.
  transport.scriptOpened(2, 1400);
  QCOMPARE(collector.events().size(), 3);
  QCOMPARE(log.warningCount(), 2);
  QVERIFY(log.hasWarning(2, QStringLiteral("this activation id was already used")));
  transport.scriptRx(2, QByteArrayLiteral("still live"), 1500);
  QCOMPARE(collector.events().size(), 4);
}

void TstSessionController::staleAndZeroActivationIdsAreRejectedByTheWatermark()
{
  ScriptedTransport transport;
  SessionController controller(&transport);
  EventCollector collector(&controller, &transport);
  DiagnosticLog log;

  // The id 0 is never an activation id (ADR-003: ids start at 1), and each of the
  // three observations is rejected with its own diagnostic - none of them is
  // silently absorbed.
  transport.scriptOpened(0, 1000);
  QCOMPARE(collector.events().size(), 0);
  QCOMPARE(controller.state() == SessionController::State::Idle, true);
  QCOMPARE(log.warningCount(), 1);
  QVERIFY(log.hasWarning(0, QStringLiteral("0 is not an activation id")));

  transport.scriptError(0, 1001, openErrorMetadata());
  QCOMPARE(log.warningCount(), 2);
  QVERIFY(log.hasWarning(0, QStringLiteral("the observation carries no activation id")));

  const int warningsBeforeZeroRead = log.warningCount();
  transport.scriptRx(0, QByteArrayLiteral("x"), 1002);
  QCOMPARE(log.warningCount(), warningsBeforeZeroRead + 1);
  // The read has to produce its own diagnostic: @p warningsBeforeZeroRead makes the
  // check specific to the warning it just added, so the earlier error(0) warning
  // cannot satisfy it.
  QVERIFY(log.hasWarning(0, QStringLiteral("the observation carries no activation id"),
                         warningsBeforeZeroRead));
  QCOMPARE(collector.events().size(), 0);

  // An admitted failed attempt raises the watermark, so a *delayed* `opened` of a
  // lower id can never be accepted afterwards: the id decides, never the arrival
  // order (ADR-003, SPEC-M8 7).
  transport.scriptError(2, 1100, openErrorMetadata());
  QCOMPARE(collector.events().size(), 1);
  transport.scriptOpened(1, 1200);
  QCOMPARE(collector.events().size(), 1);
  QCOMPARE(controller.state() == SessionController::State::Idle, true);
  QCOMPARE(log.warningCount(), 4);
  QVERIFY(log.hasWarning(1, QStringLiteral("this activation id was already used")));

  // An attempt above the watermark opens the session normally.
  transport.scriptOpened(3, 1300);
  QCOMPARE(controller.state() == SessionController::State::Live, true);
  QCOMPARE(controller.currentActivationId(), quint64(3));
  QCOMPARE(collector.events().size(), 2);

  // A foreign, non-zero TX observation of a past activation is rejected with the
  // live-activation reason (not with the "no activation id" one).
  transport.scriptTx(2, QByteArrayLiteral("foreign"), 1350);
  QCOMPARE(collector.events().size(), 2);
  QVERIFY(log.hasWarning(2, QStringLiteral("the observation does not belong to the live activation")));
}

void TstSessionController::operationTableRowsCrossTheControllerUnchanged()
{
  ScriptedTransport transport;
  SessionController controller(&transport);
  EventCollector collector(&controller, &transport);

  transport.scriptOpened(1, 1000);

  // A full result with the complete metadata of SPEC-M8 6.2: every requested and
  // every read-back hardware field, buffering, the compatibility field and the
  // groups of the transaction. Requested and effective agree because the
  // transaction was full.
  const QJsonObject fullResult{
    { QStringLiteral("requested"), QJsonObject{ { QStringLiteral("endpoint"), QStringLiteral("/dev/pts/9") },
                                                { QStringLiteral("baudRate"), QStringLiteral("9600") },
                                                { QStringLiteral("dataBits"), QStringLiteral("8") },
                                                { QStringLiteral("stopBits"), QStringLiteral("1") },
                                                { QStringLiteral("parity"), QStringLiteral("NONE") },
                                                { QStringLiteral("flowControl"), QStringLiteral("NONE") } } },
    { QStringLiteral("effective"), QJsonObject{ { QStringLiteral("endpoint"), QStringLiteral("/dev/pts/9") },
                                                { QStringLiteral("baudRate"), QStringLiteral("9600") },
                                                { QStringLiteral("dataBits"), QStringLiteral("8") },
                                                { QStringLiteral("stopBits"), QStringLiteral("1") },
                                                { QStringLiteral("parity"), QStringLiteral("NONE") },
                                                { QStringLiteral("flowControl"), QStringLiteral("NONE") } } },
    { QStringLiteral("localBuffering"), QJsonObject{ { QStringLiteral("rxQueue"), 2048 },
                                                     { QStringLiteral("flushRate"), 100 } } },
    { QStringLiteral("compatibility"), QJsonObject{ { QStringLiteral("startBits"), QStringLiteral("1") } } },
    { QStringLiteral("changedGroups"), QJsonArray{ QStringLiteral("hardware"),
                                                   QStringLiteral("localBuffering") } },
    { QStringLiteral("applyStatus"), QStringLiteral("full") },
  };
  transport.scriptConfigurationChanged(1, 1100, fullResult);
  QCOMPARE(collector.events().size(), 2);
  QCOMPARE(collector.events().at(1).metadata, fullResult);
  const QJsonObject eventMetadata = collector.events().at(1).metadata;
  const QJsonObject requestedHardware = eventMetadata.value(QStringLiteral("requested")).toObject();
  const QJsonObject effectiveHardware = eventMetadata.value(QStringLiteral("effective")).toObject();
  QCOMPARE(requestedHardware.value(QStringLiteral("endpoint")).toString(), QStringLiteral("/dev/pts/9"));
  QCOMPARE(requestedHardware.value(QStringLiteral("baudRate")).toString(), QStringLiteral("9600"));
  QCOMPARE(requestedHardware.value(QStringLiteral("dataBits")).toString(), QStringLiteral("8"));
  QCOMPARE(requestedHardware.value(QStringLiteral("stopBits")).toString(), QStringLiteral("1"));
  QCOMPARE(requestedHardware.value(QStringLiteral("parity")).toString(), QStringLiteral("NONE"));
  QCOMPARE(requestedHardware.value(QStringLiteral("flowControl")).toString(), QStringLiteral("NONE"));
  QCOMPARE(effectiveHardware.value(QStringLiteral("endpoint")).toString(), QStringLiteral("/dev/pts/9"));
  QCOMPARE(effectiveHardware.value(QStringLiteral("baudRate")).toString(), QStringLiteral("9600"));
  QCOMPARE(effectiveHardware.value(QStringLiteral("flowControl")).toString(), QStringLiteral("NONE"));
  QVERIFY(!effectiveHardware.contains(QStringLiteral("portName")));
  QCOMPARE(eventMetadata.value(QStringLiteral("localBuffering")).toObject()
               .value(QStringLiteral("rxQueue")).toInt(), 2048);
  QCOMPARE(eventMetadata.value(QStringLiteral("localBuffering")).toObject()
               .value(QStringLiteral("flushRate")).toInt(), 100);
  QCOMPARE(eventMetadata.value(QStringLiteral("compatibility")).toObject()
               .value(QStringLiteral("startBits")).toString(), QStringLiteral("1"));
  QCOMPARE(eventMetadata.value(QStringLiteral("changedGroups")).toArray().size(), 2);

  // Partial: the two observations of the transaction arrive as two events, in the
  // order SPEC-M8 6.2 states.
  transport.scriptConfigurationChanged(1, 1200, QJsonObject{
    { QStringLiteral("applyStatus"), QStringLiteral("partial") },
    { QStringLiteral("changedGroups"), QJsonArray{ QStringLiteral("hardware") } },
  });
  transport.scriptError(1, 1201, QJsonObject{
    { QStringLiteral("kind"), QStringLiteral("apply") },
    { QStringLiteral("code"), QStringLiteral("apply_partial") },
    { QStringLiteral("message"), QStringLiteral("the device may not support this combination") },
  });
  QCOMPARE(collector.events().size(), 4);
  QCOMPARE(typeCode(collector.events().at(3)), static_cast<int>(SessionEventType::Error));

  // Failed without a change: only the error observation exists.
  transport.scriptError(1, 1300, QJsonObject{
    { QStringLiteral("kind"), QStringLiteral("apply") },
    { QStringLiteral("code"), QStringLiteral("apply_failed") },
  });
  QCOMPARE(collector.events().size(), 5);

  // A startBits-only compatibility result crosses unchanged, and the sequence has
  // no gap anywhere in this sequence of transactions.
  const QJsonObject compatibilityResult{
    { QStringLiteral("changedGroups"), QJsonArray{ QStringLiteral("compatibility") } },
    { QStringLiteral("applyStatus"), QStringLiteral("full") },
  };
  transport.scriptConfigurationChanged(1, 1400, compatibilityResult);
  QCOMPARE(collector.events().size(), 6);
  QCOMPARE(collector.events().at(5).metadata, compatibilityResult);

  // `failed` with a changed effective state: the port moved the effective baud
  // rate away from the 9600 of the preceding full result, so this transaction
  // really changed the effective state although it is not clean. The session sees
  // two observations - the result (with the changed read-back state) and then the
  // transaction's error.
  const QJsonObject failedWithChangeResult{
    { QStringLiteral("applyStatus"), QStringLiteral("failed") },
    { QStringLiteral("changedGroups"), QJsonArray{ QStringLiteral("hardware") } },
    { QStringLiteral("requested"), QJsonObject{ { QStringLiteral("baudRate"), QStringLiteral("38400") } } },
    { QStringLiteral("effective"), QJsonObject{ { QStringLiteral("endpoint"), QStringLiteral("/dev/pts/9") },
                                                { QStringLiteral("baudRate"), QStringLiteral("19200") } } },
    { QStringLiteral("message"), QStringLiteral("Could not apply the requested port settings") },
  };
  transport.scriptConfigurationChanged(1, 1500, failedWithChangeResult);
  transport.scriptError(1, 1501, QJsonObject{
    { QStringLiteral("kind"), QStringLiteral("apply") },
    { QStringLiteral("code"), QStringLiteral("apply_failed") },
  });
  QCOMPARE(collector.events().size(), 8);
  QCOMPARE(collector.events().at(6).metadata, failedWithChangeResult);
  QCOMPARE(collector.events().at(6).metadata.value(QStringLiteral("applyStatus")).toString(),
           QStringLiteral("failed"));
  QCOMPARE(collector.events().at(6).metadata.value(QStringLiteral("effective")).toObject()
               .value(QStringLiteral("baudRate")).toString(), QStringLiteral("19200"));
  QCOMPARE(collector.events().at(6).metadata.value(QStringLiteral("requested")).toObject()
               .value(QStringLiteral("baudRate")).toString(), QStringLiteral("38400"));
  QCOMPARE(collector.events().at(6).metadata.value(QStringLiteral("changedGroups")).toArray().size(), 1);
  // The modeled effective state really changed relative to the preceding event.
  QVERIFY(collector.events().at(6).metadata.value(QStringLiteral("effective")).toObject()
              .value(QStringLiteral("baudRate")).toString()
          != collector.events().at(1).metadata.value(QStringLiteral("effective")).toObject()
                 .value(QStringLiteral("baudRate")).toString());
  QCOMPARE(collector.events().at(7).metadata.value(QStringLiteral("code")).toString(),
           QStringLiteral("apply_failed"));
  QCOMPARE(collector.events().at(7).type == SessionEventType::Error, true);

  // A live endpoint change is an activation change: the transport closes the old
  // activation, opens the new one and applies the configuration once, so the
  // session sees close, open and the one result, in that order (SPEC-M8 6.2).
  transport.scriptClosed(1, 1600);
  transport.scriptOpened(2, 1700);
  transport.scriptConfigurationChanged(2, 1701, QJsonObject{
    { QStringLiteral("applyStatus"), QStringLiteral("full") },
    { QStringLiteral("changedGroups"), QJsonArray{ QStringLiteral("hardware") } },
    { QStringLiteral("effective"), QJsonObject{ { QStringLiteral("endpoint"), QStringLiteral("/dev/pts/11") } } },
  });
  QCOMPARE(collector.events().size(), 11);
  QCOMPARE(collector.events().at(8).type == SessionEventType::TransportClosed, true);
  QCOMPARE(collector.events().at(9).type == SessionEventType::TransportOpened, true);
  QCOMPARE(collector.events().at(10).type == SessionEventType::TransportConfigChanged, true);
  QCOMPARE(collector.events().at(10).metadata.value(QStringLiteral("effective")).toObject()
               .value(QStringLiteral("endpoint")).toString(), QStringLiteral("/dev/pts/11"));
  QCOMPARE(controller.currentActivationId(), quint64(2));
  QCOMPARE(collector.events().at(10).sourceId, quint32(1));

  // No transaction in this whole sequence leaves a gap in the sequence numbers.
  for ( int i = 0; i < collector.events().size(); ++i )
    QCOMPARE(collector.events().at(i).sequence, quint64(i + 1));
}

/** The anchor reference of ADR-010 (decision D8): invalid until the domain's
  *  first event was accepted, then fixed at the anchor's raw source time with
  *  session time 0 - the pair a recorder writes into a header when it starts
  *  observing a session that is already running. */
void TstSessionController::clockDomainReferenceStartsInvalidAndCarriesTheAnchor()
{
  ScriptedTransport transport;
  SessionController controller(&transport);
  EventCollector collector(&controller, &transport);

  QVERIFY(!controller.clockDomainReference().valid);

  // An observation of an unknown activation is dropped and creates no anchor.
  transport.scriptRx(7, QByteArrayLiteral("stray"), 500);
  QVERIFY(!controller.clockDomainReference().valid);
  QCOMPARE(collector.events().size(), 0);

  // The first accepted event is the anchor.
  transport.scriptOpened(1, 1000);
  const SessionClockDomainReference reference = controller.clockDomainReference();
  QVERIFY(reference.valid);
  QCOMPARE(reference.sourceTimestampNs, qint64(1000));
  QCOMPARE(reference.sessionTimestampNs, qint64(0));
  QCOMPARE(collector.events().at(0).sourceTimestampNs, qint64(1000));
  QCOMPARE(collector.events().at(0).timestampNs, qint64(0));

  // It stays fixed while the session timeline advances: a consumer that starts
  // mid-session reads the same pair as one that saw the anchor, and the events it
  // then observes already carry session times well above zero. The full
  // close/reopen lifecycle of SPEC-M8 7 must not move it either.
  transport.scriptRx(1, QByteArrayLiteral("later"), 4500);
  transport.scriptClosed(1, 4800);
  QCOMPARE(static_cast<int>(controller.state()), static_cast<int>(SessionController::State::Idle));
  const SessionClockDomainReference afterClose = controller.clockDomainReference();
  QVERIFY(afterClose.valid);
  QCOMPARE(afterClose.sourceTimestampNs, qint64(1000));
  QCOMPARE(afterClose.sessionTimestampNs, qint64(0));

  transport.scriptOpened(2, 5000);          // a second activation of the same session
  QVERIFY(controller.clockDomainReference().valid);
  QCOMPARE(controller.clockDomainReference().sourceTimestampNs, qint64(1000));
  transport.scriptTx(2, QByteArrayLiteral("z"), 5500);

  const SessionClockDomainReference later = controller.clockDomainReference();
  QVERIFY(later.valid);
  QCOMPARE(later.sourceTimestampNs, qint64(1000));
  QCOMPARE(later.sessionTimestampNs, qint64(0));
  QCOMPARE(collector.events().last().timestampNs, qint64(4500));
}

/** A failed open is a legitimate anchor (ADR-005): the reference reports it even
  *  though the session stays idle, and a later activation does not move it. */
void TstSessionController::clockDomainReferenceUsesAFailedOpenAsTheAnchor()
{
  ScriptedTransport transport;
  SessionController controller(&transport);
  EventCollector collector(&controller, &transport);

  transport.scriptError(1, 100000, openErrorMetadata());
  const SessionClockDomainReference reference = controller.clockDomainReference();
  QVERIFY(reference.valid);
  QCOMPARE(reference.sourceTimestampNs, qint64(100000));
  QCOMPARE(reference.sessionTimestampNs, qint64(0));
  QCOMPARE(static_cast<int>(controller.state()), static_cast<int>(SessionController::State::Idle));

  transport.scriptOpened(2, 100500);
  QCOMPARE(controller.clockDomainReference().sourceTimestampNs, qint64(100000));
  QCOMPARE(collector.events().at(1).timestampNs, qint64(500));
}

QTEST_MAIN(TstSessionController)

#include "tst_sessioncontroller.moc"
