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

    Extended for Milestone 4 (TODO.md): 256-color SGR (CSI 38;5;N / 48;5;N),
    DECSTBM scroll regions (CSI Pt;Pb r) including the full-screen-still-
    feeds-scrollback invariant, insert mode (CSI 4h/4l), and the VT220
    device-attributes response.
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
  void csi256ColorSetsIndexedForegroundAndBackground();
  void malformed256ColorSubSequenceDoesNotCorruptOtherAttributes();
  void insertModeShiftsRestOfRowRight();
  void scrollRegionConfinesIndexAndReverseIndex();
  void indexAndReverseIndexOutsideRegionDoNotScroll();
  void fullScreenRegionStillFeedsScrollbackAfterExplicitReset();
  void zeroParamsResetScrollRegionToFullScreen();
  void fullScreenResetSurvivesLaterResize();
  void printableAutoWrapRespectsScrollRegion();
  void insertAndDeleteLineRespectScrollRegion();
  void deviceAttributesRecognizesCsiForm();
  void deleteCharClearsWithConfiguredDefaultColors();
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

void TstEmulation::csi256ColorSetsIndexedForegroundAndBackground()
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

  // Index 196 and 46 are the pure-red and pure-green corners of xterm's
  // 6x6x6 color cube (well-known values, independently checkable against
  // any xterm 256-color chart) - a good cross-check that the cube-step
  // arithmetic in xterm256ToColor() is right, not just internally
  // self-consistent.
  sendCsi("38;5;196", 'm'); // foreground: cube red
  cellArray.drawChar(QChar('A'), 0, 0);
  QCOMPARE( cellArray.cell(0,0)->foregroundColor(), QColor(255,0,0) );

  sendCsi("48;5;46", 'm'); // background: cube green
  cellArray.drawChar(QChar('B'), 1, 0);
  QCOMPARE( cellArray.cell(1,0)->backgroundColor(), QColor(0,255,0) );

  // Index 244 sits in the 24-step grayscale ramp (232-255) - "Grey50",
  // 128/128/128 on any standard xterm 256-color chart.
  sendCsi("48;5;244", 'm');
  cellArray.drawChar(QChar('C'), 2, 0);
  QCOMPARE( cellArray.cell(2,0)->backgroundColor(), QColor(128,128,128) );

  // Index 9 is one of the base 16 (bright red) - must be byte-for-byte the
  // same QColor plain "CSI 91m" already produces, so a host mixing both
  // forms for the same color gets the same pixel either way.
  sendCsi("0", 'm'); // reset first (previous background would otherwise persist)
  sendCsi("38;5;9", 'm');
  cellArray.drawChar(QChar('D'), 3, 0);
  QCOMPARE( cellArray.cell(3,0)->foregroundColor(), QColor(255,0,0).lighter(140) );
}

void TstEmulation::malformed256ColorSubSequenceDoesNotCorruptOtherAttributes()
{
  KomportSerial serial;
  KomportCellArray cellArray;
  KomportEmulation emu(&serial, &cellArray);

  auto sendCsi = [&](const QByteArray &params, char finalByte) {
    emu.slotReceivedChar(0x1B);
    emu.slotReceivedChar('[');
    for ( char c : params ) emu.slotReceivedChar(c);
    emu.slotReceivedChar(finalByte);
  };

  // Well-formed case first: bold on, extended foreground color 196,
  // underline on - all in one SGR sequence. Exercises the general
  // index-skipping mechanism on the *complete* path.
  sendCsi("1;38;5;196;4", 'm');
  QVERIFY( cellArray.bold() );
  QVERIFY( cellArray.underline() );
  QVERIFY( !cellArray.blink() );
  QCOMPARE( cellArray.foregroundColor(), QColor(255,0,0) );

  // Now the actually *malformed*/truncated cases (a Codex review finding:
  // the first version of this code only advanced past the 38/5/index
  // fields on the complete path - a truncated "38;5" with no index
  // following it left the next field un-consumed, so it fell through and
  // got misread as an unrelated top-level SGR code on the next loop
  // iteration). "38;5" alone (no index): the "5" must be consumed as part
  // of the (incomplete) extended-color sequence, NOT reinterpreted as
  // "Blink on".
  sendCsi("0", 'm'); // reset first
  sendCsi("38;5", 'm');
  QVERIFY2( !cellArray.blink(), "truncated \"38;5\" must not fall through as Blink on" );

  // "38;2;255;0" (true-color, only 2 of the 3 rgb components present): all
  // present components must be consumed, so the trailing "0" is NOT
  // reinterpreted as "reset all attributes" (which would wipe the bold set
  // just before it).
  sendCsi("0", 'm');
  sendCsi("1;38;2;255;0", 'm');
  QVERIFY2( cellArray.bold(), "trailing incomplete rgb component must not fall through as attribute reset" );

  // "38;5;;m" (empty color index between the semicolons) and "38;5;xm"
  // (non-numeric): a second Codex review round on the fix above found that
  // both still silently applied color index 0 (a real, valid black) via
  // plain toInt()'s "0 for anything unparseable" behaviour - indistinguishable
  // from an explicit, well-formed "38;5;0". Set a known, distinctive color
  // first so a wrong-but-silent fallback to black/index-0 is observable.
  sendCsi("0", 'm');
  sendCsi("38;5;196", 'm'); // known distinctive color (cube red) first
  QCOMPARE( cellArray.foregroundColor(), QColor(255,0,0) );
  sendCsi("38;5;", 'm'); // empty index
  QVERIFY2( cellArray.foregroundColor() == QColor(255,0,0),
            "malformed (empty) color index must not silently apply color 0" );
  // "." rather than a letter: any byte in 0x40-0x7E (which most letters
  // fall in) ends a CSI sequence per ECMA-48/ANSI X3.64 (see sequence()'s
  // own comment) - a letter here would terminate the sequence early
  // instead of being parsed as part of this malformed parameter.
  sendCsi("38;5;.", 'm'); // non-numeric index
  QVERIFY2( cellArray.foregroundColor() == QColor(255,0,0),
            "malformed (non-numeric) color index must not silently apply color 0" );
}

void TstEmulation::insertModeShiftsRestOfRowRight()
{
  KomportSerial serial;
  KomportCellArray cellArray;
  KomportEmulation emu(&serial, &cellArray);

  auto sendCsi = [&](const QByteArray &params, char finalByte) {
    emu.slotReceivedChar(0x1B);
    emu.slotReceivedChar('[');
    for ( char c : params ) emu.slotReceivedChar(c);
    emu.slotReceivedChar(finalByte);
  };

  // "ABC" typed normally (replace mode, the default).
  emu.slotReceivedChar('A');
  emu.slotReceivedChar('B');
  emu.slotReceivedChar('C');
  QCOMPARE( cellArray.cursor(), QPoint(3,0) );

  // Move back between 'A' and 'B', enable insert mode (CSI 4h), type 'X' -
  // must push "BC" one column right instead of overwriting 'B'.
  sendCsi("1;2", 'H'); // row 1, col 2 (1-based) -> (x=1,y=0), right after 'A'
  sendCsi("4", 'h');   // DECIM on
  emu.slotReceivedChar('X');

  QCOMPARE( cellArray.cell(0,0)->character(), QChar('A') );
  QCOMPARE( cellArray.cell(1,0)->character(), QChar('X') );
  QCOMPARE( cellArray.cell(2,0)->character(), QChar('B') );
  QCOMPARE( cellArray.cell(3,0)->character(), QChar('C') );
  QCOMPARE( cellArray.cursor(), QPoint(2,0) ); // advanced past the inserted 'X'

  // CSI 4l turns it back off - back to plain overwrite.
  sendCsi("4", 'l');
  sendCsi("1;1", 'H'); // home
  emu.slotReceivedChar('Z');
  QCOMPARE( cellArray.cell(0,0)->character(), QChar('Z') );
  QCOMPARE( cellArray.cell(1,0)->character(), QChar('X') ); // unshifted
}

void TstEmulation::scrollRegionConfinesIndexAndReverseIndex()
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

  // Markers just outside the region-to-be, so any leakage past the region
  // boundary is immediately visible.
  cellArray.drawChar(QChar('T'), 0, 3);  // row 3: one above the region
  cellArray.drawChar(QChar('B'), 0, 11); // row 11: one below the region

  sendCsi("5;10", 'r'); // DECSTBM: rows 5-10 (1-based) -> 0-based [4,9]
  QCOMPARE( cellArray.cursor(), QPoint(0,0) ); // DECSTBM homes the cursor

  QSignalSpy scrollSpy( &cellArray, &KomportCellArray::aboutToScrollUp );

  // Put the cursor on the region's bottom row and index past it - must
  // scroll only rows [4,9], and must NOT feed the scrollback buffer (a
  // region-confined scroll isn't "history").
  sendCsi("10;1", 'H'); // row 10, col 1 -> (0, 9), the region's bottom row
  cellArray.drawChar(QChar('9'), 0, 9); // mark the row about to scroll off
  emu.slotReceivedChar(0x0A); // LF -> doIndex(), region-aware

  QCOMPARE( scrollSpy.count(), 0 );
  QCOMPARE( cellArray.cursor(), QPoint(0,9) ); // stayed at the bottom margin
  QCOMPARE( cellArray.cell(0,3)->character(), QChar('T') );  // untouched
  QCOMPARE( cellArray.cell(0,11)->character(), QChar('B') ); // untouched
  QCOMPARE( cellArray.cell(0,8)->character(), QChar('9') );  // shifted up from row 9
  QCOMPARE( cellArray.cell(0,9)->character(), QChar(' ') );  // scrolled up out of the region

  // Reverse index at the region's top row must scroll the region back
  // down, again without touching rows outside [4,9].
  sendCsi("5;1", 'H'); // row 5, col 1 -> (0,4), the region's top row
  cellArray.drawChar(QChar('4'), 0, 4);
  emu.slotReceivedChar(0x1B);
  emu.slotReceivedChar('M'); // ESC M -> doReverseIndex()

  QCOMPARE( cellArray.cursor(), QPoint(0,4) ); // stayed at the top margin
  QCOMPARE( cellArray.cell(0,3)->character(), QChar('T') );  // still untouched
  QCOMPARE( cellArray.cell(0,11)->character(), QChar('B') ); // still untouched
  QCOMPARE( cellArray.cell(0,4)->character(), QChar(' ') );  // blank row scrolled in from the top
  QCOMPARE( cellArray.cell(0,5)->character(), QChar('4') );  // shifted down from row 4
}

void TstEmulation::indexAndReverseIndexOutsideRegionDoNotScroll()
{
  // Codex review findings: the first version of doIndex()/doReverseIndex()
  // scrolled whenever the cursor was *at or past* the margin ("pos.y() >=
  // bottom" / "pos.y() <= top") instead of *exactly at* it - so a cursor
  // sitting outside the region entirely (below it for Index, above it for
  // Reverse Index - both reachable via an absolute CUP, and the latter
  // right after any DECSTBM, which homes the cursor to (0,0)) wrongly
  // scrolled a region it wasn't even inside.
  KomportSerial serial;
  KomportCellArray cellArray; // 80x25 by default
  KomportEmulation emu(&serial, &cellArray);

  auto sendCsi = [&](const QByteArray &params, char finalByte) {
    emu.slotReceivedChar(0x1B);
    emu.slotReceivedChar('[');
    for ( char c : params ) emu.slotReceivedChar(c);
    emu.slotReceivedChar(finalByte);
  };

  sendCsi("5;10", 'r'); // region rows 5-10 (1-based) -> 0-based [4,9]

  // Index with the cursor *below* the region (row 12, 0-based 11): must
  // just move down by one, clamped to the physical screen - no scroll.
  {
    QSignalSpy regionSpy( &cellArray, &KomportCellArray::rowChanged );
    sendCsi("12;1", 'H'); // (0,11)
    emu.slotReceivedChar(0x0A); // LF -> doIndex()
    QCOMPARE( cellArray.cursor(), QPoint(0,12) ); // moved down by one, nothing scrolled
    QCOMPARE( regionSpy.count(), 0 );
  }

  // Reverse index with the cursor *above* the region (row 2, 0-based 1,
  // e.g. right after the DECSTBM above homed it to (0,0) and it moved down
  // once): must just move up by one - no scroll.
  {
    sendCsi("2;1", 'H'); // (0,1)
    QSignalSpy regionSpy( &cellArray, &KomportCellArray::rowChanged );
    emu.slotReceivedChar(0x1B);
    emu.slotReceivedChar('M'); // ESC M -> doReverseIndex()
    QCOMPARE( cellArray.cursor(), QPoint(0,0) );
    QCOMPARE( regionSpy.count(), 0 );
  }
}

void TstEmulation::fullScreenRegionStillFeedsScrollbackAfterExplicitReset()
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

  // First narrow the region, confirm it really did (no scrollback signal
  // on a region-confined scroll - already covered in detail by
  // scrollRegionConfinesIndexAndReverseIndex() above, just re-checked
  // briefly here as the "before" half of this test).
  sendCsi("1;10", 'r');
  {
    QSignalSpy narrowSpy( &cellArray, &KomportCellArray::aboutToScrollUp );
    sendCsi("10;1", 'H');
    emu.slotReceivedChar(0x0A);
    QCOMPARE( narrowSpy.count(), 0 );
  }

  // "CSI r" with no parameters resets the region to the whole screen
  // (Pt defaults to 1, Pb to the last row) - the critical invariant this
  // guards: after that reset, an Index/LF at the bottom margin must go
  // right back through the ordinary whole-screen scrollUp(), i.e. keep
  // feeding scrollback exactly as it did before Milestone 4 introduced
  // scroll regions at all.
  sendCsi("", 'r');
  QSignalSpy fullScreenSpy( &cellArray, &KomportCellArray::aboutToScrollUp );
  sendCsi("25;1", 'H'); // last row of the (now full-screen) region
  emu.slotReceivedChar(0x0A);
  QCOMPARE( fullScreenSpy.count(), 1 );
}

void TstEmulation::zeroParamsResetScrollRegionToFullScreen()
{
  // Codex review finding: ctlParam() (used by every other CSI parameter in
  // this emulation) already treats a 0 or missing numeric parameter as
  // "use the default" - doSetScrollRegion() didn't follow that convention
  // and instead computed top=bottom=0 for "CSI 0;0 r", a degenerate region
  // that got silently ignored, leaving whatever region was previously
  // active untouched instead of resetting to the full screen like "CSI r"
  // (no params at all) already correctly does.
  KomportSerial serial;
  KomportCellArray cellArray; // 80x25 by default
  KomportEmulation emu(&serial, &cellArray);

  auto sendCsi = [&](const QByteArray &params, char finalByte) {
    emu.slotReceivedChar(0x1B);
    emu.slotReceivedChar('[');
    for ( char c : params ) emu.slotReceivedChar(c);
    emu.slotReceivedChar(finalByte);
  };

  sendCsi("5;10", 'r'); // narrow the region first
  sendCsi("0;0", 'r');  // must reset it to the full screen, like "CSI r" does

  QSignalSpy fullScreenSpy( &cellArray, &KomportCellArray::aboutToScrollUp );
  sendCsi("25;1", 'H'); // last row of the full screen
  emu.slotReceivedChar(0x0A);
  QCOMPARE( fullScreenSpy.count(), 1 );
}

void TstEmulation::fullScreenResetSurvivesLaterResize()
{
  // Codex review finding (round 4): an intermediate fix for
  // zeroParamsResetScrollRegionToFullScreen() above stored the *current*
  // arrayHeight()-1 as a concrete bottom margin for "reset to full
  // screen" - indistinguishable from a real full-screen region right up
  // until the grid is resized (KomportView::resizeGridRows() does this on
  // every ordinary window resize). After growing, the stored bottom
  // stayed pinned at the old row, so the new physical bottom row was no
  // longer recognised as the margin and LF there silently stopped
  // scrolling. The fix restores the dynamic std::numeric_limits<int>::max()
  // sentinel instead of a concrete row number, so scrollBottom() tracks
  // arrayHeight() automatically - this test narrows, resets, resizes
  // *larger*, and checks that the new last row still scrolls.
  KomportSerial serial;
  KomportCellArray cellArray; // 80x25 by default
  KomportEmulation emu(&serial, &cellArray);

  auto sendCsi = [&](const QByteArray &params, char finalByte) {
    emu.slotReceivedChar(0x1B);
    emu.slotReceivedChar('[');
    for ( char c : params ) emu.slotReceivedChar(c);
    emu.slotReceivedChar(finalByte);
  };

  sendCsi("5;10", 'r'); // narrow the region first
  sendCsi("", 'r');     // reset to full screen ("CSI r", no params)

  cellArray.setArraySize( QSize(80, 30) ); // grow, as a window resize would

  QSignalSpy fullScreenSpy( &cellArray, &KomportCellArray::aboutToScrollUp );
  sendCsi("30;1", 'H'); // the *new* last row (0-based 29)
  emu.slotReceivedChar(0x0A);
  QCOMPARE( fullScreenSpy.count(), 1 );
}

void TstEmulation::printableAutoWrapRespectsScrollRegion()
{
  // Codex review finding: printing a character used to always advance via
  // KomportCellArray::advanceCursor(), which has no scroll-region concept
  // at all - so a host reaching the region's bottom margin via plain
  // column wrap (instead of an explicit LF/Index) escaped the region
  // entirely. Two failure shapes, both covered here: (1) region bottom
  // above the physical bottom - wrap used to fall through into rows below
  // the region; (2) region bottom AT the physical bottom but top > 0 -
  // wrap used to go through whole-screen scrollUp(), pulling rows above
  // the region into scrollback that a region-confined scroll must not
  // touch.
  KomportSerial serial;
  KomportCellArray cellArray; // 80x25 by default
  KomportEmulation emu(&serial, &cellArray);

  auto sendCsi = [&](const QByteArray &params, char finalByte) {
    emu.slotReceivedChar(0x1B);
    emu.slotReceivedChar('[');
    for ( char c : params ) emu.slotReceivedChar(c);
    emu.slotReceivedChar(finalByte);
  };

  // Shape 1: region bottom (row 10, 0-based 9) strictly above the last
  // physical row (24).
  cellArray.drawChar(QChar('T'), 0, 3);  // sentinel above the region
  cellArray.drawChar(QChar('B'), 0, 11); // sentinel below the region
  sendCsi("5;10", 'r'); // region rows 5-10 (1-based) -> 0-based [4,9]

  {
    QSignalSpy scrollSpy( &cellArray, &KomportCellArray::aboutToScrollUp );
    sendCsi("10;80", 'H'); // region's bottom row, last column (0-based (79,9))
    emu.slotReceivedChar('X'); // draws at (79,9), then wraps

    QCOMPARE( cellArray.cursor(), QPoint(0,9) ); // wrapped, stayed at the bottom margin
    QCOMPARE( scrollSpy.count(), 0 );            // region-confined scroll, not scrollback
    QCOMPARE( cellArray.cell(0,3)->character(), QChar('T') );  // untouched
    QCOMPARE( cellArray.cell(0,11)->character(), QChar('B') ); // untouched
  }

  // Shape 2: region bottom AT the physical last row (24), but top (4) > 0.
  sendCsi("5;25", 'r'); // region rows 5-25 (1-based) -> 0-based [4,24]
  {
    QSignalSpy scrollSpy( &cellArray, &KomportCellArray::aboutToScrollUp );
    sendCsi("25;80", 'H'); // region's bottom row, last column (0-based (79,24))
    emu.slotReceivedChar('Y');

    QCOMPARE( cellArray.cursor(), QPoint(0,24) );
    QCOMPARE( scrollSpy.count(), 0 ); // must NOT pull row 3 into scrollback
    QCOMPARE( cellArray.cell(0,3)->character(), QChar('T') ); // still untouched
  }
}

void TstEmulation::insertAndDeleteLineRespectScrollRegion()
{
  // Codex review finding: doInsertLine()/doDeleteLine() (CSI L/M) predate
  // DECSTBM and always operated on the whole physical screen - once a
  // program narrows the scroll region, they must only affect rows between
  // the cursor and the region's bottom margin, and must no-op when the
  // cursor is outside the region entirely.
  KomportSerial serial;
  KomportCellArray cellArray; // 80x25 by default
  KomportEmulation emu(&serial, &cellArray);

  auto sendCsi = [&](const QByteArray &params, char finalByte) {
    emu.slotReceivedChar(0x1B);
    emu.slotReceivedChar('[');
    for ( char c : params ) emu.slotReceivedChar(c);
    emu.slotReceivedChar(finalByte);
  };

  cellArray.drawChar(QChar('T'), 0, 3);  // sentinel above the region
  cellArray.drawChar(QChar('B'), 0, 11); // sentinel below the region
  sendCsi("5;10", 'r'); // region rows 5-10 (1-based) -> 0-based [4,9]

  // Mark row 9 (the region's bottom row) so a wrongly-full-screen insert
  // shifting it down past row 10 (into the untouched sentinel's row) would
  // be observable.
  cellArray.drawChar(QChar('9'), 0, 9);

  sendCsi("5;1", 'H'); // cursor to the region's top row (0-based (0,4))
  sendCsi("1", 'L');   // insert 1 line

  QCOMPARE( cellArray.cell(0,3)->character(), QChar('T') );  // untouched, above region
  QCOMPARE( cellArray.cell(0,11)->character(), QChar('B') ); // untouched, below region
  QCOMPARE( cellArray.cell(0,4)->character(), QChar(' ') );  // new blank line at the top
  QCOMPARE( cellArray.cell(0,9)->character(), QChar(' ') );  // '9' pushed off the *region's*
                                                               // bottom, not into row 10

  // Cursor outside the region must be a no-op.
  cellArray.drawChar(QChar('4'), 0, 4); // re-mark row 4 to detect any unwanted shift
  sendCsi("12;1", 'H'); // row 12 (0-based 11) - below the region
  sendCsi("1", 'L');
  QCOMPARE( cellArray.cell(0,4)->character(), QChar('4') );  // region untouched
  QCOMPARE( cellArray.cell(0,11)->character(), QChar('B') ); // no-op, 'B' still there

  // CSI M (delete line), symmetric check: mark the region's top row, cursor
  // there, delete - row must pull up from *within* the region only.
  cellArray.drawChar(QChar('4'), 0, 4);
  cellArray.drawChar(QChar('5'), 0, 5);
  sendCsi("5;1", 'H');
  sendCsi("1", 'M'); // delete 1 line at the region's top row

  QCOMPARE( cellArray.cell(0,4)->character(), QChar('5') );  // row 5 pulled up into row 4
  QCOMPARE( cellArray.cell(0,9)->character(), QChar(' ') );  // region's bottom row now blank
  QCOMPARE( cellArray.cell(0,3)->character(), QChar('T') );  // still untouched
  QCOMPARE( cellArray.cell(0,11)->character(), QChar('B') ); // still untouched
}

void TstEmulation::deviceAttributesRecognizesCsiForm()
{
  // Same rationale as escZRespondsLikeDeviceAttributes() above: the serial
  // port is never opened here, so the actual VT220 reply bytes ("\x1b[?62c")
  // aren't observable through this unit test - it only verifies that
  // "CSI c" and "CSI 0c" are recognised as complete, handled sequences
  // (routed to doDeviceAttributes(), now answering as VT220 - see
  // TODO.md Milestone 4) rather than left dangling or crashing.
  KomportSerial serial;
  KomportCellArray cellArray;
  KomportEmulation emu(&serial, &cellArray);

  auto sendCsi = [&](const QByteArray &params, char finalByte) {
    emu.slotReceivedChar(0x1B);
    emu.slotReceivedChar('[');
    for ( char c : params ) emu.slotReceivedChar(c);
    emu.slotReceivedChar(finalByte);
  };

  sendCsi("", 'c');  // CSI c
  sendCsi("0", 'c'); // CSI 0c

  // Must be ready for normal input again right afterwards.
  sendCsi("", 'H'); // cursor home
  QCOMPARE( cellArray.cursor(), QPoint(0, 0) );
}

void TstEmulation::deleteCharClearsWithConfiguredDefaultColors()
{
  // Milestone 5 (color schemes): doDeleteChar() (CSI Pn P) used to clear
  // the columns exposed at the row's end via a direct cell()->clear() with
  // no colors of its own - which, before Milestone 5's KomportCell::clear()
  // signature change, silently fell back to KomportCell's own OS-palette
  // default instead of this array's *configured* default colors. Give the
  // array an obviously-non-palette scheme first, so a wrong fallback would
  // be immediately visible.
  KomportSerial serial;
  KomportCellArray cellArray; // 80x25 by default
  KomportEmulation emu(&serial, &cellArray);

  const QColor scheme_fg(0,255,0);
  const QColor scheme_bg(0,0,0);
  cellArray.setDefaultForegroundColor(scheme_fg);
  cellArray.setDefaultBackgroundColor(scheme_bg);

  auto sendCsi = [&](const QByteArray &params, char finalByte) {
    emu.slotReceivedChar(0x1B);
    emu.slotReceivedChar('[');
    for ( char c : params ) emu.slotReceivedChar(c);
    emu.slotReceivedChar(finalByte);
  };

  // Fill row 0 with 'X', explicitly colored red - so the exposed columns'
  // *previous* content isn't already coincidentally scheme-colored.
  cellArray.setForegroundColor( QColor(255,0,0) );
  for ( int x = 0; x < cellArray.arrayWidth(); ++x ) cellArray.drawChar( QChar('X'), x, 0 );

  sendCsi("0;1", 'H'); // cursor home, row 0 col 0 (0-based (0,0))
  sendCsi("5", 'P');   // delete 5 characters - columns 75-79 get cleared

  for ( int x = cellArray.arrayWidth()-5; x < cellArray.arrayWidth(); ++x ) {
    QCOMPARE( cellArray.cell(x,0)->character(), QChar(' ') );
    QCOMPARE( cellArray.cell(x,0)->foregroundColor(), scheme_fg );
    QCOMPARE( cellArray.cell(x,0)->backgroundColor(), scheme_bg );
  }
}

QTEST_MAIN(TstEmulation)
#include "tst_emulation.moc"
