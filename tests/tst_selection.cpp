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

  // The actual regression: select() at screen position (0,0) while
  // scrolled back must read/flag the scroll-buffer cell that's actually
  // visible there, not the live grid's cell at the same raw coordinate.
  // clip=true to exercise the same code path real usage does (it also
  // sets the X11 "Selection" clipboard) - not asserted on directly, since
  // the "offscreen" QPA platform used for these tests doesn't support
  // that clipboard mode at all (QApplication::clipboard()->text(Selection)
  // reads back empty regardless of what was set, platform limitation, not
  // a code bug); the cell-identity checks below are what actually prove
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

  // deselect() must clear the scroll-buffer cell's flag too (not just
  // sweep the live grid, which would leave this stale forever).
  view->deselect();
  QVERIFY( !view->getCell(0,0)->select() );

  win->close();
  QTRY_VERIFY( guard.isNull() );
}

QTEST_MAIN(TstSelection)
#include "tst_selection.moc"
