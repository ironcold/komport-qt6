/***************************************************************************
                          tst_charset.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : 2026 (new in the Qt6 port)
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de

    Regression test for Milestone 7 (TODO.md): retro/industrial byte-level
    character-set translation (KomportCharset). Covers the static lookup-
    table functions directly, plus KomportEmulation::slotReceivedChar()'s
    integration point (the RX direction). The TX direction (slotKeyPressed()/
    slotSimKeyPressed()) is gated behind KomportSerial::isOpen(), same
    limitation already documented for escZRespondsLikeDeviceAttributes() in
    tst_emulation.cpp - no test in this suite opens a real serial port, so
    KomportCharset::toWire() itself (exercised directly below) is the
    practical, reliable way to cover that logic.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "komportcharset.h"
#include "komportemulation.h"
#include "komportcellarray.h"
#include "komportserial.h"
#include "komport.h"
#include "komportview.h"
#include "settingsdialog.h"

#include <QTest>
#include <QPointer>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QComboBox>

class TstCharset : public QObject
{
  Q_OBJECT
private slots:
  void initTestCase();
  void cleanupTestCase();

  void standardCharsetIsIdentity();
  void cp437HighRangeMapsToBoxDrawingAndAccents();
  void cp437LowRangeMapsToControlPictureGlyphs();
  void cp437RoundTripsDistinctiveGlyphsBackToWire();
  void cp437SpaceDoesNotCollideWithNul();
  void petsciiSubstitutesThreePunctuationBytes();
  void petsciiAsciiCompatibleRangePassesThrough();
  void petsciiGraphicsRangeIsDeliberatelyUnmapped();
  void petsciiDisplacedAsciiCharactersDoNotSilentlySendWrongByte();
  void petsciiControlCharactersSurviveMultilinePaste();
  void unrepresentableCharacterOnCp437ReturnsPlaceholderNotWrongByte();
  void nonLatin1CharacterNeverSilentlyBecomesNul();
  void namesAndIndexRoundTrip();
  void settingsKeyRoundTrips();
  void emulationTranslatesReceivedBytesThroughSelectedCharset();
  void emulationControlCodesUnaffectedByNonStandardCharset();
  void emulationDefaultsToStandardCharset();
  void settingsDialogComboBoxStoresIdAsUserRoleNotRowPosition();
  void legacyProfileWithoutCharsetKeyFallsBackToStandard();

private:
  QString mTestConfigFile;
};

void TstCharset::initTestCase()
{
  // Same reasoning as tst_appearance.cpp/tst_windowlifetime.cpp: keep
  // this off the real user's "Komport-Qt6" profiles entirely.
  QCoreApplication::setOrganizationName( QStringLiteral("Komport-Qt6-Test-Charset") );
  QCoreApplication::setApplicationName( QStringLiteral("Komport-Qt6-Test-Charset") );
  mTestConfigFile = QSettings().fileName();
}

void TstCharset::cleanupTestCase()
{
  const QString dirPath = QFileInfo(mTestConfigFile).absolutePath();
  QFile::remove(mTestConfigFile);
  QDir().rmdir(dirPath);
}

void TstCharset::standardCharsetIsIdentity()
{
  // Byte value == Unicode code point, unchanged - exactly the behavior
  // every byte had before Milestone 7 introduced KomportCharset at all
  // (KomportCellArray::drawChar() used to receive the raw `char` directly,
  // which QChar's own char-constructor already treats as a Latin-1 byte).
  for ( int b : { 0x00, 0x41, 0x7F, 0x80, 0xA4, 0xFF } ) {
    QCOMPARE( KomportCharset::toDisplay(KomportCharset::Standard, static_cast<unsigned char>(b)),
              QChar(static_cast<uchar>(b)) );
  }
}

void TstCharset::cp437HighRangeMapsToBoxDrawingAndAccents()
{
  // A handful of well-known, high-confidence CP437 entries.
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::CP437, 0xDB), QChar(0x2588) ); // █ full block
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::CP437, 0xB0), QChar(0x2591) ); // ░ light shade
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::CP437, 0xC9), QChar(0x2554) ); // ╔ double corner
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::CP437, 0x81), QChar(0x00FC) ); // ü
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::CP437, 0xE1), QChar(0x00DF) ); // ß
}

void TstCharset::cp437LowRangeMapsToControlPictureGlyphs()
{
  // Codex review finding, scope correction: the first version of this
  // table mapped 0x00-0x1F/0x7F to CP437's separate "control picture"
  // glyphs (☺♥♦♣♠ etc.), which conflicted with several real VT100 control
  // codes this emulation doesn't special-case (e.g. VT/FF at 0x0B/0x0C,
  // which many real hosts use like LF, would have drawn ♂/♀ and merely
  // advanced the cursor instead of doing a line feed). 0x00-0x7F is now
  // deliberately identity under CP437 too (same as Standard) - this test
  // now asserts *that*, not the old glyph mapping, so a regression back
  // to the control-picture table would be caught.
  for ( int b : { 0x00, 0x01, 0x03, 0x06, 0x0B, 0x0C, 0x7F } ) {
    QCOMPARE( KomportCharset::toDisplay(KomportCharset::CP437, static_cast<unsigned char>(b)),
              QChar(static_cast<uchar>(b)) );
  }
}

void TstCharset::cp437RoundTripsDistinctiveGlyphsBackToWire()
{
  QCOMPARE( KomportCharset::toWire(KomportCharset::CP437, QChar(0x2588)), static_cast<char>(0xDB) );
  QCOMPARE( KomportCharset::toWire(KomportCharset::CP437, QChar(0x00FC)), static_cast<char>(0x81) ); // ü
}

void TstCharset::cp437SpaceDoesNotCollideWithNul()
{
  // Codex review finding: the first version's table mapped *both* 0x00
  // and 0x20 to U+0020 (space) for display-safety reasons (see the old
  // comment, since removed) - cp437Reverse()'s "keep the first insert"
  // tie-break then made 0x00 win for U+0020, so every typed space was
  // silently sent as NUL instead. The 0x00-0x7F scope correction above
  // removes the duplicate at its root (0x00 now maps to U+0000, not
  // U+0020), but this test pins the specific, practically-critical
  // symptom directly so it can never quietly come back.
  QCOMPARE( KomportCharset::toWire(KomportCharset::CP437, QChar(' ')), ' ' );
}

void TstCharset::petsciiSubstitutesThreePunctuationBytes()
{
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::PETSCII, 0x5C), QChar(0x00A3) ); // £
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::PETSCII, 0x5E), QChar(0x2191) ); // ↑
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::PETSCII, 0x5F), QChar(0x2190) ); // ←
  // and the reverse direction
  QCOMPARE( KomportCharset::toWire(KomportCharset::PETSCII, QChar(0x00A3)), static_cast<char>(0x5C) );
  QCOMPARE( KomportCharset::toWire(KomportCharset::PETSCII, QChar(0x2191)), static_cast<char>(0x5E) );
  QCOMPARE( KomportCharset::toWire(KomportCharset::PETSCII, QChar(0x2190)), static_cast<char>(0x5F) );
}

void TstCharset::petsciiAsciiCompatibleRangePassesThrough()
{
  for ( char c : { 'A', 'Z', '0', '9', '@', ' ' } ) {
    QCOMPARE( KomportCharset::toDisplay(KomportCharset::PETSCII, static_cast<unsigned char>(c)), QChar(c) );
  }
}

void TstCharset::petsciiGraphicsRangeIsDeliberatelyUnmapped()
{
  // Documented scope boundary (see KomportCharset::Id::PETSCII's header
  // comment): the CBM-specific block-graphics range isn't implemented in
  // this pass, so it falls back to identity, same as Standard - this test
  // exists to make that an explicit, checked decision rather than a
  // silent gap that could be "fixed" by accident into something wrong.
  for ( int b : { 0x60, 0x7F, 0xA0, 0xFF } ) {
    QCOMPARE( KomportCharset::toDisplay(KomportCharset::PETSCII, static_cast<unsigned char>(b)),
              QChar(static_cast<uchar>(b)) );
  }
}

void TstCharset::petsciiDisplacedAsciiCharactersDoNotSilentlySendWrongByte()
{
  // Codex review finding (round 2): the first version of toWire()'s
  // PETSCII fallback treated "anything <= 0x7F" as safe ASCII identity -
  // wrong, because PETSCII displaces several of those bytes to mean
  // something else entirely. A literal '\' (U+005C) fell through to byte
  // 0x5C, which PETSCII actually displays as £ - the exact
  // silent-wrong-byte bug this function exists to prevent, just via a
  // different path than the one already fixed. Same for '^'/'_'
  // (-> ↑/←), and any lowercase letter (0x61-0x7A sits inside PETSCII's
  // deliberately-unmapped graphics range, not real lowercase text).
  for ( char c : { '\\', '^', '_', 'a', 'z' } ) {
    QCOMPARE( KomportCharset::toWire(KomportCharset::PETSCII, QChar(c)), '?' );
  }
  // ']' (0x5D) sits directly between the displaced bytes and is
  // genuinely ASCII-identical under PETSCII - must still work.
  QCOMPARE( KomportCharset::toWire(KomportCharset::PETSCII, QChar(']')), ']' );
}

void TstCharset::petsciiControlCharactersSurviveMultilinePaste()
{
  // Codex review finding (round 3): narrowing the PETSCII identity range
  // to exactly the printable 0x20-0x5B/0x5D bytes (the fix just above)
  // went too far - it also excluded the C0 control range, so pasting
  // multi-line clipboard text under PETSCII turned every line break into
  // '?' ("line1\nline2" -> "line1?line2"). Same reasoning already applied
  // to CP437's control range (see KomportCharset::Id::CP437): this
  // emulation treats control bytes as identity/literal regardless of the
  // selected charset, it doesn't reinterpret them through a retro
  // charset's own (different) control-code semantics.
  for ( char c : { '\r', '\n', '\t' } ) {
    QCOMPARE( KomportCharset::toWire(KomportCharset::PETSCII, QChar(c)), c );
  }
  QCOMPARE( KomportCharset::toWire(KomportCharset::PETSCII, QChar(0x7F)), static_cast<char>(0x7F) ); // DEL
}

void TstCharset::unrepresentableCharacterOnCp437ReturnsPlaceholderNotWrongByte()
{
  // Codex review finding, corrected: the first version fell back to
  // .toLatin1() here, silently sending byte 0xFE for þ (U+00FE, a real
  // Latin-1 code point but NOT part of what CP437 can display) - 0xFE
  // happens to display as an entirely different glyph (■) under CP437,
  // silent corruption rather than a reasonable fallback. CP437's reverse
  // table is exhaustive of everything CP437 can actually represent, so
  // "not found in it" now means "genuinely not representable", and
  // toWire() returns '?' instead of guessing a misleading byte.
  const QChar thorn(0x00FE);
  QCOMPARE( KomportCharset::toWire(KomportCharset::CP437, thorn), '?' );
}

void TstCharset::nonLatin1CharacterNeverSilentlyBecomesNul()
{
  // Codex review finding: QChar::toLatin1() returns 0 (NUL) for any
  // character outside the Latin-1 range, per Qt's own documented
  // behavior - indistinguishable from someone deliberately sending a real
  // NUL, and far more disruptive to real serial gear than a visible
  // placeholder. U+0100 (Ā) is well outside Latin-1 under every charset
  // implemented here.
  const QChar farOutside(0x0100);
  for ( auto id : { KomportCharset::Standard, KomportCharset::CP437, KomportCharset::PETSCII } ) {
    QCOMPARE( KomportCharset::toWire(id, farOutside), '?' );
  }
}

void TstCharset::namesAndIndexRoundTrip()
{
  const QStringList names = KomportCharset::names();
  QCOMPARE( names.size(), 3 );
  for ( auto id : { KomportCharset::Standard, KomportCharset::CP437, KomportCharset::PETSCII } ) {
    QCOMPARE( KomportCharset::fromIndex( KomportCharset::toIndex(id) ), id );
  }
  // an out-of-range index (e.g. a corrupted saved profile) must not crash
  // and must clamp to a sane default
  QCOMPARE( KomportCharset::fromIndex(99), KomportCharset::Standard );
  QCOMPARE( KomportCharset::fromIndex(-1), KomportCharset::Standard );
}

void TstCharset::settingsKeyRoundTrips()
{
  for ( auto id : { KomportCharset::Standard, KomportCharset::CP437, KomportCharset::PETSCII } ) {
    QCOMPARE( KomportCharset::fromSettingsKey( KomportCharset::settingsKey(id) ), id );
  }
  // an unrecognized/corrupted key must not crash and must fall back to Standard
  QCOMPARE( KomportCharset::fromSettingsKey( QStringLiteral("NoSuchCharset") ), KomportCharset::Standard );
}

void TstCharset::emulationTranslatesReceivedBytesThroughSelectedCharset()
{
  KomportSerial serial;
  KomportCellArray cellArray;
  KomportEmulation emu(&serial, &cellArray);
  emu.setCharset( KomportCharset::CP437 );

  emu.slotReceivedChar( static_cast<char>(0xDB) ); // █ under CP437
  KomportCell *cell = cellArray.cell(0, 0);
  QVERIFY( cell != nullptr );
  QCOMPARE( cell->character(), QChar(0x2588) );
}

void TstCharset::emulationControlCodesUnaffectedByNonStandardCharset()
{
  // The six control codes the emulation still interprets itself (here:
  // CR/LF) must keep working exactly the same regardless of charset -
  // confirms the translation point (slotReceivedChar()'s default: branch)
  // is genuinely never reached for these bytes.
  KomportSerial serial;
  KomportCellArray cellArray;
  KomportEmulation emu(&serial, &cellArray);
  emu.setCharset( KomportCharset::CP437 );

  emu.slotReceivedChar('X');
  QCOMPARE( cellArray.cursor(), QPoint(1, 0) );
  emu.slotReceivedChar(0x0D); // CR
  QCOMPARE( cellArray.cursor().x(), 0 );
  emu.slotReceivedChar(0x0A); // LF
  QCOMPARE( cellArray.cursor().y(), 1 );
}

void TstCharset::emulationDefaultsToStandardCharset()
{
  // A freshly constructed KomportEmulation (no profile ever loaded, e.g.
  // the QTermWidget spike's usage pattern or a future embedder) must
  // behave exactly like pre-Milestone-7 code - no accidental default to
  // some other charset.
  KomportSerial serial;
  KomportCellArray cellArray;
  KomportEmulation emu(&serial, &cellArray);
  QCOMPARE( emu.charset(), KomportCharset::Standard );
}

void TstCharset::settingsDialogComboBoxStoresIdAsUserRoleNotRowPosition()
{
  // Codex review finding: CharsetComboBox used to be populated purely
  // from KomportCharset::names() and read back via
  // fromIndex(currentIndex()) - correct only as long as row position
  // happened to match Id's numeric value, with nothing enforcing that.
  // It's now populated from displayEntries() with each item's Id stored
  // explicitly as Qt::UserRole data - this test checks that data
  // directly, independent of row order, and that findData()/currentData()
  // (as komport.cpp now uses) actually round-trip through it.
  SettingsDialog dialog;
  QVERIFY( dialog.CharsetComboBox != nullptr );

  const auto entries = KomportCharset::displayEntries();
  QCOMPARE( dialog.CharsetComboBox->count(), entries.size() );
  for ( int i = 0; i < entries.size(); ++i ) {
    QCOMPARE( dialog.CharsetComboBox->itemData(i).toInt(), static_cast<int>(entries.at(i).first) );
    QCOMPARE( dialog.CharsetComboBox->itemText(i), entries.at(i).second );
  }

  const int idx = dialog.CharsetComboBox->findData( static_cast<int>(KomportCharset::PETSCII) );
  QVERIFY( idx >= 0 );
  dialog.CharsetComboBox->setCurrentIndex(idx);
  QCOMPARE( static_cast<KomportCharset::Id>(dialog.CharsetComboBox->currentData().toInt()), KomportCharset::PETSCII );
}

void TstCharset::legacyProfileWithoutCharsetKeyFallsBackToStandard()
{
  // Codex review finding (same load-order-leakage class Milestone 5 fixed
  // for Appearance settings): a profile predating Milestone 7 has no
  // "Charset" key under Profiles/<name> at all. loadProfile() used to
  // fall back to whatever strCharset *currently* held - so loading a
  // CP437 profile first, then this legacy one, silently left CP437
  // active instead of resetting to Standard.
  //
  // An earlier version of this test set the CP437 "marker" directly on
  // view->mEmulation instead of actually loading a real CP437 profile
  // first - that never touches KomportApp's own strCharset member (only
  // loadProfile() does), so it couldn't poison the fallback the bug
  // actually relies on, and the test stayed green even against the
  // unfixed code. Loading a real profile first, like below, is what
  // actually reproduces it.
  {
    QSettings settings;
    settings.beginGroup( QStringLiteral("Profiles/Cp437Profile") );
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
    settings.setValue( QStringLiteral("Charset"), KomportCharset::settingsKey(KomportCharset::CP437) );
    settings.endGroup();

    settings.beginGroup( QStringLiteral("Profiles/LegacyNoCharset") );
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
    // deliberately no "Charset" key at all
    settings.endGroup();
  }

  KomportApp *win = new KomportApp();
  win->show();
  QPointer<KomportApp> guard(win);

  KomportView *view = win->findChild<KomportView *>();
  QVERIFY( view != nullptr );

  win->loadProfile( QStringLiteral("Cp437Profile") );
  QCOMPARE( view->mEmulation->charset(), KomportCharset::CP437 ); // sanity check on the setup itself

  win->loadProfile( QStringLiteral("LegacyNoCharset") ); // must not crash, must reset to Standard

  QCOMPARE( view->mEmulation->charset(), KomportCharset::Standard );

  win->close();
  QTRY_VERIFY( guard.isNull() );
}

QTEST_MAIN(TstCharset)
#include "tst_charset.moc"
