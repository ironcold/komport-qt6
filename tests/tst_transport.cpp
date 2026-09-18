/***************************************************************************
                          tst_transport.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : 2026
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de

    M8 behaviour tests for the local transport contract (SPEC-M8 section 13):
    the observation mapping of a configuration transaction, the activation
    identity and its failure guard, RX observations independent of the legacy
    buffering, and the TX observation covering exactly the accepted prefix.

    The seams of KomportSerial (writeToPort/readFromPort/applyHardwareSettings,
    SPEC-M8 section 13) are overridden by a scripted subclass, because a genuine
    partial hardware write is not reproducible here and a PTY does not select
    chunk boundaries deterministically.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "komportserial.h"

#include <QJsonArray>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTest>

#include <pty.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>

namespace {

/** A KomportSerial whose three port-facing seams are scripted (SPEC-M8 13). */
class ScriptedSerial : public KomportSerial
{
public:
  using KomportSerial::KomportSerial;

  qint64 nextWriteResult = -1;      ///< -1: accept everything
  QList<QByteArray> scriptedReads;  ///< returned by successive readFromPort() calls
  int writeCalls = 0;
  int applyCalls = 0;
  bool rejectBaudRate = false;
  bool rejectDataBits = false;
  bool rejectStopBits = false;
  bool rejectParity = false;
  bool rejectFlowControl = false;
  /** makes the scripted port raise QSerialPort::errorOccurred() while the
    * settings are being applied (SPEC-M8 6.2: a transaction stays within its
    * two-observation budget) */
  bool injectPortErrorDuringApply = false;
  /** raises the same error again at the end of the apply (after a reentrant
    * receiver had its chance to run a nested transaction) */
  bool injectPortErrorBeforeSeamReturn = false;

protected:
  qint64 writeToPort(const char *, qint64 _len) override
  {
    ++writeCalls;
    if (nextWriteResult >= 0)
      return qMin<qint64>(nextWriteResult, _len);
    return _len;
  }

  QByteArray readFromPort() override
  {
    if (scriptedReads.isEmpty())
      return QByteArray();
    return scriptedReads.takeFirst();
  }

  bool applyHardwareSettings(const TransportConfiguration &_requested,
                             const TransportConfiguration &_previous,
                             TransportConfiguration *_effective) override
  {
    ++applyCalls;
    if (injectPortErrorDuringApply) {
      QMetaObject::invokeMethod(this, "slotPortError",
                                Q_ARG(QSerialPort::SerialPortError,
                                      QSerialPort::SerialPortError::ResourceError));
    }
    TransportConfiguration effective = _previous;
    if (!rejectBaudRate) effective.baudRate = _requested.baudRate;
    if (!rejectDataBits) effective.dataBits = _requested.dataBits;
    if (!rejectStopBits) effective.stopBits = _requested.stopBits;
    if (!rejectParity) effective.parity = _requested.parity;
    if (!rejectFlowControl) effective.flowControl = _requested.flowControl;
    effective.startBits = _requested.startBits;
    if (injectPortErrorBeforeSeamReturn) {
      QMetaObject::invokeMethod(this, "slotPortError",
                                Q_ARG(QSerialPort::SerialPortError,
                                      QSerialPort::SerialPortError::ResourceError));
    }
    if (_effective != nullptr)
      *_effective = effective;
    return !rejectBaudRate && !rejectDataBits && !rejectStopBits
        && !rejectParity && !rejectFlowControl;
  }
};

/** Names of the configuration groups as they appear in the metadata. */
QStringList changedGroupsOf(const QSignalSpy &spy, int index)
{
  const QJsonObject metadata = spy.at(index).at(2).value<QJsonObject>();
  QStringList groups;
  const QJsonArray array = metadata.value(QStringLiteral("changedGroups")).toArray();
  for (const QJsonValue &value : array)
    groups.append(value.toString());
  return groups;
}

/** Fails with both lists printed when they differ (QCOMPARE on two lists would
  *  otherwise report only their sizes). */
void compareGroups(const QStringList &actual, const QStringList &expected)
{
  QCOMPARE(actual.join(QLatin1Char(',')), expected.join(QLatin1Char(',')));
}

QString applyStatusOf(const QSignalSpy &spy, int index)
{
  const QJsonObject metadata = spy.at(index).at(2).value<QJsonObject>();
  return metadata.value(QStringLiteral("applyStatus")).toString();
}

QString kindOf(const QSignalSpy &spy, int index)
{
  const QJsonObject metadata = spy.at(index).at(2).value<QJsonObject>();
  return metadata.value(QStringLiteral("kind")).toString();
}

} // namespace

class TstTransport : public QObject
{
  Q_OBJECT
private slots:
  void openAndConfigureProducesOneOpenedAndOneTransaction();
  void failedOpenReportsExactlyOneOpenErrorAndConsumesAnActivationId();
  void rxObservationsArePerNonEmptyReadAndIndependentOfRxQueue();
  void txObservationCoversTheAcceptedPrefixAndReportsTheRefusal();
  void everyWriteEntryPointProducesExactlyOneObservation();
  void noOpConfigurationEmitsNothing();
  void bufferingAndCompatibilityChangesReportTheirOwnGroup();
  void partialAndFailedAppliesReportTheirApplyStatus();
  void configureWhileClosedStoresOnlyAndIsAppliedByTheNextOpen();
  void reopenEmitsClosedAndKeepsActivationIdsIncreasing();
  void endpointChangeWhileOpenIsAnActivationChange();
  void failedEndpointChangeReportsAFailedResult();
  void configurationMetadataCarriesTheEndpointAndReadBackValues();
  void zeroAcceptedWriteReportsOneErrorAndNoDataEvent();
  void legacySettersKeepTheirNotificationBehaviour();
  void unusableBaudRateIsReportedAndNotStored();
  void ptyCarriesEveryByteValueInBothDirections();
  void portErrorDuringApplyStaysWithinTheObservationBudget();
  void reentrantLegacyReceiverKeepsTheTransactionBudget();
  void observationsRequireALiveActivation();
  void byteWiseSixtyFourKiBUploadKeepsOneObservationPerAcceptedWrite();
};

/** The applyStatus of a returned result, as the metadata reports it. */
QString applyStatusOfResult(const ConfigurationResult &_result)
{
  return _result.toMetadata().value(QStringLiteral("applyStatus")).toString();
}

/** Read up to @p expected bytes from a non-blocking fd; returns what arrived. */
static int readIntoFromFd(int fd, QByteArray *out, int expected)
{
  while ( out->size() < expected ) {
    char buffer[1024];
    const ssize_t received = ::read(fd, buffer, sizeof(buffer));
    if ( received > 0 ) {
      out->append(buffer, int(received));
      continue;
    }
    if ( received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) ) {
      QTest::qWait(5);
      continue;
    }
    break;   // error or end of stream
  }
  return out->size();
}

/** Create a pty pair and return the slave path, or an empty string when the
  * sandbox has no openpty(). The caller keeps @p masterFd open for the duration
  * of the test (closing it would end the slave side). */
static QString makePty(int *masterFd)
{
  int slaveFd = -1;
  char slaveName[256];
  if ( ::openpty(masterFd, &slaveFd, slaveName, nullptr, nullptr) != 0 )
    return QString();
  ::close(slaveFd); // QSerialPort opens its own fd on the slave path
  return QString::fromLocal8Bit(slaveName);
}

void TstTransport::openAndConfigureProducesOneOpenedAndOneTransaction()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if ( slaveName.isEmpty() )
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });

  ScriptedSerial serial;
  serial.setDeviceName(slaveName);
  QSignalSpy openedSpy(&serial, &KomportSerial::opened);
  QSignalSpy configSpy(&serial, &KomportSerial::configurationChanged);
  QSignalSpy errorSpy(&serial, &KomportSerial::transportError);

  QVERIFY(serial.open());

  QCOMPARE(openedSpy.size(), 1);
  QCOMPARE(configSpy.size(), 1);
  QCOMPARE(errorSpy.size(), 0);
  // The open applies the configuration exactly once - the pre-M8 duplicate
  // hardware application per open() is gone (SPEC-M8 section 3/6.2).
  QCOMPARE(serial.applyCalls, 1);
  QCOMPARE(applyStatusOf(configSpy, 0), QStringLiteral("full"));
  // A new activation always applies the hardware settings, so the transaction
  // reports the hardware group even though the values equal the snapshot.
  compareGroups(changedGroupsOf(configSpy, 0), QStringList{ QStringLiteral("hardware") });

  const quint64 openedId = openedSpy.at(0).at(0).value<quint64>();
  QCOMPARE(openedId, quint64(1));
  QCOMPARE(configSpy.at(0).at(0).value<quint64>(), openedId);

  // A second, identical configure is the documented no-op.
  const ConfigurationResult result = serial.applyConfiguration(serial.requestedConfiguration());
  QCOMPARE(result.storedOnly, false);
  QVERIFY(result.changedGroups.isEmpty());
  QCOMPARE(configSpy.size(), 1);
  QCOMPARE(serial.applyCalls, 1);
}

void TstTransport::failedOpenReportsExactlyOneOpenErrorAndConsumesAnActivationId()
{
  ScriptedSerial serial;
  // tst_profileerror relies on QSerialPort failing synchronously for this path,
  // which is exactly the case where a naive implementation would report the
  // failure twice (once from open(), once from the async error).
  serial.setDeviceName(QStringLiteral("/dev/definitely-does-not-exist-komport-m8-test"));
  QSignalSpy openedSpy(&serial, &KomportSerial::opened);
  QSignalSpy errorSpy(&serial, &KomportSerial::transportError);

  QVERIFY(!serial.open());
  QCOMPARE(openedSpy.size(), 0);
  QCOMPARE(errorSpy.size(), 1);
  QCOMPARE(kindOf(errorSpy, 0), QStringLiteral("open"));
  QCOMPARE(errorSpy.at(0).at(0).value<quint64>(), quint64(1));

  // A second attempt consumes the next activation id and again reports once.
  QVERIFY(!serial.open());
  QCOMPARE(errorSpy.size(), 2);
  QCOMPARE(errorSpy.at(1).at(0).value<quint64>(), quint64(2));
}

void TstTransport::rxObservationsArePerNonEmptyReadAndIndependentOfRxQueue()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if ( slaveName.isEmpty() )
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });

  ScriptedSerial serial;
  serial.setDeviceName(slaveName);
  QVERIFY(serial.open());
  QSignalSpy rxSpy(&serial, &KomportSerial::bytesReceived);

  // A tiny RX queue makes the legacy display path lossy; the event stream must
  // not be (SPEC-M8 section 8).
  serial.setRxQueue(1);
  serial.setFlushRate(1000);

  serial.scriptedReads = { QByteArray("abc"), QByteArray(), QByteArray::fromHex("0001ff") };
  QMetaObject::invokeMethod(&serial, "slotDataAvailable");
  QMetaObject::invokeMethod(&serial, "slotDataAvailable");
  QMetaObject::invokeMethod(&serial, "slotDataAvailable");

  QCOMPARE(rxSpy.size(), 2); // the empty read produces no observation
  QCOMPARE(rxSpy.at(0).at(1).value<QByteArray>(), QByteArray("abc"));
  QCOMPARE(rxSpy.at(1).at(1).value<QByteArray>(), QByteArray::fromHex("0001ff"));
  QCOMPARE(rxSpy.at(0).at(0).value<quint64>(), rxSpy.at(1).at(0).value<quint64>());
}

void TstTransport::txObservationCoversTheAcceptedPrefixAndReportsTheRefusal()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if ( slaveName.isEmpty() )
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });

  ScriptedSerial serial;
  serial.setDeviceName(slaveName);
  QVERIFY(serial.open());
  QSignalSpy txSpy(&serial, &KomportSerial::bytesWritten);
  QSignalSpy errorSpy(&serial, &KomportSerial::transportError);
  QSignalSpy sentSpy(&serial, &KomportSerial::sentChar);

  serial.nextWriteResult = 2; // the port accepts only the first two bytes
  QVERIFY(!serial.putStr("hello", 5));

  QCOMPARE(txSpy.size(), 1);
  QCOMPARE(txSpy.at(0).at(1).value<QByteArray>(), QByteArray("he"));
  QCOMPARE(sentSpy.size(), 2); // legacy hex-monitor path unchanged
  QCOMPARE(errorSpy.size(), 1);
  QCOMPARE(kindOf(errorSpy, 0), QStringLiteral("write"));
  const QJsonObject metadata = errorSpy.at(0).at(2).value<QJsonObject>();
  QCOMPARE(metadata.value(QStringLiteral("acceptedBytes")).toInt(), 2);
  QCOMPARE(metadata.value(QStringLiteral("refusedBytes")).toInt(), 3);
}

void TstTransport::everyWriteEntryPointProducesExactlyOneObservation()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if ( slaveName.isEmpty() )
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });

  ScriptedSerial serial;
  serial.setDeviceName(slaveName);
  QVERIFY(serial.open());
  QSignalSpy txSpy(&serial, &KomportSerial::bytesWritten);

  QVERIFY(serial.putChar('A'));
  QVERIFY(serial.putStr("BC"));
  QCOMPARE(serial.writeBytes(QByteArray("DE")), Q_INT64_C(2));

  QCOMPARE(txSpy.size(), 3);
  QCOMPARE(txSpy.at(0).at(1).value<QByteArray>(), QByteArray("A"));
  QCOMPARE(txSpy.at(1).at(1).value<QByteArray>(), QByteArray("BC"));
  QCOMPARE(txSpy.at(2).at(1).value<QByteArray>(), QByteArray("DE"));

  // Nothing is sent while the port is closed, and nothing is observed either.
  ScriptedSerial closed;
  QSignalSpy closedTx(&closed, &KomportSerial::bytesWritten);
  QCOMPARE(closed.writeBytes(QByteArray("x")), Q_INT64_C(-1));
  QVERIFY(!closed.putChar('x'));
  QCOMPARE(closedTx.size(), 0);
}

void TstTransport::noOpConfigurationEmitsNothing()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if ( slaveName.isEmpty() )
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });

  ScriptedSerial serial;
  serial.setDeviceName(slaveName);
  QVERIFY(serial.open());
  QSignalSpy configSpy(&serial, &KomportSerial::configurationChanged);
  QSignalSpy errorSpy(&serial, &KomportSerial::transportError);
  const int appliesBefore = serial.applyCalls;

  const ConfigurationResult result = serial.applyConfiguration(serial.effectiveConfiguration());

  QVERIFY(result.changedGroups.isEmpty());
  QCOMPARE(result.applyStatusName(), QStringLiteral("full"));
  QCOMPARE(configSpy.size(), 0);
  QCOMPARE(errorSpy.size(), 0);
  QCOMPARE(serial.applyCalls, appliesBefore); // no hardware application at all
}

void TstTransport::bufferingAndCompatibilityChangesReportTheirOwnGroup()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if ( slaveName.isEmpty() )
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });

  ScriptedSerial serial;
  serial.setDeviceName(slaveName);
  QVERIFY(serial.open());
  QSignalSpy configSpy(&serial, &KomportSerial::configurationChanged);
  const int appliesBefore = serial.applyCalls;

  TransportConfiguration buffering = serial.requestedConfiguration();
  buffering.rxQueue = serial.requestedConfiguration().rxQueue + 512;
  QVERIFY(!serial.applyConfiguration(buffering).changedGroups.isEmpty());
  QCOMPARE(configSpy.size(), 1);
  compareGroups(changedGroupsOf(configSpy, 0), QStringList{ QStringLiteral("localBuffering") });
  QCOMPARE(serial.applyCalls, appliesBefore); // no hardware application

  // startBits is stored and reported but never applied to hardware.
  TransportConfiguration compatibility = serial.requestedConfiguration();
  compatibility.startBits = QStringLiteral("2");
  serial.applyConfiguration(compatibility);
  QCOMPARE(configSpy.size(), 2);
  compareGroups(changedGroupsOf(configSpy, 1), QStringList{ QStringLiteral("compatibility") });
  QCOMPARE(serial.applyCalls, appliesBefore);
  QCOMPARE(serial.effectiveConfiguration().startBits, QStringLiteral("2"));
}

void TstTransport::partialAndFailedAppliesReportTheirApplyStatus()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if ( slaveName.isEmpty() )
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });

  ScriptedSerial serial;
  serial.setDeviceName(slaveName);
  QVERIFY(serial.open());
  QSignalSpy configSpy(&serial, &KomportSerial::configurationChanged);
  QSignalSpy errorSpy(&serial, &KomportSerial::transportError);

  // Partial: the port takes all but the baud rate, and another hardware field
  // really changes - so the effective state differs from the previous one.
  // The two observations of a transaction also have a defined order.
  QStringList observationOrder;
  QObject::connect(&serial, &KomportSerial::configurationChanged,
                   [&observationOrder](quint64, qint64, const QJsonObject &) {
                     observationOrder.append(QStringLiteral("configurationChanged"));
                   });
  QObject::connect(&serial, &KomportSerial::transportError,
                   [&observationOrder](quint64, qint64, const QJsonObject &) {
                     observationOrder.append(QStringLiteral("transportError"));
                   });
  serial.rejectBaudRate = true;
  TransportConfiguration partial = serial.requestedConfiguration();
  partial.baudRate = QStringLiteral("19200");   // rejected
  partial.parity = QStringLiteral("EVEN");      // accepted
  serial.applyConfiguration(partial);
  QCOMPARE(configSpy.size(), 1);
  QCOMPARE(applyStatusOf(configSpy, 0), QStringLiteral("partial"));
  QCOMPARE(errorSpy.size(), 1);
  QCOMPARE(kindOf(errorSpy, 0), QStringLiteral("apply"));
  QCOMPARE(errorSpy.at(0).at(2).value<QJsonObject>().value(QStringLiteral("code")).toString(),
           QStringLiteral("apply_partial"));
  // The rejected field keeps its previous value in the effective snapshot, the
  // accepted one carries the new value.
  QCOMPARE(serial.effectiveConfiguration().baudRate, QStringLiteral("9600"));
  QCOMPARE(serial.effectiveConfiguration().parity, QStringLiteral("EVEN"));
  // Exactly two observations, in the order the mapping prescribes.
  QCOMPARE(observationOrder.join(QLatin1Char(',')),
           QStringLiteral("configurationChanged,transportError"));

  // Failed without any change: a request whose only hardware difference is
  // rejected changes nothing, so it reports only an Error.
  serial.rejectDataBits = true;
  serial.rejectStopBits = true;
  serial.rejectParity = true;
  serial.rejectFlowControl = true;
  TransportConfiguration failed = serial.requestedConfiguration();
  failed.flowControl = QStringLiteral("RTS/CTS");
  serial.applyConfiguration(failed);
  QCOMPARE(configSpy.size(), 1);           // unchanged
  QCOMPARE(errorSpy.size(), 2);
  QCOMPARE(errorSpy.at(1).at(2).value<QJsonObject>().value(QStringLiteral("code")).toString(),
           QStringLiteral("apply_failed"));
}

void TstTransport::configureWhileClosedStoresOnlyAndIsAppliedByTheNextOpen()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if ( slaveName.isEmpty() )
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });

  ScriptedSerial serial;
  QSignalSpy configSpy(&serial, &KomportSerial::configurationChanged);
  QSignalSpy openedSpy(&serial, &KomportSerial::opened);

  TransportConfiguration requested;
  requested.endpoint = slaveName;
  requested.baudRate = QStringLiteral("38400");
  requested.dataBits = QStringLiteral("8");
  requested.stopBits = QStringLiteral("1");
  requested.parity = QStringLiteral("NONE");
  requested.flowControl = QStringLiteral("NONE");
  requested.rxQueue = 2048;
  requested.flushRate = 100;
  requested.startBits = QStringLiteral("1");

  const ConfigurationResult stored = serial.applyConfiguration(requested);
  QVERIFY(stored.storedOnly);
  QCOMPARE(configSpy.size(), 0);       // nothing is emitted while closed
  QCOMPARE(serial.applyCalls, 0);      // nothing is applied while closed
  QCOMPARE(serial.flushRate(), 100);   // local buffering takes effect immediately
  QCOMPARE(serial.setRxQueue(2048), 2048);

  // The next open performs the single open-and-configure transaction.
  QVERIFY(serial.open());
  QCOMPARE(openedSpy.size(), 1);
  QCOMPARE(configSpy.size(), 1);
  QCOMPARE(serial.applyCalls, 1);
  QCOMPARE(serial.effectiveConfiguration().baudRate, QStringLiteral("38400"));
}

void TstTransport::reopenEmitsClosedAndKeepsActivationIdsIncreasing()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if ( slaveName.isEmpty() )
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });

  ScriptedSerial serial;
  serial.setDeviceName(slaveName);
  QSignalSpy openedSpy(&serial, &KomportSerial::opened);
  QSignalSpy closedSpy(&serial, &KomportSerial::closed);

  QVERIFY(serial.open());
  QCOMPARE(openedSpy.at(0).at(0).value<quint64>(), quint64(1));

  serial.close();
  QCOMPARE(closedSpy.size(), 1);
  QCOMPARE(closedSpy.at(0).at(0).value<quint64>(), quint64(1));

  QVERIFY(serial.open());
  QCOMPARE(openedSpy.size(), 2);
  QCOMPARE(openedSpy.at(1).at(0).value<quint64>(), quint64(2)); // no reset
  QCOMPARE(closedSpy.size(), 1);

  // open() on an already open port is an activation change: close then open.
  QVERIFY(serial.open());
  QCOMPARE(openedSpy.size(), 3);
  QCOMPARE(closedSpy.size(), 2);
  QCOMPARE(closedSpy.at(1).at(0).value<quint64>(), quint64(2));
  QCOMPARE(openedSpy.at(2).at(0).value<quint64>(), quint64(3));
}

void TstTransport::endpointChangeWhileOpenIsAnActivationChange()
{
  int firstMaster = -1;
  int secondMaster = -1;
  const QString first = makePty(&firstMaster);
  const QString second = makePty(&secondMaster);
  if ( first.isEmpty() || second.isEmpty() )
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&firstMaster, &secondMaster]() {
    if (firstMaster >= 0) ::close(firstMaster);
    if (secondMaster >= 0) ::close(secondMaster);
  });

  ScriptedSerial serial;
  serial.setDeviceName(first);
  QSignalSpy openedSpy(&serial, &KomportSerial::opened);
  QSignalSpy closedSpy(&serial, &KomportSerial::closed);
  QSignalSpy configSpy(&serial, &KomportSerial::configurationChanged);
  QVERIFY(serial.open());
  QCOMPARE(openedSpy.size(), 1);
  QCOMPARE(configSpy.size(), 1);

  // configure(request) on a live port with a changed endpoint is an activation
  // change (SPEC-M8 6.2 operation table): old TransportClosed, new
  // TransportOpened, then exactly one transaction result - the entry point must
  // never report a successful configuration for a port it never opened.
  TransportConfiguration moved = serial.requestedConfiguration();
  moved.endpoint = second;
  const ConfigurationResult result = serial.applyConfiguration(moved);

  QVERIFY(!result.storedOnly);
  QCOMPARE(closedSpy.size(), 1);
  QCOMPARE(closedSpy.at(0).at(0).value<quint64>(), quint64(1));   // old activation
  QCOMPARE(openedSpy.size(), 2);
  QCOMPARE(openedSpy.at(1).at(0).value<quint64>(), quint64(2));   // new activation
  QCOMPARE(configSpy.size(), 2);
  QCOMPARE(configSpy.at(1).at(0).value<quint64>(), quint64(2));   // the new one
  compareGroups(changedGroupsOf(configSpy, 1), QStringList{ QStringLiteral("hardware") });
  QCOMPARE(serial.requestedConfiguration().endpoint, second);
  QCOMPARE(serial.effectiveConfiguration().endpoint, second);
  QVERIFY(serial.isOpen());
}

void TstTransport::failedEndpointChangeReportsAFailedResult()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if ( slaveName.isEmpty() )
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });

  ScriptedSerial serial;
  serial.setDeviceName(slaveName);
  QSignalSpy openedSpy(&serial, &KomportSerial::opened);
  QSignalSpy closedSpy(&serial, &KomportSerial::closed);
  QSignalSpy configSpy(&serial, &KomportSerial::configurationChanged);
  QSignalSpy errorSpy(&serial, &KomportSerial::transportError);
  QVERIFY(serial.open());
  QCOMPARE(configSpy.size(), 1);

  // The reopen of a live endpoint change fails: this is not a store-only
  // operation (it started on a live port and consumed an open attempt) and it
  // must not add a second report of the same failure.
  TransportConfiguration moved = serial.requestedConfiguration();
  moved.endpoint = QStringLiteral("/dev/definitely-does-not-exist-komport-m8-test");
  const ConfigurationResult result = serial.applyConfiguration(moved);

  QVERIFY(!result.storedOnly);
  QCOMPARE(applyStatusOfResult(result), QStringLiteral("failed"));
  QVERIFY(!result.message.isEmpty());
  QCOMPARE(closedSpy.size(), 1);        // the old activation was closed
  QCOMPARE(openedSpy.size(), 1);        // the new one never opened
  QCOMPARE(configSpy.size(), 1);        // ... so there is no new transaction
  QCOMPARE(errorSpy.size(), 1);         // exactly one Error, from the open attempt
  QCOMPARE(errorSpy.at(0).at(2).value<QJsonObject>().value(QStringLiteral("kind")).toString(),
           QStringLiteral("open"));
  QVERIFY(!serial.isOpen());
}

void TstTransport::configurationMetadataCarriesTheEndpointAndReadBackValues()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if ( slaveName.isEmpty() )
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });

  // A real KomportSerial (no scripted seam): the metadata must carry what the
  // port reports, not what was requested.
  KomportSerial serial;
  serial.setDeviceName(slaveName);
  serial.setBaudRate(qint32(9600));
  QSignalSpy configSpy(&serial, &KomportSerial::configurationChanged);
  QVERIFY(serial.open());
  QCOMPARE(configSpy.size(), 1);

  TransportConfiguration requested = serial.requestedConfiguration();
  requested.baudRate = QStringLiteral("19200");
  serial.applyConfiguration(requested);
  QCOMPARE(configSpy.size(), 2);

  const QJsonObject metadata = configSpy.at(1).at(2).value<QJsonObject>();
  const QJsonObject effective = metadata.value(QStringLiteral("effective")).toObject();
  QCOMPARE(effective.value(QStringLiteral("endpoint")).toString(), slaveName);
  QVERIFY(effective.contains(QStringLiteral("endpoint")));
  QVERIFY(!effective.contains(QStringLiteral("portName")));  // the pre-fix key
  QCOMPARE(effective.value(QStringLiteral("baudRate")).toString(), QStringLiteral("19200"));
  QCOMPARE(effective.value(QStringLiteral("dataBits")).toString(), QStringLiteral("8"));
  QCOMPARE(effective.value(QStringLiteral("stopBits")).toString(), QStringLiteral("1"));
  QCOMPARE(effective.value(QStringLiteral("parity")).toString(), QStringLiteral("NONE"));
  QCOMPARE(effective.value(QStringLiteral("flowControl")).toString(), QStringLiteral("NONE"));
  // ... and the transport state agrees with the reported read-back.
  QCOMPARE(serial.effectiveConfiguration().baudRate, QStringLiteral("19200"));
  QCOMPARE(serial.baudRate(), qint32(19200));
}

void TstTransport::zeroAcceptedWriteReportsOneErrorAndNoDataEvent()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if ( slaveName.isEmpty() )
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });

  ScriptedSerial serial;
  serial.setDeviceName(slaveName);
  QVERIFY(serial.open());
  QSignalSpy writtenSpy(&serial, &KomportSerial::bytesWritten);
  QSignalSpy sentSpy(&serial, &KomportSerial::sentChar);
  QSignalSpy errorSpy(&serial, &KomportSerial::transportError);

  // A write the transport refuses completely: no data event, exactly one Error.
  serial.nextWriteResult = 0;
  QVERIFY(!serial.putStr("abc", 3));
  QCOMPARE(writtenSpy.size(), 0);
  QCOMPARE(sentSpy.size(), 0);
  QCOMPARE(errorSpy.size(), 1);
  QCOMPARE(errorSpy.at(0).at(2).value<QJsonObject>().value(QStringLiteral("kind")).toString(),
           QStringLiteral("write"));
  QCOMPARE(errorSpy.at(0).at(2).value<QJsonObject>().value(QStringLiteral("acceptedBytes")).toInt(), 0);
  QCOMPARE(errorSpy.at(0).at(2).value<QJsonObject>().value(QStringLiteral("refusedBytes")).toInt(), 3);

  serial.nextWriteResult = -1;
  QCOMPARE(serial.writeBytes(QByteArrayLiteral("xy")), qint64(2));
  QCOMPARE(writtenSpy.size(), 1);
}

void TstTransport::legacySettersKeepTheirNotificationBehaviour()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if ( slaveName.isEmpty() )
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });

  ScriptedSerial serial;
  serial.setDeviceName(slaveName);
  QSignalSpy changedSpy(&serial, &KomportSerial::settingsChanged);
  QSignalSpy configSpy(&serial, &KomportSerial::configurationChanged);

  QVERIFY(serial.open());
  QCOMPARE(changedSpy.size(), 1);      // open() notified, exactly as before

  // Every legacy setter becomes observable through the one transaction path; the
  // compatibility notification stays what that setter emitted before M8.
  serial.setBaudRate(qint32(19200));
  QCOMPARE(configSpy.size(), 2);
  compareGroups(changedGroupsOf(configSpy, 1), QStringList{ QStringLiteral("hardware") });
  QCOMPARE(changedSpy.size(), 2);      // setBaudRate notified, as before

  serial.setFraming(QStringLiteral("1"), QStringLiteral("7"), QStringLiteral("2"),
                    QStringLiteral("EVEN"));
  QCOMPARE(configSpy.size(), 3);
  compareGroups(changedGroupsOf(configSpy, 2), QStringList{ QStringLiteral("hardware") });
  QCOMPARE(changedSpy.size(), 2);      // setFraming never notified

  serial.setFlowControl(QStringLiteral("RTS/CTS"));
  QCOMPARE(configSpy.size(), 4);
  compareGroups(changedGroupsOf(configSpy, 3), QStringList{ QStringLiteral("hardware") });
  QCOMPARE(changedSpy.size(), 2);      // setFlowControl never notified

  // A setter whose value does not change is the documented no-op.
  serial.setFlowControl(QStringLiteral("RTS/CTS"));
  QCOMPARE(configSpy.size(), 4);
}

void TstTransport::unusableBaudRateIsReportedAndNotStored()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if ( slaveName.isEmpty() )
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });

  ScriptedSerial serial;
  serial.setDeviceName(slaveName);
  QVERIFY(serial.open());
  QSignalSpy configSpy(&serial, &KomportSerial::configurationChanged);
  QSignalSpy errorSpy(&serial, &KomportSerial::transportError);
  QSignalSpy failedSpy(&serial, &KomportSerial::settingsFailed);

  TransportConfiguration requested = serial.requestedConfiguration();
  requested.baudRate = QStringLiteral("not-a-rate");
  const ConfigurationResult result = serial.applyConfiguration(requested);

  QCOMPARE(applyStatusOfResult(result), QStringLiteral("failed"));
  QVERIFY(!result.message.isEmpty());
  QCOMPARE(configSpy.size(), 0);        // nothing changed ...
  QCOMPARE(errorSpy.size(), 1);         // ... but it is reported, not swallowed
  QCOMPARE(errorSpy.at(0).at(2).value<QJsonObject>().value(QStringLiteral("code")).toString(),
           QStringLiteral("apply_failed"));
  QCOMPARE(failedSpy.size(), 1);        // legacy user-facing path, synchronous
  // The unusable value never becomes the stored or the effective state.
  QCOMPARE(serial.requestedConfiguration().baudRate, QStringLiteral("9600"));
  QCOMPARE(serial.effectiveConfiguration().baudRate, QStringLiteral("9600"));
  QCOMPARE(serial.baudRate(), qint32(9600));
}

void TstTransport::ptyCarriesEveryByteValueInBothDirections()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if ( slaveName.isEmpty() )
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });
  ::fcntl(masterFd, F_SETFL, ::fcntl(masterFd, F_GETFL, 0) | O_NONBLOCK);

  // End-to-end through the real QSerialPort: every byte value 0x00..0xFF must
  // arrive unchanged and be observed loss-free in both directions.
  KomportSerial serial;
  serial.setDeviceName(slaveName);
  QVERIFY(serial.open());
  QSignalSpy writtenSpy(&serial, &KomportSerial::bytesWritten);
  QSignalSpy sentSpy(&serial, &KomportSerial::sentChar);
  QSignalSpy receivedSpy(&serial, &KomportSerial::bytesReceived);

  QByteArray allBytes;
  for ( int value = 0; value <= 0xFF; ++value )
    allBytes.append(char(value));

  // TX: the peer receives exactly these bytes, and the observations cover them.
  QCOMPARE(serial.writeBytes(allBytes), qint64(allBytes.size()));
  QByteArray observedTx;
  for ( int i = 0; i < writtenSpy.size(); ++i )
    observedTx += writtenSpy.at(i).at(1).value<QByteArray>();
  QCOMPARE(observedTx, allBytes);
  QCOMPARE(sentSpy.size(), allBytes.size());   // e.g. the hex monitor's view

  QByteArray atPeer;
  QTRY_VERIFY_WITH_TIMEOUT(readIntoFromFd(masterFd, &atPeer, allBytes.size()) == allBytes.size(), 5000);
  QCOMPARE(atPeer, allBytes);

  // RX: the same bytes coming back are observed byte for byte.
  QCOMPARE(::write(masterFd, allBytes.constData(), allBytes.size()), qint64(allBytes.size()));
  QTRY_VERIFY_WITH_TIMEOUT(receivedSpy.size() >= 1, 5000);
  QByteArray observedRx;
  for ( int i = 0; i < receivedSpy.size(); ++i )
    observedRx += receivedSpy.at(i).at(1).value<QByteArray>();
  QCOMPARE(observedRx, allBytes);
}

void TstTransport::portErrorDuringApplyStaysWithinTheObservationBudget()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if ( slaveName.isEmpty() )
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });

  ScriptedSerial serial;
  serial.setDeviceName(slaveName);
  QVERIFY(serial.open());
  QSignalSpy configSpy(&serial, &KomportSerial::configurationChanged);
  QSignalSpy errorSpy(&serial, &KomportSerial::transportError);
  QSignalSpy failedSpy(&serial, &KomportSerial::settingsFailed);

  // A port error raised by the port while the settings are being applied belongs
  // to the transaction: it must not become a third observation (SPEC-M8 6.2 caps
  // a transaction at two).
  serial.injectPortErrorDuringApply = true;
  serial.rejectBaudRate = true;
  TransportConfiguration partial = serial.requestedConfiguration();
  partial.baudRate = QStringLiteral("19200");   // rejected
  partial.parity = QStringLiteral("EVEN");      // accepted
  serial.applyConfiguration(partial);
  QCOMPARE(configSpy.size(), 1);
  QCOMPARE(errorSpy.size(), 1);                 // the transaction's own error
  QCOMPARE(kindOf(errorSpy, 0), QStringLiteral("apply"));
  QCOMPARE(errorSpy.at(0).at(2).value<QJsonObject>().value(QStringLiteral("code")).toString(),
           QStringLiteral("apply_partial"));
  QVERIFY(failedSpy.size() >= 1);                // legacy path still informed

  // An apply that succeeds although the port raised an error still records it:
  // the failure reaches the session record as this transaction's own apply error
  // (SPEC-M8 section 9), never as a second runtime event.
  configSpy.clear();
  errorSpy.clear();
  serial.rejectBaudRate = false;
  TransportConfiguration full = serial.requestedConfiguration();
  full.parity = QStringLiteral("ODD");
  serial.applyConfiguration(full);
  QCOMPARE(configSpy.size(), 1);
  QCOMPARE(errorSpy.size(), 1);
  QCOMPARE(kindOf(errorSpy, 0), QStringLiteral("apply"));
  QCOMPARE(errorSpy.at(0).at(2).value<QJsonObject>().value(QStringLiteral("code")).toString(),
           QStringLiteral("apply_partial"));   // changed, but not cleanly
}

void TstTransport::observationsRequireALiveActivation()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if ( slaveName.isEmpty() )
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });

  ScriptedSerial serial;
  serial.setDeviceName(slaveName);
  QSignalSpy receivedSpy(&serial, &KomportSerial::bytesReceived);
  QSignalSpy charSpy(&serial, &KomportSerial::receivedChar);
  QSignalSpy errorSpy(&serial, &KomportSerial::transportError);
  QSignalSpy failedSpy(&serial, &KomportSerial::settingsFailed);

  QVERIFY(serial.open());
  serial.close();          // the live activation is reset to 0 here
  QCOMPARE(receivedSpy.size(), 0);

  // A read arriving after the close still feeds the legacy display path, but it is
  // no event: an observation must never carry the id 0 (ADR-003).
  serial.scriptedReads << QByteArrayLiteral("AB");
  QVERIFY(QMetaObject::invokeMethod(&serial, "slotDataAvailable"));
  QCOMPARE(receivedSpy.size(), 0);
  QVERIFY(QMetaObject::invokeMethod(&serial, "slotFlushRxBuffer"));
  QCOMPARE(charSpy.size(), 2);           // legacy behaviour preserved

  // A runtime port error after the close is no event either; the legacy
  // notification stays.
  const int failedBefore = failedSpy.size();
  QVERIFY(QMetaObject::invokeMethod(&serial, "slotPortError",
                                    Q_ARG(QSerialPort::SerialPortError,
                                          QSerialPort::SerialPortError::ResourceError)));
  QCOMPARE(errorSpy.size(), 0);
  QVERIFY(failedSpy.size() > failedBefore);
}

void TstTransport::reentrantLegacyReceiverKeepsTheTransactionBudget()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if ( slaveName.isEmpty() )
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });

  ScriptedSerial serial;
  serial.setDeviceName(slaveName);
  QVERIFY(serial.open());
  QSignalSpy configSpy(&serial, &KomportSerial::configurationChanged);
  QSignalSpy errorSpy(&serial, &KomportSerial::transportError);

  // A legacy receiver that reacts to settingsFailed() by configuring again runs a
  // nested transaction *inside* the outer one. The nested transaction must not end
  // the outer apply window, otherwise a later error of the outer apply would
  // escape as a runtime observation and the transaction would exceed its budget.
  int settingsFailedCalls = 0;
  bool nestedTransactionRan = false;
  QObject::connect(&serial, &KomportSerial::settingsFailed, [&serial, &settingsFailedCalls,
                                                             &nestedTransactionRan](const QString &) {
    ++settingsFailedCalls;
    if (!nestedTransactionRan) {
      nestedTransactionRan = true;
      TransportConfiguration nested = serial.requestedConfiguration();
      nested.parity = QStringLiteral("ODD");     // forces a hardware application
      serial.applyConfiguration(nested);
    }
  });

  serial.injectPortErrorDuringApply = true;       // before the nested transaction
  serial.injectPortErrorBeforeSeamReturn = true;  // and after it returned
  serial.rejectBaudRate = true;
  TransportConfiguration partial = serial.requestedConfiguration();
  partial.baudRate = QStringLiteral("19200");
  partial.stopBits = QStringLiteral("2");
  serial.applyConfiguration(partial);

  QVERIFY(nestedTransactionRan);                  // the reentrancy really happened
  QVERIFY(settingsFailedCalls >= 2);              // the nested one was reported too
  QVERIFY(errorSpy.size() >= 1);
  // Every error observation belongs to a transaction: a port error escaping as a
  // runtime event would prove the outer apply window had been ended early.
  for ( int i = 0; i < errorSpy.size(); ++i ) {
    QCOMPARE(errorSpy.at(i).at(2).value<QJsonObject>().value(QStringLiteral("kind")).toString(),
             QStringLiteral("apply"));
  }
  // Each transaction stays within its own budget, which cannot be larger than the
  // count of its configuration observations plus one error.
  QVERIFY(configSpy.size() >= 1);
  QVERIFY(errorSpy.size() <= configSpy.size() + 1);
}

void TstTransport::byteWiseSixtyFourKiBUploadKeepsOneObservationPerAcceptedWrite()
{
  int masterFd = -1;
  const QString slaveName = makePty(&masterFd);
  if ( slaveName.isEmpty() )
    QSKIP("openpty() unavailable in this sandbox");
  auto guard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });
  ::fcntl(masterFd, F_SETFL, ::fcntl(masterFd, F_GETFL, 0) | O_NONBLOCK);

  // SPEC-M8 section 13: 64 KiB written byte-wise through putChar(), exactly as
  // KomportTransfer::upload() does - one TX event per accepted write, with the
  // correct payload bytes and the correct order. The consumer is a counter, not
  // an in-memory event vector, exactly as the specification demands.
  KomportSerial serial;
  serial.setDeviceName(slaveName);
  QVERIFY(serial.open());

  const int totalBytes = 64 * 1024;
  int txEvents = 0;
  int sentChars = 0;
  bool everyPayloadIsOneByte = true;
  QByteArray observed;
  QObject::connect(&serial, &KomportSerial::bytesWritten,
                   [&txEvents, &everyPayloadIsOneByte, &observed](quint64, const QByteArray &payload, qint64) {
                     ++txEvents;
                     if (payload.size() != 1)
                       everyPayloadIsOneByte = false;
                     observed += payload;
                   });
  QObject::connect(&serial, &KomportSerial::sentChar, [&sentChars](char) { ++sentChars; });

  QByteArray expected;
  expected.reserve(totalBytes);
  for ( int i = 0; i < totalBytes; ++i )
    expected.append(char(i & 0xFF));

  int accepted = 0;
  for ( int i = 0; i < totalBytes; ++i ) {
    if (serial.putChar(char(i & 0xFF)))
      ++accepted;
  }
  QCOMPARE(accepted, totalBytes);      // no accepted write is lost
  QCOMPARE(txEvents, totalBytes);      // exactly one TX event per accepted write
  QVERIFY(everyPayloadIsOneByte);
  QCOMPARE(sentChars, totalBytes);     // the hex monitor's view is complete
  QCOMPARE(observed, expected);

  // ... and every byte really reaches the peer, unchanged and in order.
  QByteArray atPeer;
  QTRY_VERIFY_WITH_TIMEOUT(readIntoFromFd(masterFd, &atPeer, totalBytes) == totalBytes, 20000);
  QCOMPARE(atPeer, expected);
}

QTEST_MAIN(TstTransport)

#include "tst_transport.moc"
