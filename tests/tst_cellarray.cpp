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

QTEST_MAIN(TstCellArray)
#include "tst_cellarray.moc"
