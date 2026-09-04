/***************************************************************************
                          komporthexview.h  -  Komport Serial Port Communicator
                             -------------------
    begin                : 2026 (new in the Qt6 port)
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de

    A toggleable hex monitor. Real serial gear (switches, PLCs, ...)
    routinely sends control characters (NUL, ETX, stray CR/LF
    combinations, ...) that a plain text terminal either swallows or lets
    corrupt the on-screen layout - this makes every byte, printable or
    not, visible in the classic [offset] [hex] [ascii] form, for both
    directions (RX from the wire, TX what was actually sent).
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KOMPORTHEXVIEW_H
#define KOMPORTHEXVIEW_H

#include <QWidget>
#include <QByteArray>

class QPlainTextEdit;
class QTimer;
class QCheckBox;
class QSettings;

/** Split-screen hex dump panel: shows raw bytes exchanged over the serial
 *  port as classic 16-bytes-per-row [offset] [hex] [ascii] lines, tagged
 *  RX (received) or TX (sent), each direction with its own running byte
 *  offset. Meant to sit next to KomportView in a QSplitter, toggled by
 *  KomportApp's "Hex Monitor" action.
 */
class KomportHexView : public QWidget
{
  Q_OBJECT
public:
  explicit KomportHexView(QWidget *parent = nullptr);
  ~KomportHexView() override;

  /** persist the RX/TX filter checkbox states into _settings (nested
   *  under a "HexMonitor" group - called from within a Profiles/<name>
   *  group, same pattern as KomportMacroBar::saveSettings()) */
  void saveSettings(QSettings *_settings) const;
  /** load the RX/TX filter checkbox states from _settings, defaulting
   *  both to checked if absent (not contains()-guarded - see the note on
   *  KomportMacroBar::loadSettings() for why: a profile without a
   *  "HexMonitor" group must reset to the default, not keep whatever the
   *  previously loaded profile left the checkboxes at) */
  void loadSettings(QSettings *_settings);

public slots:
  /** feed one byte received from the serial port */
  void appendRx(char _ch);
  /** feed one byte actually sent to the serial port */
  void appendTx(char _ch);
  /** clear the log and reset both offset counters */
  void clearLog();

private slots:
  /** flush whatever partial (<16 byte) row is pending, so short/idle
   *  bursts still show up promptly instead of waiting for a full row */
  void flushPending();

private:
  enum class Direction { Rx, Tx };
  void appendByte(Direction _dir, char _ch);
  void flushLine(Direction _dir);
  static QString formatRow(const char *_tag, quint64 _offset, const QByteArray &_bytes);

  QPlainTextEdit *mLog;
  QCheckBox *mRxCheck;
  QCheckBox *mTxCheck;
  QByteArray mRxBuffer;
  QByteArray mTxBuffer;
  quint64 mRxOffset;
  quint64 mTxOffset;
  QTimer *mFlushTimer;
};

#endif // KOMPORTHEXVIEW_H
