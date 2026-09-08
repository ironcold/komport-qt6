/***************************************************************************
                          tst_cellarray.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : 2026 (new in the Qt6 port)
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de

    Regression test for KomportCellArray::setArraySize(): a negative
    width/height (reachable via a hand-edited "ScrollBuffer" profile value,
    see TODO.md's Codex-review section) used to make curcnt > newcnt
    overshoot the actual cell count and call QList::takeFirst() on an
    already-empty list.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "komportcellarray.h"

#include <QTest>

class TstCellArray : public QObject
{
  Q_OBJECT
private slots:
  void negativeHeightDoesNotCrash();
  void regrowAfterNegativeIsUsable();
  void hugeWidthDoesNotCrashAndGetsClamped();
  void cellRejectsOutOfRangeCoordinatesIndividually();
  void copyIgnoresNullSource();
  void scrollRegionHelpersDoNotCrashOnZeroHeightArray();
  void setDefaultColorsLiveRecolorsMatchingCellsOnly();
  void newlyGrownCellsUseCurrentDefaultColors();
  void explicitColorMatchingSchemeDefaultSurvivesSchemeChange();
};

void TstCellArray::negativeHeightDoesNotCrash()
{
  KomportCellArray arr; // default ctor already calls setArraySize(80, 25)
  QCOMPARE( arr.arraySize(), QSize(80, 25) );

  // This is the exact shape a corrupt "ScrollBuffer=-1" profile value used
  // to produce (KomportView::setScrollBuffer() forwards the profile value
  // straight through as the height). Reaching this line without crashing
  // is the actual assertion - the clamp is inside setArraySize() itself.
  arr.setArraySize( QSize(80, -1) );
  QCOMPARE( arr.arraySize(), QSize(80, 0) );
}

void TstCellArray::regrowAfterNegativeIsUsable()
{
  KomportCellArray arr;
  arr.setArraySize( QSize(80, -1) );
  // Growing back up afterwards must still work normally - the clamp must
  // not leave the array in some permanently-broken internal state.
  arr.setArraySize( QSize(80, 25) );
  QCOMPARE( arr.arraySize(), QSize(80, 25) );
  QVERIFY( arr.cell(0, 0) != nullptr );
  QVERIFY( arr.cell(79, 24) != nullptr );
}

void TstCellArray::hugeWidthDoesNotCrashAndGetsClamped()
{
  KomportCellArray arr;
  // An extreme width alone (height stays small/sane) exercises two things
  // at once, cheaply (the clamped width keeps the actual cell count in
  // the hundreds of thousands, not the ~4.3 billion this would be
  // unguarded):
  //  1. width*height as plain int would itself already overflow computing
  //     this product (1'073'741'823 * 4 is far past INT_MAX) - the
  //     int-overflow guard inside setArraySize() must not let a garbage
  //     newcnt through and reintroduce the same takeFirst()-overshoot
  //     crash the negative-value fix addressed, just via a different
  //     route.
  //  2. mArraySize itself must not be left holding the huge width
  //     unclamped - public methods like size()/arrayWidth() use it
  //     directly, not just the eventual cell count, so a caller that only
  //     checked "did the allocation stay small" could still be handed an
  //     absurd width back out of arraySize().
  arr.setArraySize( QSize(1'073'741'823, 4) ); // ~4.3G cells if fully unguarded
  QVERIFY2( arr.arraySize().width() <= 100000,
            qPrintable(QStringLiteral("width should have been clamped to a sane maximum, got %1")
                           .arg(arr.arraySize().width())) );
  QCOMPARE( arr.arraySize().height(), 4 ); // small to begin with, not itself clamped
  QVERIFY( arr.cell(0, 0) != nullptr );
  QVERIFY( arr.cell(arr.arraySize().width()-1, 3) != nullptr );

  // Must still be usable afterwards for a normal, sane size.
  arr.setArraySize( QSize(80, 25) );
  QCOMPARE( arr.arraySize(), QSize(80, 25) );
  QVERIFY( arr.cell(79, 24) != nullptr );
}

void TstCellArray::cellRejectsOutOfRangeCoordinatesIndividually()
{
  KomportCellArray arr; // 80x25 by default
  QCOMPARE( arr.arraySize(), QSize(80, 25) );

  // cell() used to only bounds-check the *flat* index (arrayWidth()*y+x),
  // not x and y individually - an out-of-range x could still land on an
  // in-bounds flat index as long as y compensated for it, silently
  // returning a cell from a *neighbouring row* instead of the
  // out-of-range signal (nullptr) callers actually expect.
  // cell(-1, 1): 80*1 + (-1) = 79, a "valid" flat index - but that's
  // really row 0's last column, not anything belonging to row 1.
  QCOMPARE( arr.cell(-1, 1), static_cast<KomportCell*>(nullptr) );
  // cell(80, 0): 80*0 + 80 = 80, likewise "valid" - actually row 1,
  // column 0.
  QCOMPARE( arr.cell(80, 0), static_cast<KomportCell*>(nullptr) );
  // Sanity check: those *would* have been non-null if cell() only
  // checked the flat index, proving this isn't just an already-null
  // result for an unrelated reason.
  QVERIFY( arr.cell(79, 0) != nullptr ); // the row-0/row-1 boundary cell cell(-1,1) would have wrongly returned
  QVERIFY( arr.cell(0, 1) != nullptr );  // the row-1/row-2 boundary cell cell(80,0) would have wrongly returned

  // Negative y and y past the last row must be rejected too.
  QCOMPARE( arr.cell(0, -1), static_cast<KomportCell*>(nullptr) );
  QCOMPARE( arr.cell(0, 25), static_cast<KomportCell*>(nullptr) );

  // Still works normally for actually in-range coordinates.
  QVERIFY( arr.cell(0, 0) != nullptr );
  QVERIFY( arr.cell(79, 24) != nullptr );
}

void TstCellArray::copyIgnoresNullSource()
{
  // Defensive fix: copy() dereferenced _other unconditionally. No current
  // call site actually passes a null cell (their loop bounds keep it
  // in-range), but cell() (see the tests above) does hand back nullptr
  // for any out-of-range coordinate, so this is one dereference away from
  // a crash if that ever changes. Not observed as a live bug, just closed
  // to match this codebase's existing defense-in-depth style elsewhere.
  KomportCellArray arr;
  KomportCell *target = arr.cell(0, 0);
  QVERIFY( target != nullptr );
  target->setCharacter( QChar('X') );

  target->copy(nullptr); // must not crash

  QCOMPARE( target->character(), QChar('X') ); // untouched
}

void TstCellArray::scrollRegionHelpersDoNotCrashOnZeroHeightArray()
{
  // Codex review finding (Milestone 4, DECSTBM scroll regions): with
  // arrayHeight()==0, qBound(0, x, arrayHeight()-1) == qBound(0, x, -1)
  // still clamps to 0 (qBound with max < min just returns min), so the
  // unguarded original scrollUpRegion()/scrollDownRegion() would call
  // clearRow(0) -> cell(x,0)->clear() with cell() correctly returning
  // nullptr for a 0-height array - a null-pointer dereference. Not
  // reachable via the live KomportView grid today (resizeGridRows() never
  // lets it shrink below 1 row), but these are public methods with no
  // such guarantee, so both need to be safe on their own.
  KomportCellArray arr;
  arr.setArraySize( QSize(80, 0) );
  QCOMPARE( arr.arrayHeight(), 0 );

  arr.scrollUpRegion(0, 0);   // must not crash
  arr.scrollDownRegion(0, 0); // must not crash

  // And the array must still be usable normally afterwards.
  arr.setArraySize( QSize(80, 25) );
  QCOMPARE( arr.arrayHeight(), 25 );
  QVERIFY( arr.cell(0, 0) != nullptr );
}

void TstCellArray::setDefaultColorsLiveRecolorsMatchingCellsOnly()
{
  // Milestone 5 (Appearance tab / color schemes): setDefaultForegroundColor()/
  // setDefaultBackgroundColor() must immediately recolor cells that were
  // showing the *previous* default (so switching schemes is visible right
  // away without waiting for a clear/reset), but must NOT touch a cell a
  // host explicitly colored via SGR to some other color.
  KomportCellArray arr; // 80x25, default colors from QApplication::palette()

  // Two cells at the (old) default color, one explicitly colored away from it.
  arr.clear(); // every cell starts at the current default fg/bg
  arr.cell(0,0)->setForegroundColor( QColor(255,0,0) ); // explicit SGR red - must survive

  const QColor newFg(0,255,0);
  const QColor newBg(0,0,128);
  arr.setDefaultForegroundColor(newFg);
  arr.setDefaultBackgroundColor(newBg);

  QCOMPARE( arr.defaultForegroundColor(), newFg );
  QCOMPARE( arr.defaultBackgroundColor(), newBg );
  // cell(1,0) was at the old default - recolored live.
  QCOMPARE( arr.cell(1,0)->foregroundColor(), newFg );
  QCOMPARE( arr.cell(1,0)->backgroundColor(), newBg );
  // cell(0,0)'s explicit red foreground must survive the scheme change...
  QCOMPARE( arr.cell(0,0)->foregroundColor(), QColor(255,0,0) );
  // ...but its background (never explicitly set, still at the old default)
  // still gets recolored like any other untouched cell.
  QCOMPARE( arr.cell(0,0)->backgroundColor(), newBg );

  // A cell drawn *after* the scheme change must also pick up the new
  // default, not the old one - setDefaultForegroundColor()/
  // setDefaultBackgroundColor() also update the "current SGR color" used
  // for newly drawn characters when it was still at the old default.
  arr.drawChar( QChar('X'), 5, 5 );
  QCOMPARE( arr.cell(5,5)->foregroundColor(), newFg );
  QCOMPARE( arr.cell(5,5)->backgroundColor(), newBg );
}

void TstCellArray::newlyGrownCellsUseCurrentDefaultColors()
{
  // Milestone 5: a cell freshly created by setArraySize() (growing the
  // array, or first construction) used to always start out colored from
  // KomportCell's own ctor default (QApplication::palette()) regardless of
  // what this array's *current* default colors actually are - so growing
  // the grid (e.g. a window resize) after a color scheme was applied would
  // add new rows in the wrong (OS palette) colors instead of the
  // configured scheme.
  KomportCellArray arr;
  const QColor scheme_fg(0,255,0);
  const QColor scheme_bg(0,0,0);
  arr.setDefaultForegroundColor(scheme_fg);
  arr.setDefaultBackgroundColor(scheme_bg);

  arr.setArraySize( QSize(80, 30) ); // grow - new rows 25-29 are brand new cells

  QCOMPARE( arr.cell(0,29)->foregroundColor(), scheme_fg );
  QCOMPARE( arr.cell(0,29)->backgroundColor(), scheme_bg );
}

void TstCellArray::explicitColorMatchingSchemeDefaultSurvivesSchemeChange()
{
  // Codex review finding (Milestone 5, gpt-5.6-sol round): the original
  // live-recolor logic inferred "is this cell still at the default color"
  // by comparing QColor values - which breaks the moment a color scheme's
  // own default happens to equal a real SGR color a host explicitly sent
  // (e.g. "Green on Black"'s default background is the same black SGR 40
  // produces - see the color-scheme table in settingsdialog.cpp).
  // Reproduces exactly that collision: the scheme default background
  // starts black, a character is drawn with an *explicit* SGR-style black
  // background (numerically the same value), then the scheme changes to a
  // different background - the explicitly-black cell must NOT be swept
  // along with the genuinely-still-default ones.
  KomportCellArray arr;
  arr.setDefaultBackgroundColor( QColor(0,0,0) ); // scheme default: black
  arr.clear(); // every cell now at the (black) default

  // Simulate "CSI 40 m" (explicit black background) followed by a
  // character - the same setBackgroundColor()-then-drawChar() path
  // KomportEmulation::doGraphics()/slotReceivedChar() use for a real SGR
  // sequence.
  arr.setBackgroundColor( QColor(0,0,0) ); // explicit, even though it equals the default
  arr.drawChar( QChar('X'), 0, 0 );

  QVERIFY2( !arr.cell(0,0)->backgroundIsDefault(),
            "an explicitly-set SGR color must not be tracked as \"default\", "
            "even when it happens to equal the current default color" );

  // The scheme changes: default background moves from black to blue.
  arr.setDefaultBackgroundColor( QColor(0,0,255) );

  // The explicitly-black cell must stay black...
  QCOMPARE( arr.cell(0,0)->backgroundColor(), QColor(0,0,0) );
  // ...while a genuinely still-default cell (never drawn on) correctly
  // picks up the new scheme color.
  QCOMPARE( arr.cell(1,0)->backgroundColor(), QColor(0,0,255) );
}

QTEST_MAIN(TstCellArray)
#include "tst_cellarray.moc"
