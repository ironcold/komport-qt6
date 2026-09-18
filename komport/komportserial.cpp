/***************************************************************************
                          komportserial.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : Mon Feb 17 2003
    copyright            : (C) 2003 by Mike Sharkey
    email                : michael@sharkey.servebeer.com
    ported to Qt6/QSerialPort : 2026
    session/transport contract (M8, ADR-002/ADR-003) : 2026
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

#include <QDebug>
#include <QElapsedTimer>
#include <cstring>

namespace {

/** The process-wide monotonic clock (ADR-005): one instance for the lifetime of
  * the process, so observations stay comparable across transport activations.
  * The session origin reference that turns these values into session time is
  * recorded by SessionController, not here. */
qint64 monotonicNowNs()
{
  static const QElapsedTimer clock = [] {
    QElapsedTimer timer;
    timer.start();
    return timer;
  }();
  return clock.nsecsElapsed();
}

// Read-back mapping in the same representation the settings dialog and
// QSettings use, so effective values can be compared with requested ones.

QString dataBitsToString(QSerialPort::DataBits _bits)
{
  switch (_bits) {
    case QSerialPort::Data5: return QStringLiteral("5");
    case QSerialPort::Data6: return QStringLiteral("6");
    case QSerialPort::Data7: return QStringLiteral("7");
    default:
    case QSerialPort::Data8: return QStringLiteral("8");
  }
}

QString stopBitsToString(QSerialPort::StopBits _bits)
{
  switch (_bits) {
    case QSerialPort::TwoStop: return QStringLiteral("2");
    case QSerialPort::OneAndHalfStop: return QStringLiteral("1.5");
    default:
    case QSerialPort::OneStop: return QStringLiteral("1");
  }
}

QString parityToString(QSerialPort::Parity _parity)
{
  switch (_parity) {
    case QSerialPort::EvenParity: return QStringLiteral("EVEN");
    case QSerialPort::OddParity: return QStringLiteral("ODD");
    default:
    case QSerialPort::NoParity: return QStringLiteral("NONE");
  }
}

QString flowControlToString(QSerialPort::FlowControl _flow)
{
  switch (_flow) {
    case QSerialPort::SoftwareControl: return QStringLiteral("XON/XOFF");
    case QSerialPort::HardwareControl: return QStringLiteral("RTS/CTS");
    default:
    case QSerialPort::NoFlowControl: return QStringLiteral("NONE");
  }
}

/** Error metadata for a transportError observation: at least `kind`, a stable
  * machine-readable `code` and a human-readable `message` (SPEC-M8 section 9).
  * Never credentials, never decoded payload. */
QJsonObject errorMetadata(const QString &_kind, const QString &_code, const QString &_message)
{
  QJsonObject metadata;
  metadata.insert(QStringLiteral("kind"), _kind);
  metadata.insert(QStringLiteral("code"), _code);
  metadata.insert(QStringLiteral("message"), _message);
  return metadata;
}

} // namespace

KomportSerial::KomportSerial(QObject *parent)
: ITransport(parent)
, mDeviceName(QStringLiteral("/dev/ttyS0"))
, mBaudRate(9600)
, mStrStartBits(QStringLiteral("1"))
, mStrDataBits(QStringLiteral("8"))
, mStrStopBits(QStringLiteral("1"))
, mStrParity(QStringLiteral("NONE"))
, mStrFlowControl(QStringLiteral("NONE"))
, mFlushRate(250)
, mRxQueueMax(1024)
, mNextActivationId(0)
, mCurrentActivationId(0)
, mFailedAttemptReported(0)
, mConfigurationTransactionInProgress(false)
, mTransactionPortErrorRaised(false)
{
  QObject::connect(&mPort, &QSerialPort::readyRead, this, &KomportSerial::slotDataAvailable);
  QObject::connect(&mPort, &QSerialPort::errorOccurred, this, &KomportSerial::slotPortError);
  // M8 (SPEC-M8 section 3/6.2): the former self-connection
  // settingsChanged -> slotSettingsChanged is gone on purpose. It was the source
  // of the duplicate hardware application that every open() performed, and it
  // made settingsChanged() a hidden hardware path. The signal is now a pure
  // notification for existing consumers.
  QObject::connect(&mFlushTimer, &QTimer::timeout, this, &KomportSerial::slotFlushRxBuffer);
  mFlushTimer.start(mFlushRate);

  // The stored configuration mirrors the constructor defaults above, so the
  // first open-and-configure transaction has a complete request to apply.
  mRequested.endpoint = mDeviceName;
  mRequested.baudRate = QString::number(mBaudRate);
  mRequested.dataBits = mStrDataBits;
  mRequested.stopBits = mStrStopBits;
  mRequested.parity = mStrParity;
  mRequested.flowControl = mStrFlowControl;
  mRequested.rxQueue = mRxQueueMax;
  mRequested.flushRate = mFlushRate;
  mRequested.startBits = mStrStartBits;
  mEffective = mRequested;
}

KomportSerial::~KomportSerial(){
  // Destruction must not emit events and must not call into the transport
  // through an observation path (ADR-003): close the port directly instead of
  // going through close(), which would emit `closed`.
  mCurrentActivationId = 0;
  if ( mPort.isOpen() )
    mPort.close();
}

/** Set the name of the serial port device (e.g. /dev/ttyUSB0). */
void KomportSerial::setDeviceName(const QString &_dn){
  const bool changed = ( _dn != mRequested.endpoint );
  mDeviceName = _dn;
  mRequested.endpoint = _dn;
  if ( changed && isOpen() ) {
    // A device change while open is an activation change (SPEC-M8 6.2): open()
    // closes the old activation, opens the new one and applies the stored
    // configuration exactly once as its open-and-configure transaction.
    open();
  }
}

/** Return the name of the serial port device (e.g. /dev/ttyUSB0).
 */
QString KomportSerial::deviceName() const {
  return mDeviceName;
}

/** Open the serial port for communication. */
bool KomportSerial::open(){
  // An already-open port is a real activation boundary; the pre-M8 behaviour of
  // closing it first is kept unchanged (SPEC-M8 6.2, row "open() while open").
  if ( mPort.isOpen() )
    close();

  // Every attempt consumes an activation id, including a failing one (ADR-003).
  // The guard is armed *before* the attempt: QSerialPort::open() reports a
  // failure through errorOccurred() synchronously on some platforms and
  // asynchronously on others, and in both orders exactly one error observation
  // must result from this attempt.
  const quint64 attempt = ++mNextActivationId;
  mFailedAttemptReported = attempt;
  mPort.setPortName( mRequested.endpoint );
  const bool ok = mPort.open( QIODevice::ReadWrite );
  if ( !ok ) {
    // The transport knows its own attempt and reports the failure exactly once.
    // The asynchronous QSerialPort error of this same attempt is suppressed in
    // slotPortError() by the in-progress/result guard, not by text matching.
    const QString reason = mPort.errorString();
    emit transportError( attempt, monotonicNowNs(),
                         errorMetadata( QStringLiteral("open"), QStringLiteral("open_failed"), reason ) );
    // The legacy notification is emitted here, synchronously, instead of relying on
    // when QSerialPort delivers errorOccurred() for this attempt: the application
    // reads its mSerialErrorPending flag directly after applyConfiguration()/open()
    // returned, so the notification has to have happened by then. slotPortError()
    // skips its own emission for this attempt, which keeps the pre-M8 behaviour of
    // exactly one settingsFailed() per failed open on every platform.
    emit settingsFailed( reason );
    emit settingsChanged();
    return false;
  }

  mCurrentActivationId = attempt;
  mFailedAttemptReported = 0;
  QJsonObject openMetadata;
  openMetadata.insert( QStringLiteral("endpoint"), mRequested.endpoint );
  emit opened( attempt, monotonicNowNs(), openMetadata );

  // Open-and-configure: exactly one transaction, its result immediately after
  // `opened` (SPEC-M8 6.2). A new activation always applies the hardware
  // settings once, even when they equal the last effective snapshot, because the
  // port was just reopened without settings.
  mLastResult = applyConfigurationInternal( mRequested, true );
  emitConfigurationObservations( mLastResult );

  emit settingsChanged();
  return isOpen();
}

/** Close the port. */
void KomportSerial::close(){
  if ( mPort.isOpen() ) {
    mPort.close();
  }
  if ( mCurrentActivationId != 0 ) {
    const quint64 ended = mCurrentActivationId;
    mCurrentActivationId = 0;
    // Deliberately unchanged legacy behaviour (SPEC-M8 section 8): the RX
    // buffer is NOT cleared and the flush timer keeps running, so bytes received
    // before the close can still be rendered after a reopen. Those delayed
    // characters are display-path only; the event path is kept correct by the
    // controller's activation filter, not by touching this behaviour.
    QJsonObject metadata;
    metadata.insert( QStringLiteral("endpoint"), mRequested.endpoint );
    emit closed( ended, monotonicNowNs(), metadata );
  }
}

/** Is the serial port open? */
bool KomportSerial::isOpen() const {
  return mPort.isOpen();
}

/** Set the baud rate. */
void KomportSerial::setBaudRate(qint32 _baud){
  mBaudRate = _baud;
  mRequested.baudRate = QString::number( _baud );
  // Legacy setter row of SPEC-M8 6.2: store, apply once through the transaction
  // routine when the port is open, then emit the compatibility signal.
  applyConfiguration( mRequested );
  emit settingsChanged();
}

/** Set the baud rate, parsed from a string. */
void KomportSerial::setBaudRate(const QString &_baud){
  bool ok = false;
  qint32 baud = _baud.toInt(&ok);
  if ( ok && baud > 0 ) {
    setBaudRate(baud);
  }
}

/** Return the current baud rate setting. */
qint32 KomportSerial::baudRate() const {
  return mBaudRate;
}

/** Legacy compatibility slot: applies the stored configuration once when the
  *  port is open. It is deliberately no longer connected to settingsChanged. */
void KomportSerial::slotSettingsChanged(){
  if ( isOpen() ) {
    applyConfiguration( mRequested );
  }
}

/** Legacy helper: applies the stored configuration through the single
  *  transaction path (kept for compatibility with the pre-M8 helper). */
void KomportSerial::applyPortSettings(){
  if ( isOpen() )
    applyConfiguration( mRequested );
}

/** Apply the requested hardware settings to the port, field by field.
  *
  * The same string-to-setting mapping as the pre-M8 applyPortSettings() is used
  * verbatim (including "data bits unknown -> Data8" and the parity/flow-control
  * fallbacks), so no setting's meaning changes. An unparsable or non-positive
  * baud rate is ignored exactly like setBaudRate(const QString&) does, so a
  * hand-edited profile cannot alter it.
  */
bool KomportSerial::applyHardwareSettings(const TransportConfiguration &_requested,
                                          const TransportConfiguration &_previous,
                                          TransportConfiguration *_effective)
{
  TransportConfiguration effective = _previous;
  bool allAccepted = true;
  // The device of the effective state is the endpoint the transport is using.
  // QSerialPort::portName() cannot be used for that: it reports the system's
  // short form ("pts/6" for "/dev/pts/6"), while the configuration - and every
  // consumer of it - carries the device path the port was opened with.
  effective.endpoint = _requested.endpoint;

  // Every hardware field is read back from the port *after* the attempt, so the
  // reported effective values are what the device really has: a rate the kernel
  // rounded, or a setting the port refused (it then keeps its previous value),
  // appears here truthfully instead of echoing the request.
  bool baudParsed = false;
  const qint32 requestedBaud = _requested.baudRate.toInt( &baudParsed );
  if ( !baudParsed || requestedBaud <= 0 ) {
    // Defensive: the transaction normalizes an unusable baud rate beforehand.
    effective.baudRate = _previous.baudRate;
  } else {
    if ( !mPort.setBaudRate( requestedBaud ) )
      allAccepted = false;
    effective.baudRate = QString::number( mPort.baudRate() );
  }

  QSerialPort::DataBits dataBits = QSerialPort::Data8;
  switch ( _requested.dataBits.toInt() ) {
    case 5: dataBits = QSerialPort::Data5; break;
    case 6: dataBits = QSerialPort::Data6; break;
    case 7: dataBits = QSerialPort::Data7; break;
    default:
    case 8: dataBits = QSerialPort::Data8; break;
  }
  if ( !mPort.setDataBits( dataBits ) )
    allAccepted = false;
  effective.dataBits = dataBitsToString( mPort.dataBits() );

  QSerialPort::StopBits stopBits = QSerialPort::OneStop;
  if ( _requested.stopBits == QLatin1String("2") ) {
    stopBits = QSerialPort::TwoStop;
  } else if ( _requested.stopBits == QLatin1String("1.5") ) {
    stopBits = QSerialPort::OneAndHalfStop;
  } else {
    stopBits = QSerialPort::OneStop;
  }
  if ( !mPort.setStopBits( stopBits ) )
    allAccepted = false;
  effective.stopBits = stopBitsToString( mPort.stopBits() );

  QSerialPort::Parity parity = QSerialPort::NoParity;
  if ( _requested.parity.compare( QLatin1String("EVEN"), Qt::CaseInsensitive ) == 0 ) {
    parity = QSerialPort::EvenParity;
  } else if ( _requested.parity.compare( QLatin1String("ODD"), Qt::CaseInsensitive ) == 0 ) {
    parity = QSerialPort::OddParity;
  } else {
    parity = QSerialPort::NoParity;
  }
  if ( !mPort.setParity( parity ) )
    allAccepted = false;
  effective.parity = parityToString( mPort.parity() );

  QSerialPort::FlowControl flow = QSerialPort::NoFlowControl;
  if ( _requested.flowControl.compare( QLatin1String("XON/XOFF"), Qt::CaseInsensitive ) == 0 ) {
    flow = QSerialPort::SoftwareControl;
  } else if ( _requested.flowControl.compare( QLatin1String("RTS/CTS"), Qt::CaseInsensitive ) == 0 ) {
    flow = QSerialPort::HardwareControl;
  } else {
    flow = QSerialPort::NoFlowControl;
  }
  if ( !mPort.setFlowControl( flow ) )
    allAccepted = false;
  effective.flowControl = flowControlToString( mPort.flowControl() );

  // Note: start bits is intentionally not applied - a UART always transmits a
  // single start bit, QSerialPort exposes no such setting, and the original
  // termios-based implementation never actually used it either. It is kept only
  // so the settings dialog/config file keep their existing "Start bits" field
  // (reported in the transaction metadata as the `compatibility` group).
  effective.startBits = _requested.startBits;

  if ( _effective != nullptr )
    *_effective = effective;
  return allAccepted;
}

/** Single point where bytes reach the port (test seam, SPEC-M8 section 13). */
qint64 KomportSerial::writeToPort(const char *_data, qint64 _len)
{
  return mPort.write( _data, _len );
}

/** Single point where bytes are read from the port (test seam). */
QByteArray KomportSerial::readFromPort()
{
  return mPort.readAll();
}

/** The single observed write primitive (ADR-003).
  *
  * Every byte-transmitting entry point routes through here, and this is the only
  * place that creates a TX observation. The observation covers exactly the bytes
  * the transport accepted, so a partially accepted write reports its accepted
  * prefix and the session stream never loses bytes that did reach the transport
  * (the same bytes the legacy sentChar()/hex-monitor path reports).
  *
  * @return the accepted byte count, or a negative value when the write was
  *         refused entirely
  */
qint64 KomportSerial::writeRaw(const char *_data, qsizetype _len)
{
  if ( _data == nullptr || _len < 0 || !isOpen() )
    return -1;

  const qint64 accepted = writeToPort( _data, static_cast<qint64>(_len) );
  if ( accepted > 0 ) {
    emit bytesWritten( observingActivationId(),
                       QByteArray( _data, static_cast<qsizetype>(accepted) ),
                       monotonicNowNs() );
    for ( qint64 i = 0; i < accepted; ++i ) {
      emit sentChar( _data[i] );
    }
  }

  if ( accepted != static_cast<qint64>(_len) ) {
    // A write() returning less than the full length (a partial write, or -1 on
    // error - accepted defaults to -1 there, so the loop above simply didn't
    // run) used to be silently swallowed: the untransmitted remainder was
    // dropped with no retry and no way for any caller to even find out. This
    // covers macro commands/line endings, keyboard escape sequences and the
    // device-status-report/device-attributes replies the emulation sends back -
    // none of those retry on their own, so at minimum this should be
    // diagnosable. M8 additionally turns it into an observable Error event.
    const QString reason = mPort.errorString();
    qWarning() << "KomportSerial::writeRaw(): wrote" << accepted << "of" << _len
               << "bytes (" << reason << ")";
    QJsonObject metadata = errorMetadata( QStringLiteral("write"),
                                         QStringLiteral("short_write"), reason );
    const qint64 acceptedNonNegative = qMax<qint64>( accepted, 0 );
    metadata.insert( QStringLiteral("acceptedBytes"),
                     static_cast<double>(acceptedNonNegative) );
    metadata.insert( QStringLiteral("refusedBytes"),
                     static_cast<double>(static_cast<qint64>(_len) - acceptedNonNegative) );
    emit transportError( observingActivationId(), monotonicNowNs(), metadata );
  }

  return accepted;
}

/** Send bytes (ITransport). */
qint64 KomportSerial::writeBytes(const QByteArray &bytes)
{
  if ( !isOpen() )
    return -1;
  if ( bytes.isEmpty() )
    return 0;
  return writeRaw( bytes.constData(), bytes.size() );
}

/** put a character */
bool KomportSerial::putChar(char _ch){
  if ( !isOpen() )
    return false;
  return writeRaw( &_ch, 1 ) == 1;
}

/** transmit a null-terminated string - delegates to the length-aware
 *  overload below via strlen(); see the header for why a caller that
 *  needs embedded NULs sent verbatim must call that overload directly
 *  instead. */
bool KomportSerial::putStr(const char* str){
  if ( str == nullptr ) return false;
  return putStr( str, static_cast<qsizetype>( strlen(str) ) );
}

/** transmit exactly _len bytes, verbatim (see header).
  *
  * Codex review finding (round 4): _len is signed (qsizetype) but wasn't
  * rejected when negative. For _len == -1, QIODevice::write() returns its own
  * -1 error sentinel for a rejected/failed write - written(-1) != len(-1) was
  * then false, so this returned true ("success") despite transmitting nothing.
  * Not reachable from the current caller (KomportApp::slotMacroTriggered()
  * always passes a real QByteArray::size()), but the public overload itself must
  * not accept a negative length as if it meant something.
  */
bool KomportSerial::putStr(const char* _str, qsizetype _len){
  if ( _str == nullptr || !isOpen() || _len < 0 ) return false;
  return writeRaw( _str, _len ) == _len;
}

/** set size of the internal RX buffer high-water mark */
int KomportSerial::setRxQueue(int _i){
  // A high-water mark of 0 or less makes slotDataAvailable()'s overflow
  // trim ("remove everything past mRxQueueMax bytes") discard every byte
  // that arrives - silent, total data loss. The settings dialog's spinbox
  // only allows 1024..32768, but a hand-edited profile in QSettings isn't
  // range-checked at all before reaching here; clamp to a sane minimum so
  // this setter is safe regardless of where the value came from.
  TransportConfiguration requested = mRequested;
  requested.rxQueue = _i;
  applyConfiguration( requested );
  return mRxQueueMax;
}

/** QSerialPort has data available */
void KomportSerial::slotDataAvailable(){
  const QByteArray chunk = readFromPort();
  // The RX observation is emitted before the legacy buffering/character path
  // (SPEC-M8 6.2) and is therefore independent of RxQueue and FlushRate: the
  // event stream is loss-free where the display path is deliberately lossy.
  // Without a live activation (a queued readyRead arriving after close()) the
  // legacy path still runs, but no observation with the id 0 is produced.
  if ( !chunk.isEmpty() && hasLiveActivation() )
    emit bytesReceived( mCurrentActivationId, chunk, monotonicNowNs() );
  mRxBuffer += chunk;
  if ( mRxBuffer.size() > mRxQueueMax ) {
    // drop the oldest overflow rather than growing unbounded - mirrors the
    // original round-queue's overflow behaviour of overwriting old data.
    mRxBuffer.remove( 0, mRxBuffer.size() - mRxQueueMax );
  }
}

/** QSerialPort reported an error */
void KomportSerial::slotPortError(QSerialPort::SerialPortError error){
  if ( error == QSerialPort::NoError )
    return;

  qWarning() << "KomportSerial:" << mPort.errorString();

  // The legacy user-facing path keeps its pre-M8 shape: exactly one
  // settingsFailed() per reported error. A failed open emits it synchronously in
  // open() itself (so the application can read its flag right after the call
  // returned), and this slot then only reports errors it owns - a runtime error of
  // a live port, or an error of an open attempt that the transport has already
  // reported.
  const bool failedOpenAlreadyReported = ( mFailedAttemptReported != 0 && !mPort.isOpen() );
  if ( !failedOpenAlreadyReported )
    emit settingsFailed( mPort.errorString() );

  // The event path reports a failure once: the asynchronous error of an open
  // attempt whose result the transport already reported is suppressed by the
  // in-progress/result guard of that attempt (ADR-003), never by comparing text.
  if ( failedOpenAlreadyReported ) {
    mFailedAttemptReported = 0;
    return;
  }

  // A port error raised while a configuration transaction is being applied is
  // recorded for that transaction's result instead of becoming an event of its
  // own: the transaction stays within the two observations SPEC-M8 6.2 allows and
  // the failure still reaches the session record as its apply error (SPEC-M8
  // section 9). The legacy settingsFailed() above is emitted either way.
  if ( mConfigurationTransactionInProgress ) {
    mTransactionPortErrorRaised = true;
    if ( mTransactionPortErrorMessage.isEmpty() )
      mTransactionPortErrorMessage = mPort.errorString();
    return;
  }

  // A runtime error is an event only while an activation is live; the legacy
  // settingsFailed() above is emitted either way.
  if ( hasLiveActivation() ) {
    emit transportError( mCurrentActivationId, monotonicNowNs(),
                         errorMetadata( QStringLiteral("runtime"),
                                        QStringLiteral("port_error"),
                                        mPort.errorString() ) );
  }
}

/** periodic flush of mRxBuffer into receivedChar() signals */
void KomportSerial::slotFlushRxBuffer(){
  if ( mRxBuffer.isEmpty() ) return;
  // Snapshot-and-clear rather than removing the front byte one at a time:
  // QByteArray::remove(0, 1) shifts every remaining byte down by one
  // position on every call, so a loop of them is O(n^2) for an n-byte
  // buffer - expensive on bursts at high baud rates, and it's competing
  // directly with the GUI/emulation for CPU time right when there's the
  // most data to process. QByteArray is implicitly shared, so this copy is
  // just a refcount bump, not a deep copy.
  const QByteArray data = mRxBuffer;
  mRxBuffer.clear();
  for ( const char ch : data ) {
    emit receivedChar(ch);
  }
}

/** set the rate at which the Rx buffer is flushed */
void KomportSerial::setFlushRate(int _i){
  // qMax(1, ...), not qMax(0, ...): 0 is a legal QTimer interval, but not
  // a *sane* one here - QTimer::start(0) re-fires on every single trip
  // through the event loop for as long as the timer is active, which is
  // effectively a busy-poll. slotFlushRxBuffer() early-returns when
  // mRxBuffer is empty, but the timer still dispatches (and this object
  // starts one in its constructor and keeps it running for its whole
  // lifetime) - a real idle-CPU/responsiveness cost, not just a
  // theoretical one. The settings dialog's spinbox previously allowed 0
  // directly (not just a hand-edited profile), so this was reachable from
  // the ordinary UI.
  TransportConfiguration requested = mRequested;
  requested.flushRate = _i;
  applyConfiguration( requested );
}

/** set the character framing */
void KomportSerial::setFraming( const QString &_startbits, const QString &_databits,
                                 const QString &_stopbits, const QString &_parity ){
  TransportConfiguration requested = mRequested;
  requested.startBits = _startbits;
  requested.dataBits = _databits;
  requested.stopBits = _stopbits;
  requested.parity = _parity;
  // Pre-M8 this applied directly and emitted no notification; the application
  // still happens exactly once, now through the single transaction path (which
  // is what makes it observable).
  applyConfiguration( requested );
}

/** set flow control */
void KomportSerial::setFlowControl( const QString &_flowControl ){
  TransportConfiguration requested = mRequested;
  requested.flowControl = _flowControl;
  applyConfiguration( requested );
}

/** The single configuration entry point (SPEC-M8 6.2). */
ConfigurationResult KomportSerial::applyConfiguration(const TransportConfiguration &_requested)
{
  // A changed endpoint on a live port is an activation change (SPEC-M8 6.2,
  // operation table row "configure(request), port open, endpoint changed"): the
  // old activation is closed, the new endpoint is opened, and the request is
  // applied as that activation's single open-and-configure transaction. Without
  // this the entry point could report a successful configuration for a port it
  // never opened.
  if ( isOpen() && _requested.endpoint != mEffective.endpoint ) {
    mRequested = _requested;
    storeRequestedValues( _requested );
    if ( open() )
      return mLastResult;

    // The new endpoint could not be opened. open() already reported that failure
    // exactly once as Error(kind "open"), and nothing was applied. This is NOT
    // the closed-port store-only case: the operation started on a live port and
    // consumed an open attempt, so it returns a failed result and emits no
    // configuration observation of its own (a second report of the same failure
    // would exceed the two-observation budget of SPEC-M8 6.2).
    ConfigurationResult failed;
    failed.requested = _requested;
    failed.applyStatus = ConfigurationApplyStatus::Failed;
    failed.message = tr("Could not open the requested endpoint \"%1\".").arg( _requested.endpoint );
    return failed;
  }

  const ConfigurationResult result = applyConfigurationInternal( _requested, false );
  // Emits nothing for a closed port (storedOnly) and nothing for the documented
  // no-op; one or two observations otherwise.
  emitConfigurationObservations( result );
  return result;
}

/** Apply a requested configuration; see the header for the two modes. */
ConfigurationResult KomportSerial::applyConfigurationInternal(const TransportConfiguration &_requested,
                                                             bool _forceHardwareApply)
{
  TransportConfiguration requested = _requested;
  // Local Buffering is clamped here, so every path (legacy setters included)
  // behaves exactly like setRxQueue()/setFlushRate() did.
  requested.rxQueue = qMax( 1, requested.rxQueue );
  requested.flushRate = qMax( 1, requested.flushRate );

  // A baud rate that is not a positive integer cannot be applied - the same
  // validation the legacy setBaudRate(const QString&) performs. It is dropped
  // from the stored/applied state (a hand-edited profile must not leave the
  // request permanently unsatisfiable) but reported as not applied, so no caller
  // ever learns "full" for a value the port never received.
  bool baudParsed = false;
  const qint32 parsedBaud = _requested.baudRate.toInt( &baudParsed );
  const bool baudUsable = baudParsed && parsedBaud > 0;
  const bool baudRejected = !baudUsable && !_requested.baudRate.isEmpty();
  if ( !baudUsable )
    requested.baudRate = mEffective.baudRate;

  const TransportConfiguration previous = mEffective;
  ConfigurationResult result;
  result.requested = _requested;   // verbatim: what the caller asked for
  result.applyStatus = ConfigurationApplyStatus::Full;

  // What the request *asks* to change. This decides whether there is anything to
  // do at all; the groups reported in the metadata are computed after the
  // application, because a rejected field changes nothing (SPEC-M8 6.2: "failed
  // with a changed effective state" versus "failed with no change").
  const QStringList requestedGroups = requested.changedGroupsComparedTo( previous );
  const bool hardwareRequested = requestedGroups.contains(
      QLatin1String(TransportConfigurationGroup::kHardware) );

  if ( !isOpen() ) {
    // Closed port: store only, no hardware application and no observation; the
    // next open() performs the open-and-configure transaction.
    mRequested = requested;
    storeRequestedValues( requested );
    result.storedOnly = true;
    // The buffering and compatibility values take effect immediately even while
    // the port is closed (they are Komport-side), so the "in force" snapshot
    // carries them; the hardware values stay as they are until the next open.
    mEffective.rxQueue = requested.rxQueue;
    mEffective.flushRate = requested.flushRate;
    mEffective.startBits = requested.startBits;
    result.effective = QJsonObject();
    return result;
  }

  // The no-op also requires the whole request to be usable: a dropped baud rate
  // has to be reported, otherwise the caller would see "full" for a request the
  // port never received.
  const bool nothingToDo = !_forceHardwareApply && requestedGroups.isEmpty() && !baudRejected;
  if ( nothingToDo ) {
    // The documented no-op: requested values equal the effective ones, nothing
    // is applied and nothing is emitted.
    result.effective = readBackHardwareJson();
    return result;
  }

  // Local buffering and the compatibility field take effect immediately and
  // cannot be refused by the port; only the hardware group can come back partial.
  mRequested = requested;
  storeRequestedValues( requested );

  // The effective hardware state after the attempt, as the port reports it. The
  // out-parameter of the seam is a read-back state, never the bare request: a
  // platform that normalizes a setting (the kernel rounds a non-standard baud
  // rate) or refuses it must not be recorded as having accepted it.
  //
  // A port error raised during the application is consumed after the changed
  // groups are known, so both are declared here.
  bool portErrorRaised = false;
  QString portErrorMessage;

  if ( hardwareRequested || _forceHardwareApply ) {
    // A port error raised while the settings are being applied is recorded for
    // this transaction (slotPortError()) instead of being emitted as a runtime
    // event, so an apply that makes QSerialPort raise errorOccurred() cannot push
    // the transaction past the two observations SPEC-M8 6.2 allows. The window is
    // *restored* rather than cleared, because a legacy receiver may run another
    // (nested) transaction from settingsFailed() while this one is being applied.
    const bool previousTransactionWindow = mConfigurationTransactionInProgress;
    mConfigurationTransactionInProgress = true;
    TransportConfiguration effective;
    const bool allAccepted = applyHardwareSettings( requested, previous, &effective );
    mConfigurationTransactionInProgress = previousTransactionWindow;
    const bool portErrorWasRaised = mTransactionPortErrorRaised;
    portErrorRaised = portErrorWasRaised;
    portErrorMessage = mTransactionPortErrorMessage;
    mTransactionPortErrorRaised = false;
    mTransactionPortErrorMessage.clear();
    mEffective = effective;
    if ( !allAccepted ) {
      // Partial when the port took part of the request, failed when it took
      // none of it (which also means nothing changed).
      const bool anyAccepted = !effective.hardwareEquals( previous );
      result.applyStatus = anyAccepted ? ConfigurationApplyStatus::Partial
                                       : ConfigurationApplyStatus::Failed;
      result.message = tr("Could not apply the requested port settings "
                          "(baud rate/data bits/parity/flow control) - "
                          "the device may not support this combination.");
    }
  }

  // Local buffering and the compatibility field take effect immediately and
  // cannot be refused by the port; the endpoint stays the device path the
  // transport is using (a live endpoint change reopens through the entry point).
  mEffective.rxQueue = requested.rxQueue;
  mEffective.flushRate = requested.flushRate;
  mEffective.startBits = requested.startBits;
  mEffective.endpoint = mRequested.endpoint;

  // Groups that actually changed as a result of this transaction, in the fixed
  // order hardware, localBuffering, compatibility. The hardware group is defined
  // by the effective state (a rejected field changes nothing); an open always
  // applies the hardware settings, so its activation reports the group even when
  // the values equal the previous snapshot. Buffering and compatibility take
  // effect immediately and change exactly when the request differs.
  if ( _forceHardwareApply || !mEffective.hardwareEquals( previous ) )
    result.changedGroups.append( QLatin1String(TransportConfigurationGroup::kHardware) );
  if ( !requested.bufferingEquals( previous ) )
    result.changedGroups.append( QLatin1String(TransportConfigurationGroup::kLocalBuffering) );
  if ( !requested.compatibilityEquals( previous ) )
    result.changedGroups.append( QLatin1String(TransportConfigurationGroup::kCompatibility) );

  // A port error raised while the settings were applied is part of this
  // transaction's outcome (SPEC-M8 section 9) - it is recorded here as the
  // transaction's own apply error rather than as a second runtime event, so the
  // two-observation maximum holds.
  if ( portErrorRaised ) {
    const QString portErrorNote = portErrorMessage.isEmpty()
        ? tr("The port reported an error while the settings were applied.")
        : tr("The port reported an error while the settings were applied: %1")
              .arg( portErrorMessage );
    result.message = result.message.isEmpty()
        ? portErrorNote
        : result.message + QLatin1Char(' ') + portErrorNote;
    // A port error means the whole *attempt* failed, not merely individual fields:
    // the transaction is `failed` even when some settings went through before the
    // error. That is the "failed with a changed effective state" row of SPEC-M8
    // 6.2, which is mapped like `partial` (one configurationChanged, then one
    // Error); with no change at all it is the "failed with no change" row, which
    // yields the Error alone.
    result.applyStatus = ConfigurationApplyStatus::Failed;
  }

  // A dropped baud rate is reported rather than silently ignored: the caller
  // asked for something the port never received, so the transaction cannot claim
  // to be full.
  if ( baudRejected ) {
    const QString rejectedBaud = tr("The requested baud rate \"%1\" is not usable "
                                    "and was not applied.").arg( _requested.baudRate );
    result.message = result.message.isEmpty() ? rejectedBaud
                                              : result.message + QLatin1Char(' ') + rejectedBaud;
    if ( result.applyStatus == ConfigurationApplyStatus::Full ) {
      result.applyStatus = result.changedGroups.isEmpty() ? ConfigurationApplyStatus::Failed
                                                          : ConfigurationApplyStatus::Partial;
    }
  }

  // Exactly one legacy user-facing message per failed transaction, unchanged.
  if ( result.applyStatus != ConfigurationApplyStatus::Full )
    emit settingsFailed( result.message );

  result.effective = readBackHardwareJson();
  return result;
}

/** Mirror the stored configuration into the legacy member fields the getters and
  *  the flush timer use; the configuration value itself stays authoritative. */
void KomportSerial::storeRequestedValues(const TransportConfiguration &_requested)
{
  if ( _requested.flushRate != mFlushRate ) {
    mFlushRate = _requested.flushRate;
    mFlushTimer.start( mFlushRate );
  }
  mRxQueueMax = _requested.rxQueue;
  mDeviceName = _requested.endpoint;
  const qint32 baud = _requested.baudRate.toInt();
  if ( baud > 0 )
    mBaudRate = baud;   // the legacy getter's value is never a dropped 0
  mStrStartBits = _requested.startBits;
  mStrDataBits = _requested.dataBits;
  mStrStopBits = _requested.stopBits;
  mStrParity = _requested.parity;
  mStrFlowControl = _requested.flowControl;
}

/** Emit the observations of one transaction result (SPEC-M8 6.2). */
void KomportSerial::emitConfigurationObservations(const ConfigurationResult &_result)
{
  // Stored-only transactions and transactions without a live activation emit
  // nothing: an observation never carries the id 0 (ADR-003).
  if ( _result.storedOnly || !hasLiveActivation() )
    return;

  const bool anyChange = !_result.changedGroups.isEmpty();
  const QJsonObject metadata = _result.toMetadata();

  // Rows of the observable mapping: full/partial with a changed effective state
  // report configurationChanged; a failed application that changed nothing has
  // no configuration to report and only surfaces as an Error.
  const bool failedWithoutChange = ( _result.applyStatus == ConfigurationApplyStatus::Failed )
      && !anyChange;
  if ( anyChange && !failedWithoutChange ) {
    emit configurationChanged( observingActivationId(), monotonicNowNs(), metadata );
  }

  if ( _result.applyStatus != ConfigurationApplyStatus::Full ) {
    QJsonObject errorMetadataObject = metadata;
    errorMetadataObject.insert( QStringLiteral("kind"), QStringLiteral("apply") );
    errorMetadataObject.insert( QStringLiteral("code"),
                                _result.applyStatus == ConfigurationApplyStatus::Partial
                                    ? QStringLiteral("apply_partial")
                                    : QStringLiteral("apply_failed") );
    emit transportError( observingActivationId(), monotonicNowNs(), errorMetadataObject );
  }
}

/** The configuration as the port reports it (the specified read-back state). */
TransportConfiguration KomportSerial::readBackEffectiveConfiguration() const
{
  TransportConfiguration effective = mEffective;   // buffering/compatibility in force
  if ( !isOpen() )
    return effective;

  // Endpoint: the device path the transport is using. QSerialPort::portName()
  // reports the system's short form instead ("pts/6" for "/dev/pts/6"), while
  // the configuration - and everything that stores or displays it - carries the
  // device path, so that is the value the read-back state must keep.
  effective.endpoint = mRequested.endpoint;
  effective.baudRate = QString::number( mPort.baudRate() );
  effective.dataBits = dataBitsToString( mPort.dataBits() );
  effective.stopBits = stopBitsToString( mPort.stopBits() );
  effective.parity = parityToString( mPort.parity() );
  effective.flowControl = flowControlToString( mPort.flowControl() );
  return effective;
}

/** Metadata of the current hardware settings as the port reports them. The keys
  *  are the TransportConfiguration field names, so the device appears as
  *  `endpoint` - the port actually used - not as a separate "portName" key. */
QJsonObject KomportSerial::readBackHardwareJson() const
{
  const TransportConfiguration effective = readBackEffectiveConfiguration();
  QJsonObject json;
  json.insert( QStringLiteral("endpoint"), effective.endpoint );
  json.insert( QStringLiteral("baudRate"), effective.baudRate );
  json.insert( QStringLiteral("dataBits"), effective.dataBits );
  json.insert( QStringLiteral("stopBits"), effective.stopBits );
  json.insert( QStringLiteral("parity"), effective.parity );
  json.insert( QStringLiteral("flowControl"), effective.flowControl );
  return json;
}
