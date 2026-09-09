/***************************************************************************
                          komportserial.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : Mon Feb 17 2003
    copyright            : (C) 2003 by Mike Sharkey
    email                : michael@sharkey.servebeer.com
    ported to Qt6/QSerialPort : 2026
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
#include <cstring>

KomportSerial::KomportSerial(QObject *parent)
: QObject(parent)
, mDeviceName(QStringLiteral("/dev/ttyS0"))
, mBaudRate(9600)
, mStrStartBits(QStringLiteral("1"))
, mStrDataBits(QStringLiteral("8"))
, mStrStopBits(QStringLiteral("1"))
, mStrParity(QStringLiteral("NONE"))
, mStrFlowControl(QStringLiteral("NONE"))
, mFlushRate(250)
, mRxQueueMax(1024)
{
  QObject::connect(&mPort, &QSerialPort::readyRead, this, &KomportSerial::slotDataAvailable);
  QObject::connect(&mPort, &QSerialPort::errorOccurred, this, &KomportSerial::slotPortError);
  QObject::connect(this, &KomportSerial::settingsChanged, this, &KomportSerial::slotSettingsChanged);
  QObject::connect(&mFlushTimer, &QTimer::timeout, this, &KomportSerial::slotFlushRxBuffer);
  mFlushTimer.start(mFlushRate);
}

KomportSerial::~KomportSerial(){
  close();
}

/** Set the name of the serial port device (e.g. /dev/ttyUSB0). */
void KomportSerial::setDeviceName(const QString &_dn){
  bool reset = ( _dn != mDeviceName ) && isOpen();
  if ( reset )
      close();
  mDeviceName = _dn;
  if ( reset )
      open();
}

/** Return the name of the serial port device (e.g. /dev/ttyUSB0).
 */
QString KomportSerial::deviceName() const {
  return mDeviceName;
}

/** Open the serial port for communication. */
bool KomportSerial::open(){
  close();
  mPort.setPortName( mDeviceName );
  bool ok = mPort.open( QIODevice::ReadWrite );
  if ( ok ) {
    applyPortSettings();
  }
  emit settingsChanged();
  return isOpen();
}

/** Close the port. */
void KomportSerial::close(){
  if ( mPort.isOpen() ) {
    mPort.close();
  }
}

/** Is the serial port open? */
bool KomportSerial::isOpen() const {
  return mPort.isOpen();
}

/** Set the baud rate. */
void KomportSerial::setBaudRate(qint32 _baud){
  mBaudRate = _baud;
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

/** Commits settings changes to the serial port. */
void KomportSerial::slotSettingsChanged(){
  if ( isOpen() ) {
    applyPortSettings();
  }
}

/** apply the currently stored framing/parity/flow-control settings to the open port */
void KomportSerial::applyPortSettings(){
  bool ok = mPort.setBaudRate( mBaudRate );

  QSerialPort::DataBits dataBits = QSerialPort::Data8;
  switch ( mStrDataBits.toInt() ) {
    case 5: dataBits = QSerialPort::Data5; break;
    case 6: dataBits = QSerialPort::Data6; break;
    case 7: dataBits = QSerialPort::Data7; break;
    default:
    case 8: dataBits = QSerialPort::Data8; break;
  }
  ok = mPort.setDataBits( dataBits ) && ok;

  QSerialPort::StopBits stopBits = QSerialPort::OneStop;
  if ( mStrStopBits == QLatin1String("2") ) {
    stopBits = QSerialPort::TwoStop;
  } else if ( mStrStopBits == QLatin1String("1.5") ) {
    stopBits = QSerialPort::OneAndHalfStop;
  } else {
    stopBits = QSerialPort::OneStop;
  }
  ok = mPort.setStopBits( stopBits ) && ok;

  QSerialPort::Parity parity = QSerialPort::NoParity;
  if ( mStrParity.compare( QLatin1String("EVEN"), Qt::CaseInsensitive ) == 0 ) {
    parity = QSerialPort::EvenParity;
  } else if ( mStrParity.compare( QLatin1String("ODD"), Qt::CaseInsensitive ) == 0 ) {
    parity = QSerialPort::OddParity;
  } else {
    parity = QSerialPort::NoParity;
  }
  ok = mPort.setParity( parity ) && ok;

  QSerialPort::FlowControl flow = QSerialPort::NoFlowControl;
  if ( mStrFlowControl.compare( QLatin1String("XON/XOFF"), Qt::CaseInsensitive ) == 0 ) {
    flow = QSerialPort::SoftwareControl;
  } else if ( mStrFlowControl.compare( QLatin1String("RTS/CTS"), Qt::CaseInsensitive ) == 0 ) {
    flow = QSerialPort::HardwareControl;
  } else {
    flow = QSerialPort::NoFlowControl;
  }
  ok = mPort.setFlowControl( flow ) && ok;

  // Note: start bits (mStrStartBits) is intentionally not applied - a UART
  // always transmits a single start bit, QSerialPort exposes no such
  // setting, and the original termios-based implementation never actually
  // used it either. It is kept only so the settings dialog/config file keep
  // their existing "Start bits" field.

  if ( !ok ) {
    emit settingsFailed( tr("Could not apply the requested port settings "
                            "(baud rate/data bits/parity/flow control) - "
                            "the device may not support this combination.") );
  }
}

/** put a character */
bool KomportSerial::putChar(char _ch){
  if ( isOpen() && mPort.putChar( _ch ) ) {
    emit sentChar( _ch );
    return true;
  }
  return false;
}

/** transmit a null-terminated string - delegates to the length-aware
 *  overload below via strlen(); see the header for why a caller that
 *  needs embedded NULs sent verbatim must call that overload directly
 *  instead. */
bool KomportSerial::putStr(const char* str){
  if ( str == nullptr ) return false;
  return putStr( str, static_cast<qsizetype>( strlen(str) ) );
}

/** transmit exactly _len bytes, verbatim (see header) */
bool KomportSerial::putStr(const char* _str, qsizetype _len){
  // A single batched write() rather than looping putChar() per byte - one
  // syscall/QSerialPort call instead of N. sentChar() (for the hex
  // monitor) is still emitted once per byte actually written, just not
  // tangled up with how the bytes got onto the wire.
  // Codex review finding (round 4): _len is signed (qsizetype) but wasn't
  // rejected when negative. For _len == -1, QIODevice::write() returns
  // its own -1 error sentinel for a rejected/failed write - written(-1)
  // != len(-1) was then false, so this returned true ("success") despite
  // transmitting nothing. Not reachable from the current caller
  // (KomportApp::slotMacroTriggered() always passes a real
  // QByteArray::size()), but the public overload itself must not accept
  // a negative length as if it meant something.
  if ( _str == nullptr || !isOpen() || _len < 0 ) return false;
  const qint64 len = static_cast<qint64>( _len );
  const qint64 written = mPort.write( _str, len );
  for ( qint64 i = 0; i < written; ++i ) {
    emit sentChar( _str[i] );
  }
  if ( written != len ) {
    // write() returning less than the full length (a partial write, or -1
    // on error - written defaults to -1 there, so the loop above simply
    // didn't run) used to be silently swallowed: the untransmitted
    // remainder was dropped with no retry and no way for any caller to
    // even find out. This covers macro commands/line endings, keyboard
    // escape sequences and the device-status-report/device-attributes
    // replies the emulation sends back - none of those retry on their
    // own, so at minimum this should be diagnosable.
    qWarning() << "KomportSerial::putStr(): wrote" << written << "of" << len
               << "bytes (" << mPort.errorString() << ")";
    return false;
  }
  return true;
}

/** set size of the internal RX buffer high-water mark */
int KomportSerial::setRxQueue(int _i){
  // A high-water mark of 0 or less makes slotDataAvailable()'s overflow
  // trim ("remove everything past mRxQueueMax bytes") discard every byte
  // that arrives - silent, total data loss. The settings dialog's spinbox
  // only allows 1024..32768, but a hand-edited profile in QSettings isn't
  // range-checked at all before reaching here; clamp to a sane minimum so
  // this setter is safe regardless of where the value came from.
  mRxQueueMax = qMax(1, _i);
  return mRxQueueMax;
}

/** QSerialPort has data available */
void KomportSerial::slotDataAvailable(){
  mRxBuffer += mPort.readAll();
  if ( mRxBuffer.size() > mRxQueueMax ) {
    // drop the oldest overflow rather than growing unbounded - mirrors the
    // original round-queue's overflow behaviour of overwriting old data.
    mRxBuffer.remove( 0, mRxBuffer.size() - mRxQueueMax );
  }
}

/** QSerialPort reported an error */
void KomportSerial::slotPortError(QSerialPort::SerialPortError error){
  if ( error != QSerialPort::NoError ) {
    qWarning() << "KomportSerial:" << mPort.errorString();
    emit settingsFailed( mPort.errorString() );
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
  mFlushRate = qMax(1, _i);
  mFlushTimer.start( mFlushRate );
}

/** set the character framing */
void KomportSerial::setFraming( const QString &_startbits, const QString &_databits,
                                 const QString &_stopbits, const QString &_parity ){
  mStrStartBits = _startbits;
  mStrDataBits = _databits;
  mStrStopBits = _stopbits;
  mStrParity = _parity;
  if ( isOpen() ) applyPortSettings();
}

/** set flow control */
void KomportSerial::setFlowControl( const QString &_flowControl ){
  mStrFlowControl = _flowControl;
  if ( isOpen() ) applyPortSettings();
}
