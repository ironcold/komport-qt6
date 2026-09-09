/***************************************************************************
                          komportserial.h  -  Komport Serial Port Communicator
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

#ifndef KOMPORTSERIAL_H
#define KOMPORTSERIAL_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QTimer>
#include <QSerialPort>

/**Encapsulates the TTY (serial port), backed by QSerialPort/QSerialPortInfo.
  *
  * This replaces the original raw POSIX/termios + QSocketNotifier
  * implementation. The public slot/signal surface (putChar/putStr,
  * receivedChar, settingsChanged/settingsFailed) is kept close to the
  * original so KomportView/KomportEmulation/KomportTransfer did not need to
  * be restructured, only the internals changed.
  *
  *@author Mike Sharkey (original), ported to QSerialPort for Qt6
  */
class KomportSerial : public QObject  {
Q_OBJECT
public:
  explicit KomportSerial(QObject *parent = nullptr);
  ~KomportSerial() override;

  /** Open the serial port for communication. */
  bool open();
  /** Close the port. */
  void close();
  /** Is the serial port open? */
  bool isOpen() const;
  /** Return the name of the serial port device (e.g. /dev/ttyUSB0). */
  QString deviceName() const;
  /** return the current baud rate setting. */
  qint32 baudRate() const;
  /** set size of the internal RX buffer high-water mark (see setFlushRate()) */
  int setRxQueue(int _i);
  /** set the rate (ms) at which the Rx buffer is flushed into receivedChar() signals */
  void setFlushRate(int _i);
  /** the current flush rate (ms), after setFlushRate()'s clamping */
  int flushRate() const { return mFlushRate; }
  /** set the character framing. Start bits is kept for UI/config compatibility
   *  only - a UART always uses a single start bit, QSerialPort has no such
   *  setting, so it is not applied to the hardware (same as the original,
   *  which parsed but never actually used it either). */
  void setFraming( const QString &_startbits = "1", const QString &_databits = "8",
                    const QString &_stopbits = "1", const QString &_parity = "NONE" );
  /** set flow control ("XON/XOFF", "RTS/CTS" or "NONE"). New in the Qt6 port:
   *  the original settings dialog offered this but never actually applied it. */
  void setFlowControl( const QString &_flowControl );
private: // Private attributes
  /** the underlying Qt serial port */
  QSerialPort mPort;
  /** The name of the serial port (/dev/ttyUSB0, COM3, ...). */
  QString mDeviceName;
  /** Stores baud rate property. */
  qint32 mBaudRate;
  /**  */
  QString mStrStartBits;
  /**  */
  QString mStrDataBits;
  /**  */
  QString mStrStopBits;
  /**  */
  QString mStrParity;
  /** flow control */
  QString mStrFlowControl;
  /** rate (ms) at which the Rx buffer is flushed */
  int mFlushRate;
  /** buffer of bytes received but not yet emitted via receivedChar() */
  QByteArray mRxBuffer;
  /** high-water mark for mRxBuffer, kept for config/API compatibility */
  int mRxQueueMax;
  /** drives periodic flushing of mRxBuffer */
  QTimer mFlushTimer;
protected: // Protected methods
  /** apply the currently stored framing/parity/flow-control settings to the open port */
  void applyPortSettings();
public slots: // Public slots
  /** Commits settings changes to the serial port. */
  void slotSettingsChanged();
  /** Set the name of the serial port device (e.g. /dev/ttyUSB0). */
  void setDeviceName(const QString &_dn);
  /** Set the baud rate. */
  void setBaudRate(qint32 _baud);
  /** Set the baud rate, parsed from a string (as used by the settings dialog / config file). */
  void setBaudRate(const QString &_baud);
  /** put a character. Returns false if the port isn't open or the
   *  underlying write failed (nothing was sent) - callers that need to
   *  know whether a byte actually made it out (e.g. file transfers) can
   *  check this instead of assuming success. */
  bool putChar(char _ch);
  /** transmit a null-terminated string. Returns false if the port isn't
   *  open or the write was partial/failed (logged via qWarning() either
   *  way) - most callers (keyboard escape sequences, device-status
   *  replies) are fire-and-forget and don't check this, but it's
   *  available for callers that want to. Delegates to the length-aware
   *  overload below via strlen(str) - any embedded NUL truncates the
   *  string at that point, same as any other null-terminated-string API;
   *  a caller that needs to send bytes verbatim (including a real
   *  embedded NUL) must use the length-aware overload directly instead. */
  bool putStr(const char* str);
  /** transmit exactly _len bytes starting at _str, verbatim - including
   *  any embedded NUL bytes, unlike the null-terminated overload above.
   *  Milestone 7 (Codex review finding): KomportApp::slotMacroTriggered()
   *  needs this - charset-translated macro text can legitimately contain
   *  a byte value of 0x00 (e.g. CP437's byte 0x00, which is real Unicode
   *  NUL after Milestone 7's 0x00-0x7F identity scoping - see
   *  KomportCharset), and the null-terminated overload's strlen()-based
   *  length would have silently truncated everything after it. */
  bool putStr(const char* _str, qsizetype _len);
signals: // Signals
  /** Whenever a communications port setting is changed such as baud rate, etc. */
  void settingsChanged();
  /** could not apply the current settings to the open port, or the port
   *  reported an error while open (including failing to open at all -
   *  QSerialPort::open() itself surfaces failures via errorOccurred(), see
   *  slotPortError() below). _reason is a human-readable message suitable
   *  for showing directly to the user. */
  void settingsFailed(const QString &_reason);
  /** received a char */
  void receivedChar(char _ch);
  /** a char was actually written to the port (for e.g. the hex monitor) */
  void sentChar(char _ch);
private slots: // Private slots
  /** QSerialPort has data available */
  void slotDataAvailable();
  /** QSerialPort reported an error */
  void slotPortError(QSerialPort::SerialPortError error);
  /** periodic flush of mRxBuffer into receivedChar() signals */
  void slotFlushRxBuffer();
};

#endif
