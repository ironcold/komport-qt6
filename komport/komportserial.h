/***************************************************************************
                          komportserial.h  -  Komport Serial Port Communicator
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

#ifndef KOMPORTSERIAL_H
#define KOMPORTSERIAL_H

#include "itransport.h"
#include "transportconfiguration.h"

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
  * M8 (ADR-003/SPEC-M8): the class is also the local `ITransport`
  * implementation. Every byte-transmitting entry point routes through one
  * internal write primitive that alone creates the TX observation, the RX
  * observation is emitted before the legacy buffering/character path, and a
  * configuration change is one transaction with read-back values. The legacy
  * character signals (`receivedChar`/`sentChar`) and the `settingsChanged` /
  * `settingsFailed` notifications keep their previous behaviour and remain
  * display/compatibility adapters - they are not the session source.
  *
  *@author Mike Sharkey (original), ported to QSerialPort for Qt6
  */
class KomportSerial : public ITransport {
Q_OBJECT
public:
  explicit KomportSerial(QObject *parent = nullptr);
  ~KomportSerial() override;

  // --- ITransport (ADR-003) ------------------------------------------------
  /** Open the serial port for communication. Reports its own result
    * synchronously and emits `opened` (or exactly one `transportError` for the
    * failed attempt); a failed attempt consumes an activation id and never
    * produces an `opened` for it. On success the stored configuration is
    * applied as the single open-and-configure transaction of SPEC-M8 6.2. */
  bool open() override;
  /** Close the port. Emits exactly one `closed` for the current activation. */
  void close() override;
  /** Is the serial port open? */
  bool isOpen() const override;
  /** Send bytes; returns the number of bytes the transport accepted
    * (0..bytes.size()) or a negative value when the write was refused. */
  qint64 writeBytes(const QByteArray &bytes) override;

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

  // --- configuration (SPEC-M8 6.2) -----------------------------------------
  /** The single configuration entry point.
    *
    * Stores the complete requested configuration and, when the port is open,
    * performs exactly one configuration transaction: the hardware settings are
    * applied once and the result is reported through `configurationChanged`
    * (plus `transportError` with kind "apply" when the port accepted only part
    * of them or refused them all). While the port is closed the values are only
    * stored and no observation is emitted - the next `open()` applies them as
    * its open-and-configure transaction. A request that changes nothing while
    * the port is open is the documented no-op and emits nothing.
    *
    * A changed endpoint while the port is open is an activation change, not a
    * setting change (SPEC-M8 6.2, operation table): the old activation is closed,
    * the new endpoint opened and the request applied as that activation's single
    * open-and-configure transaction, whose result is returned.
    *
    * @return the transaction result; `storedOnly` marks the closed-port case
    */
  ConfigurationResult applyConfiguration(const TransportConfiguration &_requested);
  /** The complete requested configuration currently stored. */
  TransportConfiguration requestedConfiguration() const { return mRequested; }
  /** The configuration currently in force. After a transaction this holds the
    * values the port actually accepted (requested value for every accepted
    * field, the previous value for every rejected one). */
  TransportConfiguration effectiveConfiguration() const { return mEffective; }

protected:
  // --- test seams (SPEC-M8 section 13) -------------------------------------
  // A genuine partial hardware write is not reproducible in this environment and
  // PTYs do not select chunk boundaries deterministically, so the three points
  // where the transport touches QSerialPort (and where its behaviour would
  // otherwise be untestable) are overridable.
  /** Single point where bytes reach the port. Returns the accepted byte count. */
  virtual qint64 writeToPort(const char *_data, qint64 _len);
  /** Single point where bytes are read from the port. */
  virtual QByteArray readFromPort();
  /** Single point where hardware settings are applied.
    *
    * @param _requested the complete requested configuration
    * @param _previous the configuration currently in force
    * @param _effective receives the effective values afterwards: the requested
    *        value for every field the port accepted, the previous value for
    *        every field it rejected
    * @return true when the port accepted every requested hardware field
    */
  virtual bool applyHardwareSettings(const TransportConfiguration &_requested,
                                     const TransportConfiguration &_previous,
                                     TransportConfiguration *_effective);
  /** apply the currently stored framing/parity/flow-control settings to the open port */
  void applyPortSettings();
public slots: // Public slots
  /** Legacy compatibility slot: applies the stored configuration once when the
    *  port is open. The `settingsChanged` signal is deliberately *not*
    *  connected to it any more (SPEC-M8 section 3/6.2: the duplicate hardware
    *  application is gone); signals never cause a hardware application. */
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
  /** Whenever a communications port setting is changed such as baud rate, etc.
   *  Legacy notification only since M8: it never causes a hardware
   *  application (SPEC-M8 section 6.2). */
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
  /** the complete configuration that is requested/stored (SPEC-M8 6.2) */
  TransportConfiguration mRequested;
  /** the configuration currently in force (read-back based) */
  TransportConfiguration mEffective;
  /** number of `open()` attempts so far; every attempt consumes one id */
  quint64 mNextActivationId;
  /** the activation id currently live, or 0 when no activation is live */
  quint64 mCurrentActivationId;
  /** id of the failed `open()` attempt whose error was already reported;
    * used to suppress the duplicate asynchronous error of that same attempt
    * (an in-progress/result guard, never a text comparison - ADR-003) */
  quint64 mFailedAttemptReported;
  /** the result of the last `open()` configuration transaction; the entry point
    * returns it when a live endpoint change forced a reopen (SPEC-M8 6.2) */
  ConfigurationResult mLastResult;
  /** true while the hardware settings of a transaction are being applied; port
    * errors raised in that window are reported as the transaction's outcome and
    * must not become an extra observation (SPEC-M8 6.2). It is restored, not
    * cleared, so a reentrant transaction cannot end the outer one's window. */
  bool mConfigurationTransactionInProgress;
  /** a port error raised during the current transaction, recorded for its result
    * (SPEC-M8 section 9: the failure must reach the record, but as the
    * transaction's own apply error rather than a second runtime event) */
  bool mTransactionPortErrorRaised;
  QString mTransactionPortErrorMessage;
private slots: // Private slots
  /** QSerialPort has data available */
  void slotDataAvailable();
  /** QSerialPort reported an error */
  void slotPortError(QSerialPort::SerialPortError error);
  /** periodic flush of mRxBuffer into receivedChar() signals */
  void slotFlushRxBuffer();
private: // Private methods
  /** the single observed write primitive (ADR-003): every byte-transmitting
    * entry point routes through this, and this alone creates the TX
    * observation. Returns the accepted byte count, negative when refused. */
  qint64 writeRaw(const char *_data, qsizetype _len);
  /** Apply @p _requested; @p _forceHardwareApply is used by `open()`, because a
    * new activation always has to configure the port once even when the
    * requested values equal the current snapshot. */
  ConfigurationResult applyConfigurationInternal(const TransportConfiguration &_requested,
                                                  bool _forceHardwareApply);
  /** Emit the observations of one transaction result (SPEC-M8 6.2). */
  void emitConfigurationObservations(const ConfigurationResult &_result);
  /** Mirror @p _requested into the legacy member fields that the getters and the
    * flush timer use; the configuration stays the single source of truth. */
  void storeRequestedValues(const TransportConfiguration &_requested);
  /** The configuration as the *port* reports it after an attempt: the specified
    * read-back state (SPEC-M8 6.2). Unlike readBackHardwareJson() it returns a
    * full value, so the effective state can be built from it. */
  TransportConfiguration readBackEffectiveConfiguration() const;
  /** Metadata of the current hardware settings as the port reports them. */
  QJsonObject readBackHardwareJson() const;
  /** Is an activation live? Observations never carry the id 0 (ADR-003). */
  bool hasLiveActivation() const { return mCurrentActivationId != 0; }
  /** The id used for observations; only meaningful while hasLiveActivation(). */
  quint64 observingActivationId() const { return mCurrentActivationId; }
};

#endif
