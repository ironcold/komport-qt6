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

QTEST_MAIN(TstCellArray)
#include "tst_cellarray.moc"
