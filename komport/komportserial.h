/***************************************************************************
                          komportserial.h  -  Komport Serial Port Communicator
                             -------------------
    begin                : Mon Feb 17 2003
    copyright            : (C) 2003 by Mike Sharkey
    email                : michael@sharkey.servebeer.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KOMPORTSERIAL_H
#define KOMPORTSERIAL_H

#include <qobject.h>
#include <errno.h>

#include <qsocketnotifier.h>

#include  "komportqueue.h"
/**Encapsulates the TTY (serial port).
  *@author Mike Sharkey
  */

class KomportSerial : public QObject  {
Q_OBJECT
public: 
  typedef enum {
    b0, b50, b75, b110, b134, b150, b200, b300, b600, b1200, b1800, b2400, b4800,
    b9600, b19200, b38400, b57600, b115200, b230400
  } Baud;
  KomportSerial();
	~KomportSerial();
  /** Open the serial port for communication. */
  bool open();
  /** Close the port. */
  void close();
  /** In the serial port open? */
  bool isOpen();
  /** Return the name of the serial port device (e.g. /dev/ttyxx). */
  QCString deviceName();
  /** return the current baud rate setting. */
  Baud baudRate();
  /** set size of RX queue */
  int setRxQueue(int _i);
  /** set the rate at which the Tx/Rx queue(s) are flushed */
  void setFlushRate(int _i);
  /** set the character framing */
  void setFraming( QString _startbits="1", QString _databits="8", QString _stopbits="1", QString _parity="NONE" );
private: // Private attributes
  /** The name of the serial port (/dev/ttyxx). */
  QCString mDeviceName;
  /** Stores file number (handle). */
  int mFileNo;
  /** Stores baud rate property. */
  Baud mBaudRate;
  /** timer id */
  int mTimerId;
  /** round Rx queue */
  KomportQueue mRxQueue;
  /** socket notifier */
  QSocketNotifier *mSocketNotifier;
  /**  */
  QString mStrStartBits;
  /**  */
  QString mStrDataBits;
  /**  */
  QString mStrStopBits;
  /**  */
  QString mStrParity;
  /** rate at which the Tx/Rx queue(s) are flushed */
  int mFlushRate;
protected: // Protected methods
  /** Return the file number (handle). */
  int handle();
  /** timer event */
  void timerEvent(QTimerEvent* _e);
public slots: // Public slots
  /** Commits settings changes to the serial port. */
  void slotSettingsChanged();
  /** Set the name of the serial port device (e.g. /dev/ttyxx). */
  void setDeviceName(QCString _dn);
  /** Set the name of the serial port device (e.g. /dev/ttyxx). */
  void setDeviceName(QString _dn);
  /** Set the baud rate to one of the predevied enums. */
  void setBaudRate(Baud _baud=b9600);
  /** Set the baud rate to one of the predevied enums. */
  void setBaudRate(QString _baud);
  /** put a character */
  void putChar(char _ch);
  /** transmit a string */
  void putStr(const char* str);
signals: // Signals
  /** Whenever a communications port setting is changed such as baud rate, etc. */
  void settingsChanged();
  /** No descriptions */
  void settingsFailed();
  /** received a char */
  void receivedChar(char _ch);
protected slots: // Protected slots
  /** socket notifier has detected data */
  void slotDataAvailable();
};

#endif
