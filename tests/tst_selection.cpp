/***************************************************************************
                          tst_selection.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : 2026 (new in the Qt6 port)
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de

    Regression test for KomportView::select()/deselect(): while scrolled
    back into history, screen position (x,y) can resolve to a cell in the
    scroll buffer rather than the live cell array (see getCell()) - but
    select()/deselect() mutated/read the live array directly, so selecting
    and copying text while scrolled back silently grabbed the wrong
    content, and could leave a stale, invisible "selected" flag behind on
    a live-array cell (see TODO.md's Codex-review section, fourth full
    review).
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "komport.h"
#include "komportview.h"
#include "komportcellarray.h"
#include "komportemulation.h"
#include "komportminimap.h"

#include <QTest>
#include <QPointer>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QDir>

class TstSelection : public QObject
{
  Q_OBJECT
private slots:
  void initTestCase();
  void cleanupTestCase();

  void selectWhileScrolledBackReadsScrollBufferNotLiveGrid();
  void cellAtHistoryRowMatchesFullyScrolledBackGetCell();
  void oldestRowStillReadableOnceScrollBufferIsSaturated();

private:
  QString mTestConfigFile;
};

void TstSelection::initTestCase()
{
  // Same reasoning as the other KomportApp-based tests: keep this off the
  // real user's "Komport-Qt6" profiles entirely.
  QCoreApplication::setOrganizationName( QStringLiteral("Komport-Qt6-Test-Selection") );
  QCoreApplication::setApplicationName( QStringLiteral("Komport-Qt6-Test-Selection") );
  mTestConfigFile = QSettings().fileName();
}

void TstSelection::cleanupTestCase()
{
  const QString dirPath = QFileInfo(mTestConfigFile).absolutePath();
  QFile::remove(mTestConfigFile);
  QDir().rmdir(dirPath);
}

void TstSelection::selectWhileScrolledBackReadsScrollBufferNotLiveGrid()
{
  KomportApp *win = new KomportApp();
  win->show();
  QCoreApplication::processEvents(); // let the initial resizeEvent() actually run
  QPointer<KomportApp> guard(win);

  KomportView *view = win->findChild<KomportView *>();
  QVERIFY( view != nullptr );
  KomportMinimapScrollBar *scrollBar = view->findChild<KomportMinimapScrollBar *>( QStringLiteral("scroll_bar") );
  QVERIFY( scrollBar != nullptr );

  const int rows = view->cellArray()->arrayHeight();
  QVERIFY( rows > 0 );

  // Write more lines than fit on screen, each starting with a distinct
  // character, through the real emulation (char, then CR, then LF) - this
  // is what actually drives KomportCellArray::scrollUp() and, via its
  // aboutToScrollUp()/scrolledUp() signals, copies the pushed-off-the-top
  // rows into the scroll buffer (KomportView::slotAboutToScrollUp()) -
  // exactly the mechanism a real host filling the screen would trigger.
  const int totalLines = rows + 5;
  for ( int i = 0; i < totalLines; ++i ) {
    const char rowChar = 'A' + (i % 26);
    view->mEmulation->slotReceivedChar(rowChar);
    view->mEmulation->slotReceivedChar('\r');
    view->mEmulation->slotReceivedChar('\n');
  }

  // Scroll all the way back to the top of the history.
  scrollBar->setValue(0);
  QVERIFY2( scrollBar->value() < scrollBar->maximum(),
            "test setup needs the view to actually be scrolled back into history" );

  // Sanity-check the test setup itself: screen position (0,0) must now
  // resolve to *different* content than the live grid's row 0 - otherwise
  // this test can't actually distinguish the bug from the fix. (Note: the
  // "offscreen" QPA platform used for these tests shows new windows at
  // their minimum size, so the live grid ends up only a few rows tall -
  // resizeGridRows() itself pushes rows into the scroll buffer as part of
  // shrinking to that size, on top of the ones pushed by writing more
  // lines than fit. That's fine here: this test only needs *some*
  // distinct scroll-buffer content at (0,0), not a specific one.)
  const QChar scrollBufferChar = view->getCell(0, 0)->character();
  const QChar liveGridChar = view->cellArray()->cell(0, 0)->character();
  QVERIFY2( scrollBufferChar != liveGridChar,
            "test setup didn't produce distinct scroll-buffer vs. live-grid "
            "content at (0,0) - can't exercise the bug this way" );
  const QChar scrollBufferChar1 = view->getCell(1, 0)->character(); // for the selectedText() check below

  // The actual regression: select() at screen position (0,0) while
  // scrolled back must read/flag the scroll-buffer cell that's actually
  // visible there, not the live grid's cell at the same raw coordinate.
  // clip=true to exercise the same code path real usage does (it also
  // sets the X11 "Selection" clipboard, which isn't asserted on directly
  // since the "offscreen" QPA platform used for these tests doesn't
  // support that clipboard mode at all - platform limitation, not a code
  // bug). selectedText(), unlike QClipboard::Selection, *is* directly
  // checkable regardless of platform (that's the whole point of it - see
  // its doc comment in komportview.h) and is what actually proves
  // select() operated on the same cell that assembled the copied text.
  // start != end: select() treats an equal start/end as "no selection at
  // all" (a zero-size drag), so (0,0) alone would never even reach the
  // code path under test.
  view->select( QPoint(0,0), QPoint(1,0), true );

  QVERIFY2( view->getCell(0,0)->select(),
            "the visible (scroll-buffer) cell should be flagged selected" );
  QVERIFY2( !view->cellArray()->cell(0,0)->select(),
            "the live grid's cell at the same raw coordinate must not be "
            "flagged selected - it was never actually selected on screen" );
  QCOMPARE( view->selectedText(), QString(scrollBufferChar) + QString(scrollBufferChar1) + QChar('\n') );

  // deselect() must clear the scroll-buffer cell's flag too (not just
  // sweep the live grid, which would leave this stale forever).
  view->deselect();
  QVERIFY( !view->getCell(0,0)->select() );
  QVERIFY( view->selectedText().isEmpty() );

  win->close();
  QTRY_VERIFY( guard.isNull() );
}

void TstSelection::cellAtHistoryRowMatchesFullyScrolledBackGetCell()
{
  // Regression test for cellAtHistoryRow()'s off-by-one: it used to skip
  // the actual oldest scrollback row and return a permanently-blank row
  // at the newest end instead of real content (see TODO.md's Codex-review
  // section, sixth full review) - visible directly in the minimap
  // silhouette and its hover preview, both of which read history rows
  // exclusively through this function.
  //
  // getCell()'s own scroll-buffer indexing was independently verified
  // correct in an earlier review round (it's exercised, among other
  // things, by selectWhileScrolledBackReadsScrollBufferNotLiveGrid()
  // above), so rather than re-deriving mScrollBuffer's raw row layout
  // here too, this checks cellAtHistoryRow() against it directly: with
  // the view scrolled all the way back (scrollbar value 0), viewport row
  // _y and absolute history row _row=_y denote the exact same on-screen
  // position by definition, so getCell(x, row) and cellAtHistoryRow(x,
  // row) must return cells with identical content for every row that's
  // actually in the scroll buffer (row < depth).
  KomportApp *win = new KomportApp();
  win->show();
  QCoreApplication::processEvents();
  QPointer<KomportApp> guard(win);

  KomportView *view = win->findChild<KomportView *>();
  QVERIFY( view != nullptr );
  KomportMinimapScrollBar *scrollBar = view->findChild<KomportMinimapScrollBar *>( QStringLiteral("scroll_bar") );
  QVERIFY( scrollBar != nullptr );

  const int rows = view->cellArray()->arrayHeight();
  const int totalLines = rows + 5;
  for ( int i = 0; i < totalLines; ++i ) {
    const char rowChar = 'A' + (i % 26);
    view->mEmulation->slotReceivedChar(rowChar);
    view->mEmulation->slotReceivedChar('\r');
    view->mEmulation->slotReceivedChar('\n');
  }

  scrollBar->setValue(0); // fully scrolled back
  const int depth = scrollBar->maximum();
  QVERIFY2( depth > 0, "test setup needs actual scrollback content" );

  for ( int row = 0; row < depth; ++row ) {
    KomportCell *viaHistory = view->cellAtHistoryRow(0, row);
    KomportCell *viaGetCell = view->getCell(0, row);
    QVERIFY( viaHistory != nullptr );
    QVERIFY( viaGetCell != nullptr );
    QCOMPARE( viaHistory->character(), viaGetCell->character() );
  }

  // Out-of-range rows must still be rejected cleanly.
  QCOMPARE( view->cellAtHistoryRow(0, -1), static_cast<KomportCell*>(nullptr) );
  QCOMPARE( view->cellAtHistoryRow(0, view->totalHistoryRows()), static_cast<KomportCell*>(nullptr) );

  win->close();
  QTRY_VERIFY( guard.isNull() );
}

void TstSelection::oldestRowStillReadableOnceScrollBufferIsSaturated()
{
  // Regression test for the saturated-scroll-buffer edge case: once the
  // scroll buffer has accumulated at least as many pushed rows as it has
  // capacity for, KomportScrollBuffer::depth() used to report one row
  // more than actually has real content (see komportscrollbuffer.cpp) -
  // arithmetic keyed off depth() in both getCell() and
  // cellAtHistoryRow() then computed an out-of-range negative index for
  // the oldest row and silently returned nullptr (rendered as blank in
  // the minimap) instead of the real content still sitting right there.
  // This only shows up once the buffer is genuinely full, which is why
  // none of the other tests here (which only push a handful of rows)
  // caught it.
  KomportApp *win = new KomportApp();
  win->show();
  QCoreApplication::processEvents();
  QPointer<KomportApp> guard(win);

  KomportView *view = win->findChild<KomportView *>();
  QVERIFY( view != nullptr );
  KomportMinimapScrollBar *scrollBar = view->findChild<KomportMinimapScrollBar *>( QStringLiteral("scroll_bar") );
  QVERIFY( scrollBar != nullptr );

  const int rows = view->cellArray()->arrayHeight();
  // The "Default" profile's ScrollBuffer is 1024 (see komport.cpp) -
  // push comfortably past that so the scroll buffer is definitely
  // saturated, not just close to it, without needing to know its exact
  // configured size here. (applyConnectionSettings() separately clamps
  // ScrollBuffer to at most 4096 regardless of what a profile requests -
  // that's a much higher ceiling than this test needs to clear.)
  const int totalLines = rows + 1500;
  for ( int i = 0; i < totalLines; ++i ) {
    const char rowChar = 'A' + (i % 26);
    view->mEmulation->slotReceivedChar(rowChar);
    view->mEmulation->slotReceivedChar('\r');
    view->mEmulation->slotReceivedChar('\n');
  }

  scrollBar->setValue(0); // fully scrolled back
  const int depthBeforeResize = scrollBar->maximum();
  QVERIFY2( depthBeforeResize > 1000, "test setup needs a genuinely saturated scroll buffer" );

  // Saturating the buffer purely by scrolling can't reproduce the bug on
  // its own: KomportCellArray::scrollUp() (which KomportScrollBuffer::
  // scrollUp() calls into) shrinks the array by one row and immediately
  // grows it back by one, and both of those setArraySize() calls dispatch
  // virtually back into KomportScrollBuffer::setArraySize() - so every
  // scrollUp() re-clamps depth() against arrayHeight() twice, which keeps
  // depth() pinned at arrayHeight()-1 no matter how long the session runs,
  // regardless of whether the cap constant here is arrayHeight() or
  // arrayHeight()-1. The real, user-reachable trigger is a *profile
  // switch* (or a Settings change) that resizes the scroll buffer via a
  // single, un-paired setArraySize() call - e.g. switching to a profile
  // whose configured ScrollBuffer size happens to equal the current
  // depth(). setScrollBuffer() below simulates exactly that: resize to
  // depthBeforeResize itself, so the unfixed clamp ("if depth >
  // arrayHeight()") sees depth == the *new* arrayHeight() and leaves it
  // unchanged, landing depth() == arrayHeight() exactly - the
  // out-of-range state cellAtHistoryRow()/getCell() can't index into.
  view->setScrollBuffer( depthBeforeResize );
  scrollBar->setValue(0); // fully scrolled back again after the resize

  // The actual assertion: the oldest history row must still be real
  // content, not the nullptr a negative index would have produced.
  KomportCell *oldestViaHistory = view->cellAtHistoryRow(0, 0);
  KomportCell *oldestViaGetCell = view->getCell(0, 0);
  QVERIFY2( oldestViaHistory != nullptr,
            "cellAtHistoryRow(0, 0) returned nullptr for the oldest row "
            "after a scroll-buffer resize lands depth() == arrayHeight()" );
  QVERIFY2( oldestViaGetCell != nullptr,
            "getCell(0, 0) returned nullptr for the oldest row after a "
            "scroll-buffer resize lands depth() == arrayHeight(), at full "
            "scrollback" );
  QCOMPARE( oldestViaHistory->character(), oldestViaGetCell->character() );

  win->close();
  QTRY_VERIFY( guard.isNull() );
}

QTEST_MAIN(TstSelection)
#include "tst_selection.moc"
