/***************************************************************************
                tst_replayreplyui.cpp  -  Komport M10 step 4, view level
                             ------------------------
    begin                : 2026-09-20
    copyright            : (C) 2026 by Mike Sharkey
    email                : mike@mrjdesigns.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// M10 step 4 at the view level (SPEC-M10 section 11, sections 5.7/5.8): the plan
// requires the replay path to be driven through KomportView::slotReplayReceivedChar()
// and to render exactly like live input, and requires the key-input suppression to
// be proven with a real key event through the view.
//
// Builds a real KomportApp offscreen - that is what the view needs, since it reaches
// its document through its application parent - and uses a pty as the peer.

#include "komport.h"
#include "komportcell.h"
#include "komportminimap.h"
#include "komportcellarray.h"
#include "komportdoc.h"
#include "komportserial.h"
#include "komportview.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QScopeGuard>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <QByteArray>
#include <QApplication>
#include <QFont>
#include <QMouseEvent>
#include <QDir>
#include <QPoint>
#include <QString>

#include <fcntl.h>
#include <pty.h>
#include <unistd.h>

namespace {

/** Opens a pty and returns the slave name, or an empty string when unavailable. */
QString makePty(int *masterFd)
{
  int master = -1;
  int slave = -1;
  char name[256] = { 0 };
  if (::openpty(&master, &slave, name, nullptr, nullptr) != 0)
    return QString();
  ::close(slave);
  *masterFd = master;
  return QString::fromLocal8Bit(name);
}

/** Opens the window's serial on a pty: the peer whose silence is the evidence. */
bool openPtySerial(KomportSerial *serial, int *masterFd)
{
  const QString name = makePty(masterFd);
  if (name.isEmpty())
    return false;
  ::fcntl(*masterFd, F_SETFL, ::fcntl(*masterFd, F_GETFL, 0) | O_NONBLOCK);
  serial->setDeviceName(name);
  serial->setBaudRate(qint32(9600));
  return serial->open();
}

/** Reads up to @p bytes from the peer within @p timeoutMs; returns what arrived. */
QByteArray readPeerWithTimeout(int masterFd, int bytes, int timeoutMs)
{
  QByteArray received;
  QElapsedTimer clock;
  clock.start();
  while (received.size() < bytes && clock.elapsed() < timeoutMs) {
    char buffer[256] = { 0 };
    const ssize_t got = ::read(masterFd, buffer, sizeof(buffer));
    if (got > 0)
      received.append(buffer, int(got));
    else
      QTest::qWait(10);
  }
  return received;
}

/** What the user can see of a rendered stream: the grid, the cursor and the scroll
  *  position - the three things the replay path must reproduce exactly. */
struct ViewSnapshot
{
  QString text;          // the visible grid
  QString history;       // the whole scrollback + live history, addressed absolutely
  int historyRows = -1;
  QPoint cursor;
  int scrollValue = -1;
  int scrollMaximum = -1;
};

/** The complete history (oldest first), addressed absolutely - the part of the state a
  *  user scrolls through, not just the visible window. */
QString historyText(KomportView *view)
{
  KomportCellArray *cells = view->cellArray();
  QString history;
  const int rows = view->totalHistoryRows();
  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cells->arrayWidth(); ++col) {
      KomportCell *cell = view->cellAtHistoryRow(col, row);
      const QChar character = cell ? cell->character() : QChar();
      history.append(character.isNull() ? QChar(' ') : character);
    }
    history.append(QLatin1Char('\n'));
  }
  return history;
}

ViewSnapshot snapshotOf(KomportView *view)
{
  KomportCellArray *cells = view->cellArray();
  ViewSnapshot snapshot;
  for (int y = 0; y < cells->arrayHeight(); ++y) {
    for (int x = 0; x < cells->arrayWidth(); ++x) {
      KomportCell *cell = cells->cell(x, y);
      const QChar character = cell ? cell->character() : QChar();
      snapshot.text.append(character.isNull() ? QChar(' ') : character);
    }
    snapshot.text.append(QLatin1Char('\n'));
  }
  snapshot.history = historyText(view);
  snapshot.historyRows = view->totalHistoryRows();
  snapshot.cursor = cells->cursor();
  KomportMinimapScrollBar *bar = view->findChild<KomportMinimapScrollBar *>();
  if (bar) {
    snapshot.scrollValue = bar->value();
    snapshot.scrollMaximum = bar->maximum();
  }
  return snapshot;
}

/** The first place two histories differ, reported by row and column with both rows in
  *  full - the point of the exercise is to name *where* the runs diverged, not to show
  *  two grids side by side. */
QString firstDifference(const QString &left, const QString &right, int width)
{
  const int rowWidth = width + 1;   // the histories carry one '\n' per row
  const int limit = qMin(left.size(), right.size());
  for (int at = 0; at < limit; ++at) {
    if (left.at(at) == right.at(at))
      continue;
    const int row = at / rowWidth;
    const int column = at % rowWidth;
    const auto firstContentRow = [rowWidth, width](const QString &history) {
      for (int candidate = 0; candidate * rowWidth < history.size(); ++candidate) {
        const QString text = history.mid(candidate * rowWidth, width).trimmed();
        if (!text.isEmpty())
          return candidate;
      }
      return -1;
    };
    const auto dump = [rowWidth, width](const QString &history, int from, int to) {
      QString out;
      for (int r = qMax(0, from); r <= to; ++r)
        out += QStringLiteral("  [%1] %2\n").arg(r).arg(history.mid(r * rowWidth, width));
      return out;
    };
    return QStringLiteral("row %1, col %2 (offset %3):\nfirst content row: live %4, replay %5\n"
                          "live rows %6..%8:\n%9replay rows %6..%8:\n%10")
        .arg(row)
        .arg(column)
        .arg(at)
        .arg(firstContentRow(left))
        .arg(firstContentRow(right))
        .arg(qMax(0, row - 1))
        .arg(row + 1)
        .arg(dump(left, row - 1, row + 1))
        .arg(dump(right, row - 1, row + 1));
  }
  if (left.size() != right.size())
    return QStringLiteral("identical for %1 characters, then row counts differ: live has %2 rows, "
                          "replay %3")
        .arg(limit)
        .arg(left.size() / rowWidth)
        .arg(right.size() / rowWidth);
  return QStringLiteral("identical");
}

/** The stream both runs are given: printable sentinels, the four queries the spec
  *  names, one private `c` variant, and a cursor move - so rendering, the cursor and
  *  the reply behaviour are all exercised.
  *
  *  Length bound: the stream stays below `KomportSerial`'s RX high-water mark (`rxQueue`,
  *  default 1024). The legacy display path drops the OLDEST bytes of a burst that exceeds
  *  it, before its flush timer runs (SPEC-M8 6.2: the observation stream is loss-free where
  *  the display path is deliberately lossy); a replay of the same bytes renders all of
  *  them, so an over-mark burst would make the live run render less than the replay run for
  *  a reason that has nothing to do with the replay entry point. Comparing the two requires
  *  that the live path delivered every byte it was given - the caller asserts the bound. */
QByteArray replayStream()
{
  QByteArray stream = QByteArrayLiteral("\x1b[2;4Hlive sentinel");
  // Enough complete lines to push content into the scrollback (the row count depends on
  // the geometry, so this is deliberately far more than any test geometry can hold), and
  // the queries spread across them - so a reply, if one were produced, could come from
  // anywhere in the stream rather than only from its first line.
  for (int line = 0; line < 150; ++line) {
    stream += QByteArrayLiteral("L") + QByteArray::number(line) + QByteArrayLiteral("\r\n");
    if (line == 10)
      stream += QByteArrayLiteral("\x1b[5n") + QByteArrayLiteral("\x1b[6n");
    if (line == 75)
      stream += QByteArrayLiteral("\x1b[c") + QByteArrayLiteral("\x1bZ");
    if (line == 120)
      stream += QByteArrayLiteral("\x1b[?1c");
  }
  // Ends mid-line, so the last partially filled row is compared too.
  return stream + QByteArrayLiteral("tail");
}

} // namespace

class TstReplayReplyUi : public QObject
{
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();

  void aReplayedStreamRendersLikeALiveOne();
  void suppressedKeyInputNeitherSignalsNorWrites();
  void suppressedSimulatedKeyInputNeitherSignalsNorWrites();

private:
  QString mTestConfigFile;
};

void TstReplayReplyUi::initTestCase()
{
  // KomportApp reads and writes real QSettings on construction; redirect it to a
  // test-only organization so this test cannot touch a real user's profiles.
  QCoreApplication::setOrganizationName(QStringLiteral("Komport-Qt6-Test"));
  QCoreApplication::setApplicationName(QStringLiteral("Komport-Qt6-Test"));
  mTestConfigFile = QSettings().fileName();
}

void TstReplayReplyUi::cleanupTestCase()
{
  const QString dirPath = QFileInfo(mTestConfigFile).absolutePath();
  QFile::remove(mTestConfigFile);
  QDir().rmdir(dirPath);   // only succeeds if empty - fine, nothing else is created there
}

/** The live run's bytes arrive through the pty and drive the emulation and the view
  *  independently; the replay run gets the same bytes through the view's replay entry.
  *  Rendering, cursor and the scroll bookkeeping must come out identical - and only the
  *  live run may leave a reply on the wire. */
void TstReplayReplyUi::aReplayedStreamRendersLikeALiveOne()
{
  const QByteArray stream = replayStream();
  // The comparison below is only about the replay entry point if the live path delivered
  // every byte it was given: the legacy display path drops the oldest bytes of a burst above
  // KomportSerial's RX high-water mark (rxQueue, default 1024) before its flush timer runs,
  // so an over-mark stream would make live render less than replay for a reason this slice
  // does not own. Keep the bound visible instead of implicit in the stream's length.
  QVERIFY2(stream.size() < 1024,
           qPrintable(QStringLiteral("the stream is %1 bytes; it must stay below the 1024-byte "
                                     "RX high-water mark of the legacy display path").arg(stream.size())));

  KomportApp *liveWindow = new KomportApp();
  auto liveGuard = qScopeGuard([liveWindow]() { delete liveWindow; });
  KomportView *liveView = liveWindow->findChild<KomportView *>();
  QVERIFY(liveView != nullptr);
  // A fixed geometry in both runs: the row count follows the widget height, and a
  // default-sized widget would make the grids incomparable. setTerminalFont() reflows
  // the grid from the current size, which is what the live application does on resize.
  liveWindow->show();
  liveWindow->resize(900, 600);
  liveView->setTerminalFont(QFont(QStringLiteral("monospace"), 10));
  QCoreApplication::processEvents();
  // Count what the scrollback actually does in each run: a promotion happens per scroll
  // event, so if the two runs differ here, the difference is in the bookkeeping rather
  // than in the rendered content.
  int liveScrollEvents = 0;
  int liveReceived = 0;
  QObject::connect(liveView->cellArray(), &KomportCellArray::aboutToScrollUp,
                   liveView, [&liveScrollEvents]() { ++liveScrollEvents; });
  QObject::connect(liveWindow->getDocument()->getSerial(), &KomportSerial::receivedChar,
                   liveView, [&liveReceived](char) { ++liveReceived; });
  int liveMaster = -1;
  QVERIFY(openPtySerial(liveWindow->getDocument()->getSerial(), &liveMaster));
  auto liveFdGuard = qScopeGuard([&liveMaster]() { if (liveMaster >= 0) ::close(liveMaster); });

  QCOMPARE(::write(liveMaster, stream.constData(), stream.size()), qint64(stream.size()));
  // Wait until the stream has actually ARRIVED, counted per byte on the serial signal - not
  // for a text marker in the visible grid: an earlier version synchronised on a marker and
  // then compared a live render that was still incomplete against a complete replay render,
  // which produced a one-row "divergence" that was a test artefact, not product behaviour.
  // The count is also the evidence for the precondition the byte bound above protects: all
  // stream.size() bytes reached the display path, so both runs were given the same bytes.
  // The strict comparison below stays exactly as it is.
  QTRY_COMPARE_WITH_TIMEOUT(liveReceived, stream.size(), 5000);
  const ViewSnapshot live = snapshotOf(liveView);
  // The live path answers: the queries in the stream reached the peer.
  QVERIFY2(!readPeerWithTimeout(liveMaster, 1, 500).isEmpty(),
           "the live path must answer, otherwise this test cannot observe a reply at all");

  KomportApp *replayWindow = new KomportApp();
  auto replayGuard = qScopeGuard([replayWindow]() { delete replayWindow; });
  KomportView *replayView = replayWindow->findChild<KomportView *>();
  QVERIFY(replayView != nullptr);
  replayWindow->show();
  replayWindow->resize(900, 600);
  replayView->setTerminalFont(QFont(QStringLiteral("monospace"), 10));
  QCoreApplication::processEvents();
  // The comparison is only meaningful with the same geometry, and a resize without a
  // shown window does not reach the view's grid - so the equality is asserted rather
  // than assumed (the earlier run compared a 61-row grid against a 2-row one).
  QCOMPARE(replayView->cellArray()->arrayWidth(), liveView->cellArray()->arrayWidth());
  QCOMPARE(replayView->cellArray()->arrayHeight(), liveView->cellArray()->arrayHeight());
  QCOMPARE(replayView->cellArray()->cellWidth(), liveView->cellArray()->cellWidth());
  QCOMPARE(replayView->cellArray()->cellHeight(), liveView->cellArray()->cellHeight());
  int replayScrollEvents = 0;
  int replayFed = 0;
  QObject::connect(replayView->cellArray(), &KomportCellArray::aboutToScrollUp,
                   replayView, [&replayScrollEvents]() { ++replayScrollEvents; });
  int replayMaster = -1;
  QVERIFY(openPtySerial(replayWindow->getDocument()->getSerial(), &replayMaster));
  auto replayFdGuard = qScopeGuard([&replayMaster]() { if (replayMaster >= 0) ::close(replayMaster); });

  for (char byte : stream)
    { replayView->slotReplayReceivedChar(byte); ++replayFed; }
  const ViewSnapshot replay = snapshotOf(replayView);

  // 1. The rendering, the cursor and the scroll bookkeeping are indistinguishable
  //    from live input (the view follows its output in both runs).
  QVERIFY(!replay.text.isEmpty());
  // The sentinel went into the scrollback with everything else that scrolled off; the
  // visible window keeps the stream's end.
  QVERIFY2(replay.history.contains(QStringLiteral("live sentinel")), "the stream's start is missing");
  QVERIFY2(replay.text.contains(QStringLiteral("tail")), "the visible window must hold the stream's end");
  // The stream really scrolled: otherwise this comparison would prove nothing about the
  // scroll bookkeeping (that was the adversarial round's finding 1).
  // The scrollback is bounded (the project's scroll buffer keeps the newest rows), so
  // "scrolled" means: the history is longer than the window and still holds a line from
  // well before the end - not that its very first row survived.
  QVERIFY2(live.historyRows > liveView->cellArray()->arrayHeight(),
           "the stream must have scrolled into the scrollback");
  QVERIFY2(live.history.contains(QStringLiteral("L100")),
           "a mid-stream line must still be in the scrollback");
  QVERIFY2(live.history.contains(QStringLiteral("tail")),
           "the stream must have been delivered to its end");
  QVERIFY2(replay.history == live.history,
           qPrintable(QStringLiteral("live: %1 bytes, %2 scrolls, %3 rows | replay: %4 bytes, %5 scrolls, %6 rows | stream %7 bytes, %8 lines\n%9")
                          .arg(liveReceived)
                          .arg(liveScrollEvents)
                          .arg(live.historyRows)
                          .arg(replayFed)
                          .arg(replayScrollEvents)
                          .arg(replay.historyRows)
                          .arg(stream.size())
                          .arg(stream.count('\n') + stream.count('\r'))
                          .arg(firstDifference(live.history, replay.history,
                                               liveView->cellArray()->arrayWidth()))));   // whole history
  QVERIFY2(replay.historyRows == live.historyRows,
           qPrintable(QStringLiteral("history rows: live %1, replay %2")
                          .arg(live.historyRows)
                          .arg(replay.historyRows)));
  QVERIFY2(replay.text == live.text,
           qPrintable(firstDifference(live.text, replay.text, liveView->cellArray()->arrayWidth())));         // visible grid
  QCOMPARE(replay.cursor, live.cursor);            // the cursor, mid-line at the end
  QCOMPARE(replay.scrollMaximum, live.scrollMaximum);
  QCOMPARE(replay.scrollValue, replay.scrollMaximum);   // both followed their output
  QCOMPARE(live.scrollValue, live.scrollMaximum);

  // Being scrolled back is not the same state as following: place both views at the top
  // of their history, send one more printable byte to each path, and compare again - the
  // bookkeeping has to follow the new output and reach the same position in both runs.
  // The view's vertical scroll bar is a child of the window, not of the view widget.
  // The view's own vertical scroll bar (a friend is allowed to name it): findChild()
  // picked an unrelated bar of the window, whose maximum is 0.
  auto *liveBar = liveView->mScrollBar;      // the view's own minimap scroll bar
  auto *replayBar = replayView->mScrollBar;
  QVERIFY(liveBar != nullptr && replayBar != nullptr);
  QVERIFY(liveBar->maximum() > 0);
  liveBar->setValue(0);          // 0 is the top of the history (no minimum() on this widget)
  replayBar->setValue(0);
  QCOMPARE(replayBar->value(), 0);
  ::write(liveMaster, "X", 1);
  QTRY_COMPARE_WITH_TIMEOUT(liveReceived, stream.size() + 1, 5000);
  replayView->slotReplayReceivedChar('X');
  const ViewSnapshot liveAfter = snapshotOf(liveView);
  const ViewSnapshot replayAfter = snapshotOf(replayView);
  QVERIFY2(replayAfter.history == liveAfter.history,
           qPrintable(firstDifference(liveAfter.history, replayAfter.history, liveView->cellArray()->arrayWidth())));
  QVERIFY2(replayAfter.text == liveAfter.text,
           qPrintable(firstDifference(liveAfter.text, replayAfter.text, liveView->cellArray()->arrayWidth())));
  QCOMPARE(replayAfter.scrollValue, replayAfter.scrollMaximum);   // both followed the new byte
  QCOMPARE(liveAfter.scrollValue, liveAfter.scrollMaximum);

  // 2. Nothing left the process on the replay path: no reply for any of the five
  //    query forms, in a bounded window rather than a single non-blocking read.
  QCOMPARE(readPeerWithTimeout(replayMaster, 1, 300), QByteArray());
}

/** With key input suppressed, a real key event through the view must neither signal
  *  nor write - and with it enabled, the same event must do both, so the test can
  *  observe the write whose absence it asserts. */
void TstReplayReplyUi::suppressedKeyInputNeitherSignalsNorWrites()
{
  KomportApp *window = new KomportApp();
  auto windowGuard = qScopeGuard([window]() { delete window; });
  KomportView *view = window->findChild<KomportView *>();
  QVERIFY(view != nullptr);
  KomportSerial *serial = window->getDocument()->getSerial();
  int masterFd = -1;
  QVERIFY(openPtySerial(serial, &masterFd));
  auto fdGuard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });

  QSignalSpy pressed(view, &KomportView::keyPressed);
  QSignalSpy written(serial, &KomportSerial::bytesWritten);

  // Something to preserve: a rendered grid with real scrollback to sit in, a selection
  // inside it and a scroll position that is not the bottom.
  // Focus is not a property of an unshown widget in Qt, and the reviewer's requirement is
  // that suppressed input *keeps* the focus - so the window is shown (offscreen, as every
  // test in this project runs) and activated.
  window->show();
  window->activateWindow();
  window->resize(900, 600);
  view->setTerminalFont(QFont(QStringLiteral("monospace"), 10));
  for (char byte : QByteArray(60, 'x')) {
    view->slotReplayReceivedChar(byte);
    view->slotReplayReceivedChar('\r');
    view->slotReplayReceivedChar('\n');
  }
  auto *bar = view->mScrollBar;
  QVERIFY2(bar != nullptr, "the view's scroll bar");
  QVERIFY2(bar->maximum() > 0, "the test needs real scrollback for a non-bottom position");
  // The scroll position is fixed *before* the selection is made, and stays there until after
  // the key event. select() resolves its cells through the scroll-position-aware getCell(),
  // so a scroll between select() and the checks below would compare cells the selection never
  // flagged: an earlier version of this test selected at the bottom, scrolled to the top and
  // then asked getCell(0,0) - which by then resolved into the scroll buffer and turned an
  // assertion that can never hold into an apparent loss of the selection to the key event.
  // State first, then measurement, so that what is compared is comparable.
  bar->setValue(0);
  QCOMPARE(bar->value(), 0);              // 0 is the top of the history
  QVERIFY(bar->value() < bar->maximum()); // and genuinely not the bottom
  // The established selection setup, verbatim from tst_selection: select() in cell
  // coordinates with start != end (an equal pair means "no selection"), and the result is
  // checked through selectedText() and the cell flag - hasSelection() is not what that
  // test asserts on.
  view->select(QPoint(0, 0), QPoint(1, 0), true);
  // The two cells the selection covers, re-resolved the way select() resolved them.
  const auto selectionFlagsOnScreen = [view]() {
    KomportCell *cell = view->getCell(0, 0);
    KomportCell *next = view->getCell(1, 0);
    return cell != nullptr && next != nullptr && cell->select() && next->select();
  };

  view->setKeyInputEnabled(false);
  QCOMPARE(view->keyInputEnabled(), false);
  view->setFocus();
  QTRY_VERIFY_WITH_TIMEOUT(view->hasFocus(), 2000);

  // The snapshot is taken immediately before the event, because the promise is about what the
  // key event changes: a "before" state captured earlier would also cover everything that
  // happened in between (focus acquisition, event processing), and a difference found there
  // would be attributed to the key event it did not come from. Every assertion below compares
  // against exactly this snapshot.
  QVERIFY2(!view->selectedText().isEmpty(), "the test needs a selection to preserve");
  QVERIFY2(selectionFlagsOnScreen(), "the selection must carry its cell flags before the event");
  const QString selectedBefore = view->selectedText();
  const int scrollBefore = bar->value();
  const int scrollMaximumBefore = bar->maximum();

  QTest::keyClick(view, Qt::Key_A);

  QCOMPARE(pressed.count(), 0);        // the view consumed the event without signalling
  QCOMPARE(written.count(), 0);        // and no byte was transmitted
  QCOMPARE(readPeerWithTimeout(masterFd, 1, 200), QByteArray());
  // ... and it consumed it without giving anything up: focus, selection and the scroll
  // position are exactly what they were. The scroll comparisons come first: the cell-flag
  // check below resolves through getCell() and therefore depends on the scroll position, so
  // a mutation that moves it is reported by the scroll comparison - each assertion answers
  // for its own part of the promise.
  QVERIFY(view->hasFocus());
  QCOMPARE(bar->value(), scrollBefore);
  QCOMPARE(bar->maximum(), scrollMaximumBefore);
  QCOMPARE(view->selectedText(), selectedBefore);          // the selection is untouched
  QVERIFY2(selectionFlagsOnScreen(), "the selected cells keep their flag");

  // The enabled control differs in exactly the way the assertions above rely on: it signals,
  // it writes, and it takes the view bookkeeping the suppressed branch returns before - the
  // view follows its output again. Without it, "the scroll position did not move" could hold
  // simply because no path moves it.
  view->setKeyInputEnabled(true);
  QCOMPARE(view->keyInputEnabled(), true);
  QTest::keyClick(view, Qt::Key_A);

  QCOMPARE(pressed.count(), 1);
  QTRY_VERIFY_WITH_TIMEOUT(written.count() >= 1, 2000);
  QVERIFY(!readPeerWithTimeout(masterFd, 1, 500).isEmpty());
  QCOMPARE(bar->value(), bar->maximum());   // the live path follows its output
  QVERIFY(bar->value() > scrollBefore);     // i.e. the suppressed path really skipped that
}


// Review 2026-11-15 finding 1: KomportView::slotSimKeyPressed() is a public slot, so it must be
// as closed as the real key event while key input is suppressed - a direct call reaches neither
// the emulation nor the wire, and moves nothing.
void TstReplayReplyUi::suppressedSimulatedKeyInputNeitherSignalsNorWrites()
{
  KomportApp *window = new KomportApp();
  auto windowGuard = qScopeGuard([window]() { delete window; });
  KomportView *view = window->findChild<KomportView *>();
  QVERIFY(view != nullptr);
  KomportSerial *serial = window->getDocument()->getSerial();
  int masterFd = -1;
  QVERIFY(openPtySerial(serial, &masterFd));
  auto fdGuard = qScopeGuard([&masterFd]() { if (masterFd >= 0) ::close(masterFd); });

  QSignalSpy simulated(view, &KomportView::simKeyPressed);
  QSignalSpy written(serial, &KomportSerial::bytesWritten);

  view->setKeyInputEnabled(false);
  view->slotSimKeyPressed(QLatin1Char('x'));
  view->slotSimKeyPressed(QLatin1Char('\r'));
  QCOMPARE(simulated.count(), 0);
  QCOMPARE(written.count(), 0);
  QCOMPARE(readPeerWithTimeout(masterFd, 1, 200), QByteArray());

  // The emulation's own public entries are closed by the same flag (closing review round 2):
  // direct key and simulated-key calls, and a terminal query arriving on the *live* receive
  // entry with the port open, must neither write nor answer.
  QKeyEvent key(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
  view->mEmulation->slotKeyPressed(&key);
  view->mEmulation->slotSimKeyPressed(QLatin1Char('y'));
  for (const char byte : QByteArray("\x1b[5n\x1b[6n\x1b[c\x1bZ"))
    view->mEmulation->slotReceivedChar(byte);
  QCOMPARE(written.count(), 0);
  QCOMPARE(readPeerWithTimeout(masterFd, 1, 200), QByteArray());

  // The enabled control writes, so the silence above is the guard's doing.
  view->setKeyInputEnabled(true);
  view->mEmulation->slotReceivedChar('\x1b'); view->mEmulation->slotReceivedChar('['); view->mEmulation->slotReceivedChar('5'); view->mEmulation->slotReceivedChar('n');
  QTRY_VERIFY_WITH_TIMEOUT(written.count() >= 1, 2000);
  QVERIFY(!readPeerWithTimeout(masterFd, 1, 500).isEmpty());
  written.clear();
  readPeerWithTimeout(masterFd, 1, 100);
  view->slotSimKeyPressed(QLatin1Char('x'));
  QCOMPARE(simulated.count(), 1);
  QTRY_VERIFY_WITH_TIMEOUT(written.count() >= 1, 2000);
  QVERIFY(!readPeerWithTimeout(masterFd, 1, 500).isEmpty());
}

QTEST_MAIN(TstReplayReplyUi)
#include "tst_replayreplyui.moc"
