/***************************************************************************
                          tst_appearance.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : 2026 (new in the Qt6 port)
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de

    Regression test for Milestone 4 (TODO.md): the Appearance settings tab
    (font/color-scheme persistence). Covers KomportView::saveSettings()/
    loadSettings() round-tripping, and that a profile predating this
    feature (no "Appearance" group at all) still loads cleanly with
    sensible defaults instead of crashing or resetting to something
    nonsensical.

    Codex review finding (gpt-5.6-sol round 2): profileRoundTripsFontAndColors()
    below exercises KomportView::saveSettings()/loadProfile() directly, NOT
    the full KomportApp::saveProfile() -> KomportView::saveSettings() call
    chain (saveProfile() is protected, UI-only - see the test for why). It
    would not catch a regression where production code stopped calling
    view->saveSettings() from saveProfile(), or wrote it into the wrong
    QSettings group; that one-line integration in komport.cpp has to be
    reviewed by eye instead.
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
#include "komportminimap.h"
#include "settingsdialog.h"

#include <QTest>
#include <QPointer>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QApplication>
#include <QPalette>
#include <QSpinBox>
#include <QPainter>
#include <QPixmap>
#include <QImage>

class TstAppearance : public QObject
{
  Q_OBJECT
private slots:
  void initTestCase();
  void cleanupTestCase();

  void profileRoundTripsFontAndColors();
  void profileWithoutAppearanceGroupFallsBackToStableBaseline();
  void scrollbackColorsFollowSchemeChange();
  void settingsDialogFontRoundTripsSpacing();
  void paintCellUsesTerminalFontNotPainterDefault();
  void colorChangeRepaintsAlreadyScrolledBackHistory();
  void sameCharacterRedrawPicksUpNewExplicitColor();

private:
  QString mTestConfigFile;
};

void TstAppearance::initTestCase()
{
  // Same reasoning as tst_profileerror.cpp/tst_windowlifetime.cpp: keep
  // this off the real user's "Komport-Qt6" profiles entirely.
  QCoreApplication::setOrganizationName( QStringLiteral("Komport-Qt6-Test-Appearance") );
  QCoreApplication::setApplicationName( QStringLiteral("Komport-Qt6-Test-Appearance") );
  mTestConfigFile = QSettings().fileName();
}

void TstAppearance::cleanupTestCase()
{
  const QString dirPath = QFileInfo(mTestConfigFile).absolutePath();
  QFile::remove(mTestConfigFile);
  QDir().rmdir(dirPath);
}

void TstAppearance::profileRoundTripsFontAndColors()
{
  KomportApp *win = new KomportApp();
  win->show();
  QPointer<KomportApp> guard(win);

  KomportView *view = win->findChild<KomportView *>();
  QVERIFY( view != nullptr );

  // A font/color combination nothing would produce by accident, so a
  // fallback-to-default bug would be immediately visible.
  QFont scheme_font( QStringLiteral("Monospace") );
  scheme_font.setPointSize( 17 );
  scheme_font.setFixedPitch( true );
  const QColor scheme_fg(0,255,0);
  const QColor scheme_bg(0,0,0);

  view->setTerminalFont( scheme_font );
  view->cellArray()->setDefaultForegroundColor( scheme_fg );
  view->cellArray()->setDefaultBackgroundColor( scheme_bg );

  // KomportApp::saveProfile() itself is protected (only reachable from the
  // UI via slotSaveProfile()/profileCombo, both private) - call
  // KomportView::saveSettings() directly instead, into the same
  // Profiles/<name> group saveProfile() would use. This exercises exactly
  // the method Milestone 5 actually added, without needing UI access.
  {
    QSettings settings;
    settings.beginGroup( QStringLiteral("Profiles/AppearanceRoundTrip") );
    view->saveSettings( &settings );
    settings.endGroup();
  }

  // Change everything away from the saved values, so loadProfile() below
  // actually has to restore something rather than coincidentally matching
  // already-current state.
  QFont other_font( QStringLiteral("Monospace") );
  other_font.setPointSize( 9 );
  view->setTerminalFont( other_font );
  view->cellArray()->setDefaultForegroundColor( QColor(255,255,255) );
  view->cellArray()->setDefaultBackgroundColor( QColor(255,255,255) );

  win->loadProfile( QStringLiteral("AppearanceRoundTrip") );

  QCOMPARE( view->font().family(), scheme_font.family() );
  QCOMPARE( view->font().pointSize(), 17 );
  QCOMPARE( view->cellArray()->defaultForegroundColor(), scheme_fg );
  QCOMPARE( view->cellArray()->defaultBackgroundColor(), scheme_bg );

  win->close();
  QTRY_VERIFY( guard.isNull() );
}

void TstAppearance::profileWithoutAppearanceGroupFallsBackToStableBaseline()
{
  // Codex review finding (gpt-5.6-sol round): a profile saved before
  // Milestone 5 existed (or a built-in vendor preset) has no "Appearance"
  // group under Profiles/<name> at all. The first version of this test
  // asserted that loading such a profile *kept whatever was currently
  // displayed* - which turned out to be exactly the bug Codex flagged:
  // loading a styled profile A, then an old/built-in profile B, made B
  // silently inherit A's appearance (non-deterministic, load-order
  // dependent), and saving B afterwards would even persist that leaked
  // state into it permanently. The fix (see KomportView::loadSettings())
  // falls back to a fixed baseline - the same system fixed-pitch font and
  // OS palette colors the very first, pre-Milestone-5 KomportView
  // constructor always used - regardless of what was loaded before, so
  // this test now asserts *that* stable baseline instead of "whatever was
  // there a moment ago".
  {
    QSettings settings;
    settings.beginGroup( QStringLiteral("Profiles/LegacyNoAppearance") );
    settings.setValue( QStringLiteral("Device"), QStringLiteral("/dev/null") );
    settings.setValue( QStringLiteral("BaudRate"), QStringLiteral("9600") );
    settings.setValue( QStringLiteral("DataBits"), QStringLiteral("8") );
    settings.setValue( QStringLiteral("StartBits"), QStringLiteral("1") );
    settings.setValue( QStringLiteral("StopBits"), QStringLiteral("1") );
    settings.setValue( QStringLiteral("Parity"), QStringLiteral("NONE") );
    settings.setValue( QStringLiteral("FlowControl"), QStringLiteral("NONE") );
    settings.setValue( QStringLiteral("RXQueue"), QStringLiteral("1024") );
    settings.setValue( QStringLiteral("FlushRate"), QStringLiteral("256") );
    settings.setValue( QStringLiteral("ScrollBuffer"), QStringLiteral("1024") );
    settings.setValue( QStringLiteral("LineEnding"), QStringLiteral("CR") );
    // deliberately no "Appearance" subgroup at all
    settings.endGroup();
  }

  KomportApp *win = new KomportApp();
  win->show();
  QPointer<KomportApp> guard(win);

  KomportView *view = win->findChild<KomportView *>();
  QVERIFY( view != nullptr );

  // Apply an obviously-different marker font/color first, so a wrong
  // "inherited from before" fallback would be immediately visible.
  QFont marker_font( QStringLiteral("Monospace") );
  marker_font.setPointSize( 21 );
  view->setTerminalFont( marker_font );
  view->setDefaultColors( QColor(0,255,0), QColor(0,0,0) );

  win->loadProfile( QStringLiteral("LegacyNoAppearance") ); // must not crash, must reset to the stable baseline

  const QFont baselineFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  const QColor baselineFg = QApplication::palette().color(QPalette::Text);
  const QColor baselineBg = QApplication::palette().color(QPalette::Base);

  QCOMPARE( view->font().family(), baselineFont.family() );
  QCOMPARE( view->font().pointSize(), baselineFont.pointSize() );
  QCOMPARE( view->cellArray()->defaultForegroundColor(), baselineFg );
  QCOMPARE( view->cellArray()->defaultBackgroundColor(), baselineBg );

  win->close();
  QTRY_VERIFY( guard.isNull() );
}

void TstAppearance::scrollbackColorsFollowSchemeChange()
{
  // Codex review finding (gpt-5.6-sol round): applying a color scheme only
  // ever touched the live grid (KomportView::cellArray(), i.e. mCellArray)
  // - mScrollBuffer is a *separate* KomportCellArray instance (see
  // komportview.h) and kept its old colors, so scrolling back after a
  // scheme change showed history in the previous scheme while the live
  // screen already showed the new one. KomportView::setDefaultColors()
  // (which this test uses, same as KomportApp::slotShowPreferences()/
  // KomportView::loadSettings()) fixes this by updating both arrays.
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

  // Write more lines than fit on screen through the real emulation, same
  // approach as tst_selection.cpp - this is what actually pushes rows into
  // the scroll buffer.
  const int totalLines = rows + 5;
  for ( int i = 0; i < totalLines; ++i ) {
    const char rowChar = 'A' + (i % 26);
    view->mEmulation->slotReceivedChar(rowChar);
    view->mEmulation->slotReceivedChar('\r');
    view->mEmulation->slotReceivedChar('\n');
  }

  const QColor newBg(0,0,128);
  view->setDefaultColors( QColor(0,255,0), newBg );

  // Scroll all the way back into history and check a scrollback row - not
  // the live grid, which setDefaultColors() was already known to update
  // correctly even before this fix.
  scrollBar->setValue(0);
  QVERIFY2( scrollBar->value() < scrollBar->maximum(),
            "test setup needs the view to actually be scrolled back into history" );

  KomportCell *historyCell = view->getCell(0, 0);
  QVERIFY( historyCell != nullptr );
  QCOMPARE( historyCell->backgroundColor(), newBg );

  win->close();
  QTRY_VERIFY( guard.isNull() );
}

void TstAppearance::settingsDialogFontRoundTripsSpacing()
{
  // Codex review finding: TODO.md's Milestone 5 spec explicitly asks for a
  // font "Spacing" control alongside family/size - the first version of
  // the Appearance tab only exposed the latter two. FontSpacingSpinBox
  // (added afterwards) must round-trip through selectedFont()/
  // setSelectedFont() like the rest of the font.
  SettingsDialog dialog;

  // Deliberately no explicit letterSpacing on the *input* font here - a
  // first draft of this test set it on the input and got a false pass:
  // QFontComboBox::setCurrentFont()/currentFont() turned out to preserve
  // an input font's letterSpacing property on its own, so the test kept
  // passing even with selectedFont()'s own FontSpacingSpinBox wiring
  // temporarily removed. Setting FontSpacingSpinBox directly (as a user
  // would via the UI) instead actually exercises that wiring.
  QFont f( QStringLiteral("Monospace") );
  f.setPointSize( 14 );
  dialog.setSelectedFont( f );
  dialog.FontSpacingSpinBox->setValue( 130 );

  const QFont roundTripped = dialog.selectedFont();
  QCOMPARE( roundTripped.pointSize(), 14 );
  QCOMPARE( roundTripped.letterSpacingType(), QFont::PercentageSpacing );
  QCOMPARE( qRound(roundTripped.letterSpacing()), 130 );
}

void TstAppearance::paintCellUsesTerminalFontNotPainterDefault()
{
  // Codex review finding (gpt-5.6-sol round 2): setTerminalFont() re-derives
  // cell *geometry* from the new font via fontMetrics(), but paintCell()
  // used to start from _paint->font() - the font of whatever QPainter
  // happened to be handed in - rather than this widget's own font(). Since
  // every real caller paints into mPixmap (a QPixmap, which carries no font
  // of its own), that painter's font was always just the application
  // default: family/size/spacing changes altered the grid but never the
  // actual glyphs drawn. Call the (friended, see komportview.h) protected
  // paintCell() directly with a painter deliberately pre-set to a
  // *different* font, so a regression back to _paint->font() would leave
  // that different font in place and fail this test.
  KomportApp *win = new KomportApp();
  win->show();
  QPointer<KomportApp> guard(win);

  KomportView *view = win->findChild<KomportView *>();
  QVERIFY( view != nullptr );

  QFont scheme_font( QStringLiteral("Monospace") );
  scheme_font.setPointSize( 19 );
  scheme_font.setFixedPitch( true );
  view->setTerminalFont( scheme_font );

  QPixmap pm(64, 64);
  QPainter painter(&pm);
  QFont decoyFont( QStringLiteral("Serif") ); // deliberately NOT scheme_font
  decoyFont.setPointSize( 6 );
  painter.setFont( decoyFont );

  view->paintCell( &painter, 0, 0, QRect(0, 0, 64, 64) );

  QCOMPARE( painter.font().family(), scheme_font.family() );
  QCOMPARE( painter.font().pointSize(), 19 );

  win->close();
  QTRY_VERIFY( guard.isNull() );
}

void TstAppearance::colorChangeRepaintsAlreadyScrolledBackHistory()
{
  // Codex review finding (gpt-5.6-sol round 2): scrollbackColorsFollowSchemeChange()
  // above only checks that scrollback *ends up* with the new colors once
  // queried - it changes colors first and scrolls back afterwards, so it
  // never actually caught the repaint-ordering bug: KomportView::setDefaultColors()
  // used to recolor the *live* array first, whose update() is the only one
  // connected to a repaint (mScrollBuffer's own signals go nowhere) - while
  // already scrolled back, that repaint read stale mScrollBuffer colors,
  // since mScrollBuffer itself was only recolored afterwards. This test
  // scrolls back *before* changing colors, so it actually exercises the
  // ordering fix (mScrollBuffer is now updated first in setDefaultColors()).
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

  // Scroll back into history FIRST, before touching colors.
  scrollBar->setValue(0);
  QVERIFY2( scrollBar->value() < scrollBar->maximum(),
            "test setup needs the view to actually be scrolled back into history" );

  const QColor newBg(128,0,0);
  view->setDefaultColors( QColor(0,255,0), newBg );

  // getCell()'s returned *cell data* is correct in both orderings by the
  // time setDefaultColors() returns (both arrays are always fully updated
  // before the call returns) - that's not what the bug was about, and
  // checking only this (as an earlier draft of this test did) passed even
  // with the ordering fix reverted. The bug was specifically that the one
  // repaint this triggers can fire *before* mScrollBuffer has its new
  // colors, permanently baking the stale color into the visible mPixmap
  // until some unrelated later redraw happens to fix it. So check the
  // *actually rendered pixel*, via the (friended, see komportview.h)
  // mPixmap directly, not just the underlying model.
  KomportCell *historyCell = view->getCell(0, 0);
  QVERIFY( historyCell != nullptr );
  QCOMPARE( historyCell->backgroundColor(), newBg );

  const QImage img = view->mPixmap.toImage();
  const int cellW = view->cellArray()->cellWidth();
  const int cellH = view->cellArray()->cellHeight();
  QVERIFY( img.width() >= cellW && img.height() >= cellH );
  QCOMPARE( img.pixelColor( cellW/2, cellH/2 ).rgb(), newBg.rgb() );

  win->close();
  QTRY_VERIFY( guard.isNull() );
}

void TstAppearance::sameCharacterRedrawPicksUpNewExplicitColor()
{
  // Codex review finding (gpt-5.6-sol round 2): KomportCellArray::drawChar()
  // used to skip setCellAttributes() (colors + their isDefault provenance)
  // whenever the character being written matched what was already there -
  // an optimization that broke a common status-line pattern: reposition the
  // cursor, change color, redraw the *same* glyph. Write 'X' in the default
  // color, move back, switch to an explicit SGR foreground color, write 'X'
  // again - the cell must end up with the new color and lose its "default"
  // provenance (foregroundIsDefault() == false), not silently keep the old
  // default color/provenance as if nothing happened.
  KomportApp *win = new KomportApp();
  win->show();
  QPointer<KomportApp> guard(win);

  KomportView *view = win->findChild<KomportView *>();
  QVERIFY( view != nullptr );

  // ESC [ H moves the cursor to the home position (1,1) - used twice below
  // to return to the same cell before each write.
  const QByteArray home = QByteArrayLiteral("\x1b[H");
  const QByteArray red = QByteArrayLiteral("\x1b[31m"); // SGR 31 = red foreground

  for (char c : home) view->mEmulation->slotReceivedChar(c);
  view->mEmulation->slotReceivedChar('X');

  KomportCell *cell = view->cellArray()->cell(0, 0);
  QVERIFY( cell != nullptr );
  QVERIFY2( cell->foregroundIsDefault(), "cell should still be at the default color before the SGR change" );

  for (char c : home) view->mEmulation->slotReceivedChar(c);
  for (char c : red) view->mEmulation->slotReceivedChar(c);
  view->mEmulation->slotReceivedChar('X'); // same glyph, new explicit color

  QVERIFY2( !cell->foregroundIsDefault(), "redrawing the same glyph with an explicit SGR color must clear the default-color provenance flag" );
  QCOMPARE( cell->foregroundColor(), view->cellArray()->foregroundColor() );

  win->close();
  QTRY_VERIFY( guard.isNull() );
}

QTEST_MAIN(TstAppearance)
#include "tst_appearance.moc"
