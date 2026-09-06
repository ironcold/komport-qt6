/***************************************************************************
                          tst_serial.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : 2026 (new in the Qt6 port)
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de

    Regression tests for KomportSerial::setRxQueue()/setFlushRate()/
    putChar()/putStr(): a value <= 0 for RXQueue used to make
    slotDataAvailable()'s overflow trim discard every byte that arrives,
    and a FlushRate of 0 (previously reachable from the settings dialog's
    spinbox directly, not just a hand-edited profile) turned the RX flush
    timer into an idle busy-poll (QTimer::start(0) re-fires on every
    single event-loop iteration) - see TODO.md's Codex-review section.
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

#include <QTest>

class TstSerial : public QObject
{
  Q_OBJECT
private slots:
  void rxQueueClampsToPositive();
  void flushRateClampsToPositive();
  void putCharAndPutStrReportFailureWhenClosed();
};

void TstSerial::rxQueueClampsToPositive()
{
  KomportSerial serial;
  QCOMPARE( serial.setRxQueue(-1), 1 );
  QCOMPARE( serial.setRxQueue(0), 1 );
  // A normal, already-valid value must pass through unchanged.
  QCOMPARE( serial.setRxQueue(4096), 4096 );
}

void TstSerial::flushRateClampsToPositive()
{
  KomportSerial serial;
  // 0 used to be accepted (both here and by the settings dialog's
  // spinbox) - QTimer::start(0) re-fires on every single event-loop
  // iteration for as long as the timer is running, an idle busy-poll
  // rather than a "flush immediately" setting.
  serial.setFlushRate(-1);
  QCOMPARE( serial.flushRate(), 1 );
  serial.setFlushRate(0);
  QCOMPARE( serial.flushRate(), 1 );
  // A normal, already-valid value must pass through unchanged.
  serial.setFlushRate(100);
  QCOMPARE( serial.flushRate(), 100 );
}

void TstSerial::putCharAndPutStrReportFailureWhenClosed()
{
  // Regression test for putStr(): it used to be void, silently dropping a
  // partial/failed write with no way for a caller to notice (see TODO.md's
  // Codex-review section, third full review). This sandbox has no real
  // serial device to actually exercise a genuine partial hardware write,
  // but the closed-port early-return path is the one piece of that
  // contract that's reliably testable without one - both putChar() and
  // putStr() must report failure (not silently claim success) when there
  // is nowhere for the bytes to go.
  KomportSerial serial; // never opened
  QVERIFY( !serial.isOpen() );
  QCOMPARE( serial.putChar('x'), false );
  QCOMPARE( serial.putStr("hello"), false );
  // Must not crash on a null string either.
  QCOMPARE( serial.putStr(nullptr), false );
}

QTEST_MAIN(TstSerial)
#include "tst_serial.moc"
