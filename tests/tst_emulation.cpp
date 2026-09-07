/***************************************************************************
                          tst_emulation.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : 2026 (new in the Qt6 port)
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de

    Regression test for KomportEmulation::sequence()'s CSI escape-sequence
    buffer cap: a device sending "ESC[" followed by an endless run of
    digits/semicolons and no final letter used to make mCtlSequence grow
    without bound (see TODO.md's Codex-review section). Also covers the
    cursor-clamping fixes from earlier reviews (TODO-ARCHIVE.md) so those
    stay guarded against regressions too, per the review's own
    recommendation to add tests for exactly those crash classes.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "komportemulation.h"
#include "komportcellarray.h"
#include "komportserial.h"

#include <QTest>
#include <QSignalSpy>

class TstEmulation : public QObject
{
  Q_OBJECT
private slots:
  void unboundedCsiSequenceDoesNotHang();
  void cursorMovementClampsToScreenBounds();
  void eraseInDisplayStopsAtCursorAndNotifies();
  void csiFinalByteRecognizesFullAnsiRange();
  void escZRespondsLikeDeviceAttributes();
};

void TstEmulation::unboundedCsiSequenceDoesNotHang()
{
  KomportSerial serial;
  KomportCellArray cellArray;
  KomportEmulation emu(&serial, &cellArray);

  emu.slotReceivedChar(0x1B); // ESC
  emu.slotReceivedChar('[');
  // No real terminal sequence gets anywhere near this long; if the cap in
  // sequence() regresses, this loop is what would previously have made
  // mCtlSequence grow to 100000 bytes (and the test still complete, just
  // wastefully - the actual regression this guards is unbounded growth
  // under sustained adversarial/malfunctioning input, not a hang here).
  for ( int i = 0; i < 100000; ++i ) {
    emu.slotReceivedChar('0');
  }

  // The emulation must have recovered its state (aborted the runaway
  // sequence) rather than still waiting for a final byte - a subsequent,
  // well-formed sequence must be processed normally afterwards.
  emu.slotReceivedChar(0x1B);
  emu.slotReceivedChar('[');
  emu.slotReceivedChar('H'); // cursor home
  QCOMPARE( cellArray.cursor(), QPoint(0, 0) );
}

void TstEmulation::cursorMovementClampsToScreenBounds()
{
  KomportSerial serial;
  KomportCellArray cellArray; // 80x25 by default
  KomportEmulation emu(&serial, &cellArray);

  auto sendCsi = [&](const QByteArray &params, char finalByte) {
    emu.slotReceivedChar(0x1B);
    emu.slotReceivedChar('[');
    for ( char c : params ) emu.slotReceivedChar(c);
    emu.slotReceivedChar(finalByte);
  };

  // "Move cursor forward by an enormous amount" must clamp to the last
  // column, not overflow/wrap - this is the class of bug fixed in
  // TODO-ARCHIVE.md's cursor-clamping section.
  sendCsi( "999999999", 'C' ); // cursor forward
  QCOMPARE( cellArray.cursor().x(), cellArray.arrayWidth() - 1 );

  sendCsi( "999999999", 'B' ); // cursor down
  QCOMPARE( cellArray.cursor().y(), cellArray.arrayHeight() - 1 );

  // And the opposite direction must clamp at 0, not go negative.
  sendCsi( "999999999", 'D' ); // cursor backward
  QCOMPARE( cellArray.cursor().x(), 0 );

  sendCsi( "999999999", 'A' ); // cursor up
  QCOMPARE( cellArray.cursor().y(), 0 );
}

void TstEmulation::eraseInDisplayStopsAtCursorAndNotifies()
{
  KomportSerial serial;
  KomportCellArray cellArray; // 80x25 by default
  KomportEmulation emu(&serial, &cellArray);

  // Fill the whole grid with a known, non-blank character directly (no
  // need to route this through the emulation).
  for ( int y = 0; y < cellArray.arrayHeight(); ++y ) {
    for ( int x = 0; x < cellArray.arrayWidth(); ++x ) {
      cellArray.drawChar( QChar('X'), x, y );
    }
  }

  auto sendCsi = [&](const QByteArray &params, char finalByte) {
    emu.slotReceivedChar(0x1B);
    emu.slotReceivedChar('[');
    for ( char c : params ) emu.slotReceivedChar(c);
    emu.slotReceivedChar(finalByte);
  };

  const int cursorX = 10;
  const int cursorY = 5;
  sendCsi( QByteArray::number(cursorY+1) + ";" + QByteArray::number(cursorX+1), 'H' ); // CUP

  QSignalSpy cellChangedSpy( &cellArray, &KomportCellArray::cellChanged );
  sendCsi( "1", 'J' ); // CSI 1 J - erase from start of screen through cursor, inclusive

  // Every cell strictly *below* the cursor's row must be untouched - this
  // is the exact bug: the old code cleared the whole screen instead of
  // stopping at the cursor.
  for ( int x = 0; x < cellArray.arrayWidth(); ++x ) {
    QCOMPARE( cellArray.cell(x, cursorY+1)->character(), QChar('X') );
    QCOMPARE( cellArray.cell(x, cellArray.arrayHeight()-1)->character(), QChar('X') );
  }
  // Every cell on an earlier row, and up to and including the cursor on
  // its own row, must be cleared (space).
  for ( int x = 0; x < cellArray.arrayWidth(); ++x ) {
    QCOMPARE( cellArray.cell(x, 0)->character(), QChar(' ') );
  }
  for ( int x = 0; x <= cursorX; ++x ) {
    QCOMPARE( cellArray.cell(x, cursorY)->character(), QChar(' ') );
  }
  // And past the cursor on its own row, unaffected.
  QCOMPARE( cellArray.cell(cursorX+1, cursorY)->character(), QChar('X') );

  // The other bug fixed alongside this: clearing cells directly instead
  // of through cellArray()->clear() skipped cellChanged() entirely, so
  // KomportView (which only repaints via that signal) never redrew the
  // erased region.
  QVERIFY2( cellChangedSpy.count() > 0,
            "CSI 1 J must notify via cellChanged() for the cells it clears" );
}

void TstEmulation::csiFinalByteRecognizesFullAnsiRange()
{
  KomportSerial serial;
  KomportCellArray cellArray; // 80x25 by default
  KomportEmulation emu(&serial, &cellArray);

  auto sendCsi = [&](const QByteArray &params, char finalByte) {
    emu.slotReceivedChar(0x1B);
    emu.slotReceivedChar('[');
    for ( char c : params ) emu.slotReceivedChar(c);
    emu.slotReceivedChar(finalByte);
  };

  // Move away from the default (0,0) first so a stray doCursorUp() (see
  // below) is actually observable.
  sendCsi( "5;5", 'H' ); // CUP - cursor to (row 5, col 5), 1-indexed
  QCOMPARE( cellArray.cursor(), QPoint(4, 4) );

  // CSI 5 @ (Insert Character - not implemented by this emulation at all,
  // but must still be recognised as a *complete* sequence: per ECMA-48/
  // ANSI X3.64, any byte in 0x40-0x7E ends a CSI sequence, not just
  // letters). sequence() used to only recognise a-z/A-Z as a final byte,
  // so this sequence never completed - it kept absorbing bytes, and the
  // plain letter 'A' right after it got swallowed as *this* sequence's
  // final byte (CSI ... A = cursor up) instead of being printed as text.
  emu.slotReceivedChar(0x1B);
  emu.slotReceivedChar('[');
  emu.slotReceivedChar('5');
  emu.slotReceivedChar('@');
  emu.slotReceivedChar('A');

  QCOMPARE( cellArray.cell(4,4)->character(), QChar('A') );
  QCOMPARE( cellArray.cursor(), QPoint(5,4) ); // advanced by printing 'A', not moved up
}

void TstEmulation::escZRespondsLikeDeviceAttributes()
{
  // The serial port is never opened in this test, so putStr() (which
  // doDeviceAttributes() would use to send the actual reply) is a no-op
  // and the reply bytes themselves aren't observable here - this only
  // verifies that ESC Z is recognised as a complete, handled escape
  // sequence (it used to fall through shortEscape()'s default case and do
  // nothing at all) rather than crashing or leaving the emulation stuck
  // waiting for more of a sequence that will never come.
  KomportSerial serial;
  KomportCellArray cellArray;
  KomportEmulation emu(&serial, &cellArray);

  emu.slotReceivedChar(0x1B);
  emu.slotReceivedChar('Z'); // classic VT100 "identify" request

  // Must be ready for normal input again right afterwards.
  emu.slotReceivedChar(0x1B);
  emu.slotReceivedChar('[');
  emu.slotReceivedChar('H'); // cursor home
  QCOMPARE( cellArray.cursor(), QPoint(0, 0) );
}

QTEST_MAIN(TstEmulation)
#include "tst_emulation.moc"
