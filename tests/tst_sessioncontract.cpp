/***************************************************************************
                        tst_sessioncontract.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : 2026
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de

    Unit tests for the M8 public session/transport value contracts
    (sessionevent.h, transportconfiguration.h): structural event validation,
    lossless 64-bit JSON encoding, configuration group comparison and the
    configuration result metadata. Deliberately links Qt Core + Test only, so it
    also shows that these contracts need no widget dependency.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "sessionevent.h"
#include "transportconfiguration.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

namespace {

SessionEvent rxDataEvent(const QByteArray &payload = QByteArrayLiteral("x"))
{
  SessionEvent event;
  event.sequence = 1;
  event.sourceId = 1;
  event.type = SessionEventType::Data;
  event.direction = SessionDirection::Rx;
  event.payload = payload;
  return event;
}

} // namespace

class TstSessionContract : public QObject
{
  Q_OBJECT
private slots:
  void acceptsAWellFormedDataEvent();
  void rejectsMalformedDataEvents();
  void rejectsMalformedNonDataEvents();
  void rejectsReservedSourceIdAndNegativeTimes();
  void rejectsUndeclaredEventTypes();
  void roundTrips64BitJsonIntegersLosslessly();
  void rejectsInexactJsonIntegers();
  void reportsChangedConfigurationGroupsInFixedOrder();
  void treatsNothingChangedAsTheNoOpCase();
  void carriesTheFullResultMetadata();
};

void TstSessionContract::acceptsAWellFormedDataEvent()
{
  QString reason = QStringLiteral("unset");
  QVERIFY2(isValidSessionEvent(rxDataEvent(), &reason), qPrintable(reason));
  QVERIFY(reason.isEmpty());

  SessionEvent tx = rxDataEvent();
  tx.direction = SessionDirection::Tx;
  QVERIFY(isValidSessionEvent(tx));

  // A non-data event without payload is fine.
  SessionEvent opened;
  opened.sequence = 2;
  opened.sourceId = 1;
  opened.type = SessionEventType::TransportOpened;
  opened.direction = SessionDirection::None;
  QVERIFY(isValidSessionEvent(opened));
}

void TstSessionContract::rejectsMalformedDataEvents()
{
  SessionEvent emptyPayload = rxDataEvent(QByteArray());
  QString reason;
  QVERIFY(!isValidSessionEvent(emptyPayload, &reason));
  QVERIFY(!reason.isEmpty());

  SessionEvent wrongDirection = rxDataEvent();
  wrongDirection.direction = SessionDirection::None;
  QVERIFY(!isValidSessionEvent(wrongDirection));
}

void TstSessionContract::rejectsMalformedNonDataEvents()
{
  SessionEvent withPayload;
  withPayload.sequence = 1;
  withPayload.sourceId = 1;
  withPayload.type = SessionEventType::TransportClosed;
  withPayload.direction = SessionDirection::None;
  withPayload.payload = QByteArrayLiteral("not allowed");
  QVERIFY(!isValidSessionEvent(withPayload));

  SessionEvent withDirection = withPayload;
  withDirection.payload.clear();
  withDirection.direction = SessionDirection::Tx;
  QVERIFY(!isValidSessionEvent(withDirection));
}

void TstSessionContract::rejectsReservedSourceIdAndNegativeTimes()
{
  SessionEvent reservedId = rxDataEvent();
  reservedId.sourceId = 0;
  QVERIFY(!isValidSessionEvent(reservedId));

  SessionEvent negativeSource = rxDataEvent();
  negativeSource.sourceTimestampNs = -1;
  QVERIFY(!isValidSessionEvent(negativeSource));

  SessionEvent negativeSession = rxDataEvent();
  negativeSession.timestampNs = -1;
  QVERIFY(!isValidSessionEvent(negativeSession));
}

void TstSessionContract::rejectsUndeclaredEventTypes()
{
  // ADR-002: `type` must be a declared enumerator - a cast from an arbitrary
  // integer is not an event, even when direction and payload look consistent.
  SessionEvent undeclared = rxDataEvent();
  undeclared.type = static_cast<SessionEventType>(999);
  undeclared.direction = SessionDirection::None;
  undeclared.payload.clear();
  QString reason;
  QVERIFY(!isValidSessionEvent(undeclared, &reason));
  QVERIFY(reason.contains(QStringLiteral("enumerator")));

  SessionEvent zero = rxDataEvent();
  zero.type = static_cast<SessionEventType>(0);
  zero.direction = SessionDirection::None;
  zero.payload.clear();
  QVERIFY(!isValidSessionEvent(zero));

  // Every declared non-data type is still accepted.
  const SessionEventType declared[] = {
    SessionEventType::TransportOpened,
    SessionEventType::TransportClosed,
    SessionEventType::TransportConfigChanged,
    SessionEventType::LineStateChanged,
    SessionEventType::Error,
    SessionEventType::Annotation,
    SessionEventType::Bookmark,
  };
  for (const SessionEventType type : declared) {
    SessionEvent event = rxDataEvent();
    event.type = type;
    event.direction = SessionDirection::None;
    event.payload.clear();
    QString declaredReason;
    QVERIFY2(isValidSessionEvent(event, &declaredReason), qPrintable(declaredReason));
  }
}

void TstSessionContract::roundTrips64BitJsonIntegersLosslessly()
{
  const QList<qint64> values = {
    0,
    1,
    Q_INT64_C(1) << 40,                 // far beyond the 2^53 double limit
    Q_INT64_C(1) << 53,
    (Q_INT64_C(1) << 62),
    std::numeric_limits<qint64>::max(),
    std::numeric_limits<qint64>::min(),
  };
  for (const qint64 value : values) {
    const QJsonValue encoded = sessionJsonInteger(value);
    QVERIFY2(encoded.isString(), "64-bit values must be encoded as decimal strings");
    qint64 decoded = 0;
    QVERIFY2(sessionJsonIntegerToQInt64(encoded, &decoded), qPrintable(QString::number(value)));
    QCOMPARE(decoded, value);
  }
}

void TstSessionContract::rejectsInexactJsonIntegers()
{
  qint64 decoded = 0;

  // An integral JSON number inside the range where doubles represent every
  // integer is accepted, including the exact limit itself.
  QVERIFY(sessionJsonIntegerToQInt64(QJsonValue(42.0), &decoded));
  QCOMPARE(decoded, Q_INT64_C(42));
  QVERIFY(sessionJsonIntegerToQInt64(QJsonValue(9007199254740992.0), &decoded)); // 2^53
  QCOMPARE(decoded, Q_INT64_C(1) << 53);

  // Fractional, non-numeric and out-of-range values are not. Beyond 2^53 a JSON
  // number cannot be trusted to carry the intended 64-bit value (a writer may
  // have rounded it), so only the canonical decimal string counts there.
  QVERIFY(!sessionJsonIntegerToQInt64(QJsonValue(42.5), &decoded));
  QVERIFY(!sessionJsonIntegerToQInt64(QJsonValue(2.0e18), &decoded));
  QVERIFY(!sessionJsonIntegerToQInt64(QJsonValue(-2.0e18), &decoded));
  QVERIFY(!sessionJsonIntegerToQInt64(QJsonValue(QStringLiteral("12.5")), &decoded));
  QVERIFY(!sessionJsonIntegerToQInt64(QJsonValue(QStringLiteral("abc")), &decoded));
  QVERIFY(!sessionJsonIntegerToQInt64(QJsonValue(true), &decoded));
  QVERIFY(!sessionJsonIntegerToQInt64(QJsonValue(QStringLiteral("1")), nullptr));

  // The same magnitude is fine as a canonical decimal string.
  QVERIFY(sessionJsonIntegerToQInt64(sessionJsonInteger(Q_INT64_C(1) << 53), &decoded));
  QCOMPARE(decoded, Q_INT64_C(1) << 53);
}

void TstSessionContract::reportsChangedConfigurationGroupsInFixedOrder()
{
  TransportConfiguration baseline;
  baseline.endpoint = QStringLiteral("/dev/ttyS0");
  baseline.baudRate = QStringLiteral("9600");
  baseline.dataBits = QStringLiteral("8");
  baseline.stopBits = QStringLiteral("1");
  baseline.parity = QStringLiteral("NONE");
  baseline.flowControl = QStringLiteral("NONE");
  baseline.startBits = QStringLiteral("1");

  TransportConfiguration hardwareOnly = baseline;
  hardwareOnly.baudRate = QStringLiteral("19200");
  QCOMPARE(hardwareOnly.changedGroupsComparedTo(baseline),
           QStringList{ QStringLiteral("hardware") });

  TransportConfiguration bufferingOnly = baseline;
  bufferingOnly.rxQueue = 4096;
  QCOMPARE(bufferingOnly.changedGroupsComparedTo(baseline),
           QStringList{ QStringLiteral("localBuffering") });

  // startBits is stored and reported but never applied to hardware, so it has
  // its own group (SPEC-M8 section 6.2).
  TransportConfiguration compatibilityOnly = baseline;
  compatibilityOnly.startBits = QStringLiteral("2");
  QCOMPARE(compatibilityOnly.changedGroupsComparedTo(baseline),
           QStringList{ QStringLiteral("compatibility") });
  QVERIFY(compatibilityOnly.hardwareEquals(baseline));
  QVERIFY(compatibilityOnly.bufferingEquals(baseline));

  TransportConfiguration mixed = hardwareOnly;
  mixed.flushRate = 100;
  mixed.startBits = QStringLiteral("2");
  QCOMPARE(mixed.changedGroupsComparedTo(baseline),
           (QStringList{ QStringLiteral("hardware"),
                         QStringLiteral("localBuffering"),
                         QStringLiteral("compatibility") }));
}

void TstSessionContract::treatsNothingChangedAsTheNoOpCase()
{
  TransportConfiguration baseline;
  baseline.endpoint = QStringLiteral("/dev/ttyS0");
  const TransportConfiguration same = baseline;
  QVERIFY(same.changedGroupsComparedTo(baseline).isEmpty());
}

void TstSessionContract::carriesTheFullResultMetadata()
{
  TransportConfiguration requested;
  requested.endpoint = QStringLiteral("/dev/ttyS0");
  requested.baudRate = QStringLiteral("9600");
  requested.rxQueue = 2048;
  requested.flushRate = 100;
  requested.startBits = QStringLiteral("1");

  ConfigurationResult result;
  result.requested = requested;
  result.effective = QJsonObject{ { QStringLiteral("endpoint"), QStringLiteral("/dev/ttyS0") },
                                 { QStringLiteral("baudRate"), QStringLiteral("9600") } };
  result.changedGroups = QStringList{ QStringLiteral("hardware") };
  result.applyStatus = ConfigurationApplyStatus::Partial;
  result.message = QStringLiteral("flow control rejected");

  QCOMPARE(result.applyStatusName(), QStringLiteral("partial"));

  const QJsonObject metadata = result.toMetadata();
  QCOMPARE(metadata.value(QStringLiteral("requested")).toObject().value(QStringLiteral("endpoint")).toString(),
           QStringLiteral("/dev/ttyS0"));
  const QJsonObject effective = metadata.value(QStringLiteral("effective")).toObject();
  QCOMPARE(effective.value(QStringLiteral("endpoint")).toString(), QStringLiteral("/dev/ttyS0"));
  QVERIFY(!effective.contains(QStringLiteral("portName")));
  QCOMPARE(effective.value(QStringLiteral("baudRate")).toString(), QStringLiteral("9600"));
  const QJsonObject buffering = metadata.value(QStringLiteral("localBuffering")).toObject();
  QCOMPARE(buffering.value(QStringLiteral("rxQueue")).toInt(), 2048);
  QCOMPARE(buffering.value(QStringLiteral("flushRate")).toInt(), 100);
  QCOMPARE(metadata.value(QStringLiteral("compatibility")).toObject().value(QStringLiteral("startBits")).toString(),
           QStringLiteral("1"));
  QCOMPARE(metadata.value(QStringLiteral("changedGroups")).toArray().at(0).toString(),
           QStringLiteral("hardware"));
  QCOMPARE(metadata.value(QStringLiteral("applyStatus")).toString(), QStringLiteral("partial"));
  QCOMPARE(metadata.value(QStringLiteral("message")).toString(),
           QStringLiteral("flow control rejected"));

  // A successful transaction reports no message.
  result.applyStatus = ConfigurationApplyStatus::Full;
  result.message.clear();
  QVERIFY(!result.toMetadata().contains(QStringLiteral("message")));
  QCOMPARE(result.applyStatusName(), QStringLiteral("full"));
  QCOMPARE(ConfigurationResult().applyStatusName(), QStringLiteral("full"));
}

QTEST_APPLESS_MAIN(TstSessionContract)

#include "tst_sessioncontract.moc"
