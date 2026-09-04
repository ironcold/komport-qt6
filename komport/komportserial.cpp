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

  if ( !ok ) emit settingsFailed();
}

/** put a character */
void KomportSerial::putChar(char _ch){
  if ( isOpen() ) {
    mPort.putChar( _ch );
  }
}

/** transmit a string */
void KomportSerial::putStr(const char* str){
  if ( str != nullptr && isOpen() ) {
    mPort.write( str, static_cast<qint64>( strlen(str) ) );
  }
}

/** set size of the internal RX buffer high-water mark */
int KomportSerial::setRxQueue(int _i){
  mRxQueueMax = _i;
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
    emit settingsFailed();
  }
}

/** periodic flush of mRxBuffer into receivedChar() signals */
void KomportSerial::slotFlushRxBuffer(){
  while ( !mRxBuffer.isEmpty() ) {
    char ch = mRxBuffer.front();
    mRxBuffer.remove(0, 1);
    emit receivedChar(ch);
  }
}

/** set the rate at which the Rx buffer is flushed */
void KomportSerial::setFlushRate(int _i){
  mFlushRate = _i;
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
