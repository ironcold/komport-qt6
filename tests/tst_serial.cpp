/***************************************************************************
                          tst_serial.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : 2026 (new in the Qt6 port)
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de

    Regression test for KomportSerial::setRxQueue()/setFlushRate(): a
    hand-edited profile isn't range-checked by the settings dialog's
    spinboxes, and a value <= 0 for RXQueue used to make
    slotDataAvailable()'s overflow trim discard every byte that arrives
    (see TODO.md's Codex-review section).
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
  void flushRateClampsToNonNegative();
};

void TstSerial::rxQueueClampsToPositive()
{
  KomportSerial serial;
  QCOMPARE( serial.setRxQueue(-1), 1 );
  QCOMPARE( serial.setRxQueue(0), 1 );
  // A normal, already-valid value must pass through unchanged.
  QCOMPARE( serial.setRxQueue(4096), 4096 );
}

void TstSerial::flushRateClampsToNonNegative()
{
  KomportSerial serial;
  // setFlushRate() has no return value to inspect directly, but it must
  // not crash/assert on a negative interval (QTimer::start() only accepts
  // non-negative intervals) and must leave the timer running afterwards.
  serial.setFlushRate(-1);
  serial.setFlushRate(100);
}

QTEST_MAIN(TstSerial)
#include "tst_serial.moc"
