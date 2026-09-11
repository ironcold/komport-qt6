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
#include <QTextStream>
#include <QScopeGuard>
#include <QRegularExpression>
#include <algorithm>

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
  void customCharsetsDirectoryIsCreated();
  void customCharsetFileMechanismLoadsAndTranslatesCorrectly();
  void customCharsetMalformedLinesAreSkippedNotFatal();
  void customCharsetIdCollidingWithBuiltinNameIsRejected();
  void emulationTranslatesThroughLoadedCustomCharset();
  void customCharsetOversizedFileIsRejected();
  void customCharsetCountIsCapped();
  void customCharsetSurrogateCodePointIsRejected();
  void customCharsetDisplayNameCollidingWithBuiltinIsDisambiguated();
  void customCharsetDisplayNameCollidingWithAnotherCustomIsDisambiguated();
  void deletedCustomCharsetSelectionIsReconciledToStandard();
  void stillLoadedCustomCharsetSelectionSurvivesReconciliation();
  void customCharsetsDirectoryMkpathFailureIsLoggedNotCrashing();
  void customCharsetFileSizeBoundaryIsInclusive();
  void customCharsetOverlongLineIsDiscardedWholeNotMisparsedAsTailChunk();
  void customCharsetBinaryFileWithEmbeddedNulIsRejected();
  void customCharsetSurrogateBoundaryValues();
  void customCharsetSuffixDisambiguationItselfCannotCollide();
  void multiWindowCustomCharsetSelectionIsReconciledInAllWindows();
  void loadProfileNormalizesDanglingStrCharsetNotJustLiveEmulation();
  void customCharsetCapCountsExaminedFilesNotJustSuccessful();
  void customCharsetLineExactlyAtLengthLimitIsAccepted();
  void newWindowConstructionReconcilesOtherOpenWindows();
  void customCharsetAmbiguousReverseMappingIsWarnedAndLowerByteWins();
  void customCharsetLineCountCapStopsProcessingWithoutCrashing();
  void customCharsetLineLengthMeasuredInCharactersNotUtf8BytesOrCrlf();
  void customCharsetUnrepresentableCharacterUsesTablesOwnQuestionMarkByte();
  void customCharsetLineCountCapExactBoundaryIsPrecise();
  void customCharsetLineLengthCountsCodePointsNotUtf16Units();
  void customCharsetMalformedUtf8IsRejected();
  void customCharsetExactly100000LinesWithTrailingNewlineDoesNotWarn();
  void customCharsetQuestionMarkFallbackWarnsWhenTableHasNoQuestionMarkByteEither();
  void customCharsetTruncatedUtf8AtEndOfFileIsRejected();
  void customCharsetAstralLineLengthExactBoundaryIsPrecise();

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
  // Milestone 7 custom-charset addendum: this directory now also holds a
  // charsets/ subdirectory (an auto-written README.txt, plus whatever
  // *.charset files an individual test didn't already clean up itself
  // via its own qScopeGuard) - remove it too, or the plain rmdir() below
  // (which only succeeds on an already-empty directory) would silently
  // leave this whole tree behind across test binary runs, contaminating
  // the *next* run's "was this really just created?" assertions (see
  // customCharsetsDirectoryIsCreated() - caught exactly this way while
  // red/green-verifying the README-writing fix).
  QDir(dirPath + QStringLiteral("/charsets")).removeRecursively();
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
  // Milestone 7 custom-charset addendum: fromSettingsKey()->Id was
  // replaced by resolveSettingsKey()->Selection, since a bare Id can no
  // longer distinguish between "no custom charset" and "which of
  // possibly many loaded custom charsets" - see KomportCharset::Selection.
  for ( auto id : { KomportCharset::Standard, KomportCharset::CP437, KomportCharset::PETSCII } ) {
    const KomportCharset::Selection sel = KomportCharset::resolveSettingsKey( KomportCharset::settingsKey(id) );
    QCOMPARE( sel.id, id );
    QVERIFY( sel.customId.isEmpty() );
  }
  // an unrecognized/corrupted key must not crash and must fall back to Standard
  const KomportCharset::Selection unknown = KomportCharset::resolveSettingsKey( QStringLiteral("NoSuchCharset") );
  QCOMPARE( unknown.id, KomportCharset::Standard );
  QVERIFY( unknown.customId.isEmpty() );
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
  // Milestone 7 custom-charset addendum: the UserRole payload is now the
  // settingsKey()-style *string*, not the bare Id, since every loaded
  // custom charset shares Id::Custom - only the string uniquely
  // identifies a row. This test deliberately doesn't assume zero custom
  // entries (another test in this file may have loaded one into the
  // process-wide registry already - see customCharsetFileMechanism...()
  // below), it just checks the built-ins come first, in order, followed
  // by whatever customCharsetEntries() currently reports.
  SettingsDialog dialog;
  QVERIFY( dialog.CharsetComboBox != nullptr );

  const auto builtins = KomportCharset::displayEntries();
  const auto customs = KomportCharset::customCharsetEntries();
  QCOMPARE( dialog.CharsetComboBox->count(), builtins.size() + customs.size() );
  for ( int i = 0; i < builtins.size(); ++i ) {
    QCOMPARE( dialog.CharsetComboBox->itemData(i).toString(), KomportCharset::settingsKey(builtins.at(i).first) );
    QCOMPARE( dialog.CharsetComboBox->itemText(i), builtins.at(i).second );
  }
  for ( int i = 0; i < customs.size(); ++i ) {
    QCOMPARE( dialog.CharsetComboBox->itemData(builtins.size() + i).toString(), customs.at(i).first );
    QCOMPARE( dialog.CharsetComboBox->itemText(builtins.size() + i), customs.at(i).second );
  }

  const QString petsciiKey = KomportCharset::settingsKey(KomportCharset::PETSCII);
  const int idx = dialog.CharsetComboBox->findData( petsciiKey );
  QVERIFY( idx >= 0 );
  dialog.CharsetComboBox->setCurrentIndex(idx);
  QCOMPARE( dialog.CharsetComboBox->currentData().toString(), petsciiKey );
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

void TstCharset::customCharsetsDirectoryIsCreated()
{
  // Milestone 7 addendum (user request: "einen geeigneten Mechanismus
  // vorsehen, so dass neue Tabellen einfach in ein entsprechendes
  // Verzeichnis abgelegt werden") - the directory has to actually exist
  // before a user can be told "drop a file in here", not just be a path
  // string that happens to work once something else creates it first.
  const QString dir = KomportCharset::customCharsetsDirectory();
  QVERIFY( !dir.isEmpty() );
  QVERIFY2( QDir(dir).exists(), qPrintable(QStringLiteral("%1 should have been created").arg(dir)) );
  // this test's own org/app name (see initTestCase()) keeps it away from
  // the real user's ~/.config/Komport-Qt6/charsets/ entirely.
  QVERIFY( dir.contains(QStringLiteral("Komport-Qt6-Test-Charset")) );

  // Milestone 7 addendum (user request: "die Benutzung auch sauber
  // dokumentieren, so dass das für jeden sofort verständlich ist") - a
  // short usage README should already be sitting right there the first
  // time anyone opens this folder, not just an empty directory with no
  // explanation.
  const QString readmePath = dir + QStringLiteral("/README.txt");
  QVERIFY2( QFile::exists(readmePath), "customCharsetsDirectory() should write a usage README.txt on first creation" );
  QFile readme(readmePath);
  QVERIFY( readme.open(QIODevice::ReadOnly | QIODevice::Text) );
  const QString content = QString::fromUtf8( readme.readAll() );
  QVERIFY2( content.contains(QStringLiteral(".charset")), "README should mention the *.charset file extension" );
  QVERIFY2( content.contains(QStringLiteral("EN:")) && content.contains(QStringLiteral("DE:")),
            "README should be bilingual (EN/DE), per the user's explicit request" );
}

void TstCharset::customCharsetFileMechanismLoadsAndTranslatesCorrectly()
{
  // End-to-end proof of the actual "drop a file in, it becomes
  // selectable" mechanism: write a real *.charset file, reload the
  // registry, verify it's listed with the right name/id, and that RX/TX
  // translation genuinely reads from its table (not e.g. accidentally
  // falling back to Standard/CP437's tables).
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString filePath = dir + QStringLiteral("/tstcustom.charset");
  {
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly | QIODevice::Text) );
    QTextStream out(&f);
    out << "# Name: Test Custom Charset\n";
    out << "DB=2588\n"; // full block
    out << "41=03B1\n"; // 'A' (0x41) -> α, an arbitrary distinctive override
  }
  auto cleanup = qScopeGuard( [&filePath]() {
    QFile::remove(filePath);
    KomportCharset::reloadCustomCharsets(); // leave the registry clean for later tests
  } );
  KomportCharset::reloadCustomCharsets();

  const auto customs = KomportCharset::customCharsetEntries();
  auto it = std::find_if( customs.begin(), customs.end(),
      []( const QPair<QString,QString> &e ) { return e.first == QStringLiteral("tstcustom"); } );
  QVERIFY2( it != customs.end(), "tstcustom.charset was not picked up by reloadCustomCharsets()" );
  QCOMPARE( it->second, QStringLiteral("Test Custom Charset") );

  // RX: the two overridden bytes translate as specified in the file...
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0xDB, QStringLiteral("tstcustom")), QChar(0x2588) );
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0x41, QStringLiteral("tstcustom")), QChar(0x03B1) );
  // ...and anything NOT listed defaults to identity, same policy as
  // CP437/PETSCII's own control ranges - this is what makes control
  // codes safe without the file author needing to think about VT100 at all.
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0x42, QStringLiteral("tstcustom")), QChar('B') );
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0x0D, QStringLiteral("tstcustom")), QChar(0x0D) );

  // TX: reverse direction, derived automatically from the same table -
  // no separate "reverse table" section in the file format.
  QCOMPARE( KomportCharset::toWire(KomportCharset::Custom, QChar(0x2588), QStringLiteral("tstcustom")), static_cast<char>(0xDB) );
  QCOMPARE( KomportCharset::toWire(KomportCharset::Custom, QChar(0x03B1), QStringLiteral("tstcustom")), static_cast<char>(0x41) );
  QCOMPARE( KomportCharset::toWire(KomportCharset::Custom, QChar('B'), QStringLiteral("tstcustom")), 'B' );

  // Persistence: the filename stem *is* the settings key directly - no
  // "Custom:" prefix or similar needed.
  const KomportCharset::Selection sel = KomportCharset::resolveSettingsKey( QStringLiteral("tstcustom") );
  QCOMPARE( sel.id, KomportCharset::Custom );
  QCOMPARE( sel.customId, QStringLiteral("tstcustom") );
}

void TstCharset::customCharsetMalformedLinesAreSkippedNotFatal()
{
  // Consistent with this codebase's general policy of clamping/skipping
  // individual bad values instead of rejecting an entire file/profile
  // over one bad line (see KomportApp::loadProfile()'s handling of a
  // corrupted profile field) - a typo on one line must not lose every
  // other, valid override in the same file.
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString filePath = dir + QStringLiteral("/tstmalformed.charset");
  {
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly | QIODevice::Text) );
    QTextStream out(&f);
    out << "41=03B1\n";        // valid
    out << "this is garbage\n"; // malformed - no '='
    out << "ZZ=03B2\n";        // malformed - byte not valid hex
    out << "42=GGGG\n";        // malformed - code point not valid hex
    out << "43=1FFFFF\n";      // malformed - code point out of range (>0xFFFF)
    out << "FF=0100\n";        // valid
  }
  auto cleanup = qScopeGuard( [&filePath]() {
    QFile::remove(filePath);
    KomportCharset::reloadCustomCharsets();
  } );
  KomportCharset::reloadCustomCharsets();

  const auto customs = KomportCharset::customCharsetEntries();
  auto it = std::find_if( customs.begin(), customs.end(),
      []( const QPair<QString,QString> &e ) { return e.first == QStringLiteral("tstmalformed"); } );
  QVERIFY2( it != customs.end(), "a file with some malformed lines must still load overall" );

  QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0x41, QStringLiteral("tstmalformed")), QChar(0x03B1) ); // valid line took effect
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0xFF, QStringLiteral("tstmalformed")), QChar(0x0100) ); // valid line took effect
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0x42, QStringLiteral("tstmalformed")), QChar('B') );    // malformed code point -> left at identity
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0x43, QStringLiteral("tstmalformed")), QChar('C') );    // out-of-range code point -> left at identity
}

void TstCharset::customCharsetIdCollidingWithBuiltinNameIsRejected()
{
  // A file whose id (filename stem) matches a built-in name must not be
  // able to shadow or interfere with that hardened, already-reviewed
  // built-in implementation - see reloadCustomCharsets()'s own comment.
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString filePath = dir + QStringLiteral("/CP437.charset"); // collides case-insensitively
  {
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly | QIODevice::Text) );
    QTextStream out(&f);
    out << "41=03B1\n"; // if this somehow took effect, CP437's own 'A' would break
  }
  auto cleanup = qScopeGuard( [&filePath]() {
    QFile::remove(filePath);
    KomportCharset::reloadCustomCharsets();
  } );
  KomportCharset::reloadCustomCharsets();

  const auto customs = KomportCharset::customCharsetEntries();
  auto it = std::find_if( customs.begin(), customs.end(),
      []( const QPair<QString,QString> &e ) { return e.first.compare(QStringLiteral("CP437"), Qt::CaseInsensitive) == 0; } );
  QVERIFY2( it == customs.end(), "a custom charset file colliding with a built-in name must be rejected, not silently shadow it" );

  // the real, built-in CP437 must be completely unaffected.
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::CP437, 0x41), QChar('A') );
}

void TstCharset::emulationTranslatesThroughLoadedCustomCharset()
{
  // Same integration-level check as
  // emulationTranslatesReceivedBytesThroughSelectedCharset() above, but
  // for a loaded custom charset instead of a built-in one - proves the
  // KomportEmulation::setCharset(Id, customId)/mCustomCharsetId plumbing
  // actually reaches KomportCharset::toDisplay(), not just the direct
  // static-function call tested above.
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString filePath = dir + QStringLiteral("/tstemu.charset");
  {
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly | QIODevice::Text) );
    QTextStream out(&f);
    out << "41=2588\n"; // 'A' (0x41) -> █
  }
  auto cleanup = qScopeGuard( [&filePath]() {
    QFile::remove(filePath);
    KomportCharset::reloadCustomCharsets();
  } );
  KomportCharset::reloadCustomCharsets();

  KomportSerial serial;
  KomportCellArray cellArray;
  KomportEmulation emu(&serial, &cellArray);
  emu.setCharset( KomportCharset::Custom, QStringLiteral("tstemu") );
  QCOMPARE( emu.charset(), KomportCharset::Custom );
  QCOMPARE( emu.customCharsetId(), QStringLiteral("tstemu") );

  emu.slotReceivedChar('A');
  KomportCell *cell = cellArray.cell(0, 0);
  QVERIFY( cell != nullptr );
  QCOMPARE( cell->character(), QChar(0x2588) );
}

void TstCharset::customCharsetOversizedFileIsRejected()
{
  // Codex review round-2 finding (Medium): an unbounded *.charset file
  // could make loading it slow/memory-hungry - loadCustomCharsetFile()
  // now rejects anything over MaxCustomCharsetFileSize (1 MiB) up front,
  // before even opening it for parsing. Content doesn't matter here, only
  // size - a run of filler bytes is enough to trip the check.
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString filePath = dir + QStringLiteral("/tstoversized.charset");
  {
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly) );
    // 1 MiB + a bit, well past the limit - single write, no need to be clever.
    QVERIFY( f.write( QByteArray(1024 * 1024 + 4096, 'a') ) > 0 );
  }
  auto cleanup = qScopeGuard( [&filePath]() {
    QFile::remove(filePath);
    KomportCharset::reloadCustomCharsets();
  } );
  KomportCharset::reloadCustomCharsets();

  const auto customs = KomportCharset::customCharsetEntries();
  auto it = std::find_if( customs.begin(), customs.end(),
      []( const QPair<QString,QString> &e ) { return e.first == QStringLiteral("tstoversized"); } );
  QVERIFY2( it == customs.end(), "a *.charset file over the size limit must be rejected, not loaded" );
}

void TstCharset::customCharsetCountIsCapped()
{
  // Codex review round-2 finding (Medium): an unbounded number of
  // *.charset files in the directory could make every reload (app
  // startup, every Settings-dialog reopen) slow - reloadCustomCharsets()
  // now stops after MaxCustomCharsetCount (256) files, in the same
  // alphabetical order QDir::entryList(..., QDir::Name) already lists
  // them in, so which ones get kept is deterministic.
  //
  // Codex review round-3 finding: the original version of this test
  // measured a "sizeBefore" baseline instead of assuming the directory
  // starts empty, and claimed that made it independent of execution
  // order/leftover state - not actually true (the cap applies to the
  // *entire* directory's contents, not additively per call, so the math
  // silently breaks if enough pre-existing files were already present).
  // Deletes every pre-existing *.charset file up front instead, so the
  // starting state is a real, verified empty directory rather than an
  // assumed one - the guarantee this test actually needs.
  const QString dir = KomportCharset::customCharsetsDirectory();
  {
    QDir d(dir);
    for ( const QString &leftover : d.entryList(QStringList{QStringLiteral("*.charset")}, QDir::Files) ) {
      QFile::remove( d.filePath(leftover) );
    }
  }

  constexpr int kFileCount = 257; // one past the production 256 cap
  QStringList createdPaths;
  for ( int i = 0; i < kFileCount; ++i ) {
    // zero-padded so QDir::Name's alphabetical order matches numeric order
    const QString filePath = dir + QStringLiteral("/tstcap%1.charset").arg(i, 3, 10, QLatin1Char('0'));
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly | QIODevice::Text) );
    QTextStream out(&f);
    out << "41=2588\n"; // minimal, valid, content is irrelevant to this test
    createdPaths << filePath;
  }
  auto cleanup = qScopeGuard( [&createdPaths]() {
    for ( const QString &p : createdPaths ) QFile::remove(p);
    KomportCharset::reloadCustomCharsets();
  } );

  KomportCharset::reloadCustomCharsets();
  const auto customs = KomportCharset::customCharsetEntries();
  QCOMPARE( customs.size(), 256 ); // capped, not 257, from a verified-empty starting point

  // the alphabetically-first 256 (tstcap000..tstcap255) must have been
  // kept, the last one (tstcap256, examined 257th) must not have been -
  // pins *which* ones survive the cap, not just the total count.
  auto has = [&customs]( const QString &id ) {
    return std::any_of( customs.begin(), customs.end(),
        [&id]( const QPair<QString,QString> &e ) { return e.first == id; } );
  };
  QVERIFY( has(QStringLiteral("tstcap000")) );
  QVERIFY( has(QStringLiteral("tstcap255")) );
  QVERIFY2( !has(QStringLiteral("tstcap256")), "the 257th file (examined only after the cap is already full) must not have been loaded" );
}

void TstCharset::customCharsetSurrogateCodePointIsRejected()
{
  // Codex review round-2 finding (Low): 0xD800-0xDFFF are UTF-16
  // surrogate halves, not valid standalone Unicode scalar values -
  // rejected the same way as any other out-of-range code point (line
  // skipped, rest of the file still loads).
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString filePath = dir + QStringLiteral("/tstsurrogate.charset");
  {
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly | QIODevice::Text) );
    QTextStream out(&f);
    out << "41=D800\n"; // surrogate - must be rejected
    out << "42=0041\n"; // valid - rest of the file must still load
  }
  auto cleanup = qScopeGuard( [&filePath]() {
    QFile::remove(filePath);
    KomportCharset::reloadCustomCharsets();
  } );
  KomportCharset::reloadCustomCharsets();

  QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0x41, QStringLiteral("tstsurrogate")), QChar('A') ); // left at identity
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0x42, QStringLiteral("tstsurrogate")), QChar('A') ); // valid line took effect
}

void TstCharset::customCharsetDisplayNameCollidingWithBuiltinIsDisambiguated()
{
  // Codex review round-2 finding (Low): an unchecked "# Name: ..." line
  // could claim to be "Standard"/"IBM CP437"/"PETSCII", making the
  // dropdown show an entry indistinguishable from a built-in even though
  // the underlying id (and behavior) differs. Disambiguated with
  // " (<id>)" rather than rejected outright - the file is still loadable.
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString filePath = dir + QStringLiteral("/tstbuiltincollide.charset");
  {
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly | QIODevice::Text) );
    QTextStream out(&f);
    out << "# Name: Standard\n"; // collides with the built-in "Standard"
    out << "41=2588\n";
  }
  auto cleanup = qScopeGuard( [&filePath]() {
    QFile::remove(filePath);
    KomportCharset::reloadCustomCharsets();
  } );
  KomportCharset::reloadCustomCharsets();

  const auto customs = KomportCharset::customCharsetEntries();
  auto it = std::find_if( customs.begin(), customs.end(),
      []( const QPair<QString,QString> &e ) { return e.first == QStringLiteral("tstbuiltincollide"); } );
  QVERIFY2( it != customs.end(), "the file itself must still load despite the name collision" );
  QCOMPARE( it->second, QStringLiteral("Standard (tstbuiltincollide)") );
}

void TstCharset::customCharsetDisplayNameCollidingWithAnotherCustomIsDisambiguated()
{
  // Same finding as above, but two *custom* files claiming the same
  // display name - alphabetically first ("tstdup1") wins the plain name,
  // the later one ("tstdup2") gets disambiguated against it.
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString path1 = dir + QStringLiteral("/tstdup1.charset");
  const QString path2 = dir + QStringLiteral("/tstdup2.charset");
  for ( const QString &p : { path1, path2 } ) {
    QFile f(p);
    QVERIFY( f.open(QIODevice::WriteOnly | QIODevice::Text) );
    QTextStream out(&f);
    out << "# Name: Duplicate Name\n";
    out << "41=2588\n";
  }
  auto cleanup = qScopeGuard( [&path1, &path2]() {
    QFile::remove(path1);
    QFile::remove(path2);
    KomportCharset::reloadCustomCharsets();
  } );
  KomportCharset::reloadCustomCharsets();

  const auto customs = KomportCharset::customCharsetEntries();
  auto find = [&customs]( const QString &id ) {
    return std::find_if( customs.begin(), customs.end(),
        [&id]( const QPair<QString,QString> &e ) { return e.first == id; } );
  };
  const auto it1 = find( QStringLiteral("tstdup1") );
  const auto it2 = find( QStringLiteral("tstdup2") );
  QVERIFY( it1 != customs.end() );
  QVERIFY( it2 != customs.end() );
  QCOMPARE( it1->second, QStringLiteral("Duplicate Name") );              // first alphabetically - keeps the plain name
  QCOMPARE( it2->second, QStringLiteral("Duplicate Name (tstdup2)") );    // second - disambiguated
}

void TstCharset::deletedCustomCharsetSelectionIsReconciledToStandard()
{
  // Codex review round-2 finding (Medium): if the *live* session's
  // selected custom charset's file gets deleted/renamed, the stored
  // selection itself (view->mEmulation's charset()/customCharsetId(),
  // and strCharset) used to stay dangling on the now-nonexistent id -
  // KomportApp::reconcileCharsetSelectionAfterReload() (factored out of
  // slotShowPreferences(), see komport.h, so this can be tested directly
  // without driving a modal QDialog::exec()) must reset both back to
  // Standard once KomportCharset::reloadCustomCharsets() no longer lists it.
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString filePath = dir + QStringLiteral("/tstreconcile.charset");
  {
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly | QIODevice::Text) );
    QTextStream out(&f);
    out << "41=2588\n";
  }
  KomportCharset::reloadCustomCharsets();

  KomportApp *win = new KomportApp();
  win->show();
  QPointer<KomportApp> guard(win);
  KomportView *view = win->findChild<KomportView *>();
  QVERIFY( view != nullptr );

  view->mEmulation->setCharset( KomportCharset::Custom, QStringLiteral("tstreconcile") );
  win->strCharset = QStringLiteral("tstreconcile"); // TstCharset is a friend of KomportApp - see komport.h
  QCOMPARE( view->mEmulation->charset(), KomportCharset::Custom ); // sanity check on the setup itself

  QFile::remove(filePath);
  KomportCharset::reloadCustomCharsets(); // registry no longer has "tstreconcile"

  win->reconcileCharsetSelectionAfterReload();

  QCOMPARE( view->mEmulation->charset(), KomportCharset::Standard );
  QCOMPARE( win->strCharset, KomportCharset::settingsKey(KomportCharset::Standard) );

  win->close();
  QTRY_VERIFY( guard.isNull() );
}

void TstCharset::stillLoadedCustomCharsetSelectionSurvivesReconciliation()
{
  // Counterpart to the test above: a custom charset whose file is still
  // present and loaded must NOT be reset - reconcileCharsetSelectionAfterReload()
  // only acts on a genuinely missing id.
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString filePath = dir + QStringLiteral("/tststillloaded.charset");
  {
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly | QIODevice::Text) );
    QTextStream out(&f);
    out << "41=2588\n";
  }
  auto cleanup = qScopeGuard( [&filePath]() {
    QFile::remove(filePath);
    KomportCharset::reloadCustomCharsets();
  } );
  KomportCharset::reloadCustomCharsets();

  KomportApp *win = new KomportApp();
  win->show();
  QPointer<KomportApp> guard(win);
  KomportView *view = win->findChild<KomportView *>();
  QVERIFY( view != nullptr );

  view->mEmulation->setCharset( KomportCharset::Custom, QStringLiteral("tststillloaded") );
  win->strCharset = QStringLiteral("tststillloaded");

  win->reconcileCharsetSelectionAfterReload();

  QCOMPARE( view->mEmulation->charset(), KomportCharset::Custom );
  QCOMPARE( view->mEmulation->customCharsetId(), QStringLiteral("tststillloaded") );
  QCOMPARE( win->strCharset, QStringLiteral("tststillloaded") );

  win->close();
  QTRY_VERIFY( guard.isNull() );
}

void TstCharset::customCharsetsDirectoryMkpathFailureIsLoggedNotCrashing()
{
  // Codex review round-2 finding (Low): customCharsetsDirectory() used to
  // ignore QDir::mkpath()'s return value entirely - a read-only
  // filesystem, or (reproduced here) a regular *file* already sitting at
  // the "charsets" path instead of a directory, silently handed back a
  // path that doesn't actually work for anything using it afterwards,
  // with no diagnostic explaining why custom charsets never show up.
  // Forces exactly that collision, confirms it's non-crashing (the only
  // thing that can realistically be asserted about a qWarning()-only
  // fix), and restores a real directory afterwards so later runs of this
  // same test binary aren't left in a broken state.
  const QString dir = KomportCharset::customCharsetsDirectory(); // establishes/normalizes the path first
  QDir(dir).removeRecursively();
  QVERIFY2( QFile(dir).open(QIODevice::WriteOnly), "could not set up the file-instead-of-directory collision" );

  auto restore = qScopeGuard( [&dir]() {
    QFile::remove(dir);
    KomportCharset::customCharsetsDirectory(); // recreates it as a real directory again
  } );

  QTest::ignoreMessage( QtWarningMsg, QRegularExpression(QStringLiteral("could not create")) );
  const QString result = KomportCharset::customCharsetsDirectory(); // must not crash
  QCOMPARE( result, dir );
  QVERIFY2( !QDir(dir).exists(), "the collision must still block an actual directory from existing at that path" );
}

void TstCharset::customCharsetFileSizeBoundaryIsInclusive()
{
  // Codex review round-3 finding: the size-cap test only checked well
  // above the 1 MiB limit, not the boundary itself - pins that exactly
  // 1 MiB is still accepted (the check is "> limit", not ">= limit") and
  // one byte over is rejected, so the boundary can't silently drift to an
  // off-by-one in either direction later.
  const QString dir = KomportCharset::customCharsetsDirectory();
  const qint64 oneMiB = 1024 * 1024;

  const QString atLimitPath = dir + QStringLiteral("/tstatlimit.charset");
  const QString overLimitPath = dir + QStringLiteral("/tstoverlimit.charset");
  auto cleanup = qScopeGuard( [&atLimitPath, &overLimitPath]() {
    QFile::remove(atLimitPath);
    QFile::remove(overLimitPath);
    KomportCharset::reloadCustomCharsets();
  } );

  {
    // "41=2588\n" is 8 bytes - pad with comment filler up to exactly 1 MiB.
    QFile f(atLimitPath);
    QVERIFY( f.open(QIODevice::WriteOnly) );
    const QByteArray mapping = "41=2588\n";
    QByteArray padding( oneMiB - mapping.size() - 1, '#' ); // '#' lines are comments, harmless filler
    padding += '\n';
    QVERIFY( f.write(padding) == padding.size() );
    QVERIFY( f.write(mapping) == mapping.size() );
    f.close(); // flush to disk before checking the size below - QFileInfo on a still-open, unflushed QFile can read a stale/short size
  }
  QCOMPARE( QFileInfo(atLimitPath).size(), oneMiB );
  {
    QFile f(overLimitPath);
    QVERIFY( f.open(QIODevice::WriteOnly) );
    QByteArray oneOver( oneMiB + 1, 'a' );
    QVERIFY( f.write(oneOver) == oneOver.size() );
  }

  KomportCharset::reloadCustomCharsets();
  const auto customs = KomportCharset::customCharsetEntries();
  auto has = [&customs]( const QString &id ) {
    return std::any_of( customs.begin(), customs.end(),
        [&id]( const QPair<QString,QString> &e ) { return e.first == id; } );
  };
  QVERIFY2( has(QStringLiteral("tstatlimit")), "a file at exactly the 1 MiB limit must still be accepted" );
  QVERIFY2( !has(QStringLiteral("tstoverlimit")), "a file one byte over the 1 MiB limit must be rejected" );
}

void TstCharset::customCharsetOverlongLineIsDiscardedWholeNotMisparsedAsTailChunk()
{
  // Codex review round-3 finding (Low): QTextStream::readLine(maxlen)
  // *splits* an over-length physical line across multiple readLine()
  // calls rather than rejecting it - the exact scenario Codex described:
  // one physical line consisting of a "#" comment marker, filler up to
  // the 4096-character chunk limit, immediately followed (still on the
  // very same physical line, no real newline yet) by what looks like a
  // real "41=2588" mapping. Under the bug, the split-off tail chunk
  // ("41=2588") would be parsed as its own independent, valid line. This
  // proves it is NOT: 0x41 must stay at its default identity mapping.
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString filePath = dir + QStringLiteral("/tstoverlong.charset");
  {
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly) );
    // one single physical line: "#" + exactly enough 'x' filler to reach
    // the 4096-char readLine() chunk size, then "41=2588" glued on with NO
    // real newline in between yet, then a real newline, then a second,
    // genuinely valid, independent line to prove the parser resyncs.
    QByteArray commentPrefix = "#" + QByteArray(4095, 'x'); // exactly 4096 chars so far
    QByteArray line1 = commentPrefix + "41=2588\n";
    QVERIFY( f.write(line1) == line1.size() );
    QByteArray line2 = "42=0041\n"; // genuinely independent, valid second line
    QVERIFY( f.write(line2) == line2.size() );
  }
  auto cleanup = qScopeGuard( [&filePath]() {
    QFile::remove(filePath);
    KomportCharset::reloadCustomCharsets();
  } );
  KomportCharset::reloadCustomCharsets();

  QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0x41, QStringLiteral("tstoverlong")), QChar('A') ); // NOT 0x2588 - the split tail must not have been parsed as a mapping
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0x42, QStringLiteral("tstoverlong")), QChar(0x0041) ); // the real next line still parses correctly - parser resynced
}

void TstCharset::customCharsetBinaryFileWithEmbeddedNulIsRejected()
{
  // Codex review round-3 finding (Low): QTextStream::status() does not
  // reliably detect a genuinely binary/malformed-encoding file in this
  // parsing path (Qt's decoder substitutes replacement characters rather
  // than raising a stream error) - loadCustomCharsetFile() now sniffs for
  // an embedded NUL byte up front instead, the standard binary-content
  // heuristic. A real *.charset text file has no legitimate reason to
  // contain one.
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString filePath = dir + QStringLiteral("/tstbinary.charset");
  {
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly) );
    QByteArray content = "41=2588\n";
    content.insert( 2, '\0' ); // embed a NUL byte in otherwise-plausible content
    QVERIFY( f.write(content) == content.size() );
  }
  auto cleanup = qScopeGuard( [&filePath]() {
    QFile::remove(filePath);
    KomportCharset::reloadCustomCharsets();
  } );
  KomportCharset::reloadCustomCharsets();

  const auto customs = KomportCharset::customCharsetEntries();
  auto it = std::find_if( customs.begin(), customs.end(),
      []( const QPair<QString,QString> &e ) { return e.first == QStringLiteral("tstbinary"); } );
  QVERIFY2( it == customs.end(), "a file with an embedded NUL byte must be rejected as binary, not loaded" );
}

void TstCharset::customCharsetSurrogateBoundaryValues()
{
  // Codex review round-3 finding: the original surrogate test only
  // exercised 0xD800 - a faulty implementation rejecting just that one
  // value would still have passed. Covers both ends of the D800-DFFF
  // surrogate range plus the immediately adjacent, genuinely valid code
  // points on either side (D7FF and E000), so an off-by-one at either
  // boundary would be caught.
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString filePath = dir + QStringLiteral("/tstsurrogateboundary.charset");
  {
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly | QIODevice::Text) );
    QTextStream out(&f);
    out << "41=D7FF\n"; // valid - just below the surrogate range
    out << "42=D800\n"; // invalid - first surrogate
    out << "43=DFFF\n"; // invalid - last surrogate
    out << "44=E000\n"; // valid - just above the surrogate range
  }
  auto cleanup = qScopeGuard( [&filePath]() {
    QFile::remove(filePath);
    KomportCharset::reloadCustomCharsets();
  } );
  KomportCharset::reloadCustomCharsets();

  QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0x41, QStringLiteral("tstsurrogateboundary")), QChar(0xD7FF) );
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0x42, QStringLiteral("tstsurrogateboundary")), QChar('B') ); // rejected -> identity
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0x43, QStringLiteral("tstsurrogateboundary")), QChar('C') ); // rejected -> identity
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0x44, QStringLiteral("tstsurrogateboundary")), QChar(0xE000) );
}

void TstCharset::customCharsetSuffixDisambiguationItselfCannotCollide()
{
  // Codex review round-3 finding (Low): the first version of the
  // disambiguation logic only checked the *original* declared name, then
  // applied the " (<id>)" suffix unconditionally without re-checking
  // whether the generated result itself now collided with an
  // already-registered entry. Reproduces exactly the scenario Codex gave:
  // "tstdupa.charset" declares the literal name "Standard (tstdupb)" (no
  // collision by itself), while "tstdupb.charset" declares "Standard"
  // (collides with the built-in) - naively suffixing the latter with its
  // own id would produce the *same* string "Standard (tstdupb)" the first
  // file already claimed. Both final names must be distinct.
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString pathA = dir + QStringLiteral("/tstdupa.charset"); // alphabetically first
  const QString pathB = dir + QStringLiteral("/tstdupb.charset");
  {
    QFile f(pathA);
    QVERIFY( f.open(QIODevice::WriteOnly | QIODevice::Text) );
    QTextStream out(&f);
    out << "# Name: Standard (tstdupb)\n"; // deliberately what tstdupb's own suffix would produce
    out << "41=2588\n";
  }
  {
    QFile f(pathB);
    QVERIFY( f.open(QIODevice::WriteOnly | QIODevice::Text) );
    QTextStream out(&f);
    out << "# Name: Standard\n"; // collides with the built-in "Standard"
    out << "41=2588\n";
  }
  auto cleanup = qScopeGuard( [&pathA, &pathB]() {
    QFile::remove(pathA);
    QFile::remove(pathB);
    KomportCharset::reloadCustomCharsets();
  } );
  KomportCharset::reloadCustomCharsets();

  const auto customs = KomportCharset::customCharsetEntries();
  auto find = [&customs]( const QString &id ) {
    return std::find_if( customs.begin(), customs.end(),
        [&id]( const QPair<QString,QString> &e ) { return e.first == id; } );
  };
  const auto itA = find( QStringLiteral("tstdupa") );
  const auto itB = find( QStringLiteral("tstdupb") );
  QVERIFY( itA != customs.end() );
  QVERIFY( itB != customs.end() );
  QCOMPARE( itA->second, QStringLiteral("Standard (tstdupb)") ); // unmodified - no collision when it was registered first
  QVERIFY2( itB->second != itA->second, "the disambiguated name must not collide with an already-registered entry's name" );
  QVERIFY2( itB->second != QStringLiteral("Standard"), "must still be disambiguated away from the built-in it originally collided with" );
}

void TstCharset::multiWindowCustomCharsetSelectionIsReconciledInAllWindows()
{
  // Codex review round-3 finding (Medium): KomportCharset's custom-charset
  // registry is one process-wide static shared by every open KomportApp
  // window - reconciling only the window that happens to be opening
  // Settings left any *other* open window's live Custom selection
  // dangling. Two windows, both pointed at the same now-deleted custom
  // charset, reconciled via the static all-windows helper (what
  // slotShowPreferences() now actually calls) - both must end up reset,
  // not just the one that would have opened Settings.
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString filePath = dir + QStringLiteral("/tstmultiwin.charset");
  {
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly | QIODevice::Text) );
    QTextStream out(&f);
    out << "41=2588\n";
  }
  KomportCharset::reloadCustomCharsets();

  KomportApp *winA = new KomportApp();
  winA->show();
  QPointer<KomportApp> guardA(winA);
  KomportApp *winB = new KomportApp();
  winB->show();
  QPointer<KomportApp> guardB(winB);

  KomportView *viewA = winA->findChild<KomportView *>();
  KomportView *viewB = winB->findChild<KomportView *>();
  QVERIFY( viewA != nullptr );
  QVERIFY( viewB != nullptr );

  viewA->mEmulation->setCharset( KomportCharset::Custom, QStringLiteral("tstmultiwin") );
  winA->strCharset = QStringLiteral("tstmultiwin");
  viewB->mEmulation->setCharset( KomportCharset::Custom, QStringLiteral("tstmultiwin") );
  winB->strCharset = QStringLiteral("tstmultiwin");

  QFile::remove(filePath);
  KomportCharset::reloadCustomCharsets(); // registry no longer has "tstmultiwin" - neither window has reconciled yet

  KomportApp::reconcileCharsetSelectionAfterReloadForAllWindows();

  QCOMPARE( viewA->mEmulation->charset(), KomportCharset::Standard );
  QCOMPARE( viewB->mEmulation->charset(), KomportCharset::Standard );
  QCOMPARE( winA->strCharset, KomportCharset::settingsKey(KomportCharset::Standard) );
  QCOMPARE( winB->strCharset, KomportCharset::settingsKey(KomportCharset::Standard) );

  winA->close();
  winB->close();
  QTRY_VERIFY( guardA.isNull() );
  QTRY_VERIFY( guardB.isNull() );
}

void TstCharset::loadProfileNormalizesDanglingStrCharsetNotJustLiveEmulation()
{
  // Codex review round-3 finding (Medium): loadProfile() correctly reset
  // the *live emulation* to Standard for a profile referencing a
  // now-missing custom charset id (via resolveSettingsKey()'s own
  // fallback), but left strCharset itself holding the original, still-
  // dangling raw id - which then got re-persisted verbatim if the profile
  // was saved again, and reconcileCharsetSelectionAfterReload() can't
  // catch this case either (it only acts when charset()==Custom, which is
  // no longer true once resolveSettingsKey() has already downgraded it).
  // Checks strCharset directly (TstCharset is a friend of KomportApp),
  // not just view->mEmulation->charset().
  {
    QSettings settings;
    settings.beginGroup( QStringLiteral("Profiles/DanglingCharsetProfile") );
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
    // references a custom charset id that was never loaded/no longer exists
    settings.setValue( QStringLiteral("Charset"), QStringLiteral("nonexistentcustomcharset") );
    settings.endGroup();
  }

  KomportApp *win = new KomportApp();
  win->show();
  QPointer<KomportApp> guard(win);
  KomportView *view = win->findChild<KomportView *>();
  QVERIFY( view != nullptr );

  win->loadProfile( QStringLiteral("DanglingCharsetProfile") );

  QCOMPARE( view->mEmulation->charset(), KomportCharset::Standard ); // already worked before this fix
  QCOMPARE( win->strCharset, KomportCharset::settingsKey(KomportCharset::Standard) ); // this is what round-3 fixed

  win->close();
  QTRY_VERIFY( guard.isNull() );
}

void TstCharset::customCharsetCapCountsExaminedFilesNotJustSuccessful()
{
  // Codex review round-3 finding (Medium): the original cap only counted
  // *successfully registered* entries, so files that fail to load (too
  // large, corrupt, id-colliding, ...) didn't count against it -
  // arbitrarily many such files could still be opened and parsed before
  // the cap ever took effect. This is the one test that can actually tell
  // "examined" and "successful" apart: 3 deliberately oversized (thus
  // failing) files, alphabetically first, followed by 257 valid ones -
  // under the fixed behavior the failures consume 3 of the 256-file
  // examination budget, so only 253 end up successfully registered, not
  // 256. The old, buggy "only count successes" cap would have let the
  // loop keep going past the 3 failures and still land on exactly 256
  // registered - so this specific count (253, not 256) is what
  // distinguishes the two implementations from each other.
  const QString dir = KomportCharset::customCharsetsDirectory();
  {
    QDir d(dir);
    for ( const QString &leftover : d.entryList(QStringList{QStringLiteral("*.charset")}, QDir::Files) ) {
      QFile::remove( d.filePath(leftover) );
    }
  }

  QStringList createdPaths;
  // 3 files, alphabetically before "valid...", each over the 1 MiB size
  // limit so loadCustomCharsetFile() rejects them outright.
  for ( int i = 0; i < 3; ++i ) {
    const QString filePath = dir + QStringLiteral("/aaafail%1.charset").arg(i);
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly) );
    QByteArray oversized( 1024 * 1024 + 1024, 'a' );
    QVERIFY( f.write(oversized) == oversized.size() );
    createdPaths << filePath;
  }
  // 257 valid files, alphabetically after the 3 failing ones.
  for ( int i = 0; i < 257; ++i ) {
    const QString filePath = dir + QStringLiteral("/validcap%1.charset").arg(i, 3, 10, QLatin1Char('0'));
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly | QIODevice::Text) );
    QTextStream out(&f);
    out << "41=2588\n";
    createdPaths << filePath;
  }
  auto cleanup = qScopeGuard( [&createdPaths]() {
    for ( const QString &p : createdPaths ) QFile::remove(p);
    KomportCharset::reloadCustomCharsets();
  } );

  KomportCharset::reloadCustomCharsets();
  const auto customs = KomportCharset::customCharsetEntries();
  QCOMPARE( customs.size(), 253 ); // 256-file examination budget minus the 3 failures, not 256
}

void TstCharset::customCharsetLineExactlyAtLengthLimitIsAccepted()
{
  // Codex review round-4 finding (Low): the round-3 fix's "a returned
  // chunk >= the length limit means this physical line was split" rule
  // had a false positive - QTextStream::readLine(maxlen) returns a chunk
  // of exactly maxlen characters both when a longer line got split AND
  // when a physical line just happens to be exactly maxlen characters
  // long, so the old code rejected the second, entirely legitimate case
  // too. The round-4 fix reads the file into memory and splits on '\n'
  // directly instead, giving each physical line's real, unambiguous
  // length. Uses a "# Name: ..." line built to be exactly
  // MaxCustomCharsetLineLength (4096) characters long - if it were
  // wrongly rejected as "too long", the fallback (filename-as-)id would
  // be what shows up as the display name instead.
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString filePath = dir + QStringLiteral("/tstexactlimit.charset");
  const QString namePrefix = QStringLiteral("# Name: "); // 8 characters
  const int fillerLength = 4096 - namePrefix.size(); // exactly 4096 chars total for the whole line
  const QString filler( fillerLength, QLatin1Char('X') );
  QVERIFY( QStringLiteral("%1%2").arg(namePrefix, filler).size() == 4096 );
  {
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly) );
    const QByteArray line1 = QStringLiteral("%1%2\n").arg(namePrefix, filler).toUtf8();
    QCOMPARE( line1.size(), 4097 ); // 4096 chars + the trailing '\n' itself
    QVERIFY( f.write(line1) == line1.size() );
    const QByteArray line2 = "41=2588\n";
    QVERIFY( f.write(line2) == line2.size() );
  }
  auto cleanup = qScopeGuard( [&filePath]() {
    QFile::remove(filePath);
    KomportCharset::reloadCustomCharsets();
  } );
  KomportCharset::reloadCustomCharsets();

  const auto customs = KomportCharset::customCharsetEntries();
  auto it = std::find_if( customs.begin(), customs.end(),
      []( const QPair<QString,QString> &e ) { return e.first == QStringLiteral("tstexactlimit"); } );
  QVERIFY2( it != customs.end(), "the file must still load" );
  QCOMPARE( it->second, filler ); // the exactly-4096-char "# Name: ..." line must have been parsed, not skipped as over-length
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0x41, QStringLiteral("tstexactlimit")), QChar(0x2588) ); // the following line still parses too
}

void TstCharset::newWindowConstructionReconcilesOtherOpenWindows()
{
  // Codex review round-4 finding (Medium): the KomportApp constructor
  // calls KomportCharset::reloadCustomCharsets() directly (to have the
  // registry populated before its own profile load) - just as capable of
  // invalidating an *already open* window's live Custom selection as the
  // reload in slotShowPreferences() is, but wasn't followed by a
  // reconcile-all-windows call. Unlike multiWindowCustomCharsetSelectionIsReconciledInAllWindows()
  // above (which calls the reconcile helper directly), this one
  // reproduces the *real* code path: winA's selection must be fixed up
  // purely as a side effect of *constructing* winB, without anything
  // calling a reconcile method on winA directly.
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString filePath = dir + QStringLiteral("/tstnewwin.charset");
  {
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly | QIODevice::Text) );
    QTextStream out(&f);
    out << "41=2588\n";
  }
  KomportCharset::reloadCustomCharsets();

  KomportApp *winA = new KomportApp();
  winA->show();
  QPointer<KomportApp> guardA(winA);
  KomportView *viewA = winA->findChild<KomportView *>();
  QVERIFY( viewA != nullptr );

  viewA->mEmulation->setCharset( KomportCharset::Custom, QStringLiteral("tstnewwin") );
  winA->strCharset = QStringLiteral("tstnewwin");
  QCOMPARE( viewA->mEmulation->charset(), KomportCharset::Custom ); // sanity check on the setup itself

  QFile::remove(filePath); // the file winA is relying on disappears

  // Constructing winB reloads the shared registry as one of its very
  // first steps (before this call returns) - winA must come out of this
  // reconciled too, even though nothing touched winA directly.
  KomportApp *winB = new KomportApp();
  winB->show();
  QPointer<KomportApp> guardB(winB);

  QCOMPARE( viewA->mEmulation->charset(), KomportCharset::Standard );
  QCOMPARE( winA->strCharset, KomportCharset::settingsKey(KomportCharset::Standard) );

  winA->close();
  winB->close();
  QTRY_VERIFY( guardA.isNull() );
  QTRY_VERIFY( guardB.isNull() );
  KomportCharset::reloadCustomCharsets(); // leave the registry clean for later tests
}

void TstCharset::customCharsetAmbiguousReverseMappingIsWarnedAndLowerByteWins()
{
  // Codex review round-5 finding (High - CNC-transfer-relevant, see
  // reloadCustomCharsets()'s comment on why this matters beyond display):
  // a custom table mapping two different bytes to the same displayed
  // character used to silently pick "whichever byte the tie-break happens
  // to favor" for the TX direction, with nothing telling the file's
  // author their table was ambiguous. Now warns. The reverse table is
  // built by scanning bytes 0..255 in *numeric* order regardless of which
  // order the two mapping lines appear in the file, so the numerically
  // lower byte (0x01 here) always wins the tie-break, deterministically.
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString filePath = dir + QStringLiteral("/tstambiguous.charset");
  {
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly | QIODevice::Text) );
    QTextStream out(&f);
    out << "41=2588\n"; // 'A' (0x41) -> full block
    out << "01=2588\n"; // SOH (0x01) -> the SAME full block - ambiguous
  }
  auto cleanup = qScopeGuard( [&filePath]() {
    QFile::remove(filePath);
    KomportCharset::reloadCustomCharsets();
  } );
  // Codex review round-6 finding: an earlier version of this message
  // mixed hex and decimal for the byte values across the same message
  // (some printed via Qt::hex, the final one left in decimal after a
  // stray Qt::dec) - this regex pins that every byte value in the
  // message uses the same "0x..." hex form throughout, not just that the
  // word "ambiguous" appears somewhere.
  QTest::ignoreMessage( QtWarningMsg, QRegularExpression(
      QStringLiteral("both byte 0x1 and byte 0x41 map to the same character U\\+2588 - ambiguous table.*"
                      "transmit byte 0x1 \\(the numerically lower one\\), not byte 0x41")) );
  KomportCharset::reloadCustomCharsets();

  // both bytes still display as the same glyph...
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0x41, QStringLiteral("tstambiguous")), QChar(0x2588) );
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0x01, QStringLiteral("tstambiguous")), QChar(0x2588) );
  // ...but sending that glyph back is deterministic: the numerically lower byte wins, not whichever line came first in the file.
  QCOMPARE( KomportCharset::toWire(KomportCharset::Custom, QChar(0x2588), QStringLiteral("tstambiguous")), static_cast<char>(0x01) );
}

void TstCharset::customCharsetLineCountCapStopsProcessingWithoutCrashing()
{
  // Codex review round-4/round-5 findings: MaxCustomCharsetLinesRead
  // (100000) must actually stop parsing at that many physical lines -
  // and, after the round-5 fix replacing raw.split('\n') with a manual
  // indexOf('\n', ...) scan, must do so without ever materializing every
  // line up front. A real mapping line placed well past the cap must NOT
  // take effect; this only demonstrates correctness (not the internal
  // materialization behavior itself, which isn't observable from the
  // test's outside view), but a real hang/crash here would still fail
  // the test via QTest's own timeout.
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString filePath = dir + QStringLiteral("/tstlinecap.charset");
  constexpr int kBlankLines = 100010; // safely past the 100000-line cap
  {
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly) );
    QByteArray blankLines;
    blankLines.reserve(kBlankLines);
    for ( int i = 0; i < kBlankLines; ++i ) blankLines += '\n';
    QVERIFY( f.write(blankLines) == blankLines.size() );
    const QByteArray mapping = "41=2588\n"; // past the cap - must be ignored
    QVERIFY( f.write(mapping) == mapping.size() );
  }
  auto cleanup = qScopeGuard( [&filePath]() {
    QFile::remove(filePath);
    KomportCharset::reloadCustomCharsets();
  } );
  KomportCharset::reloadCustomCharsets();

  QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0x41, QStringLiteral("tstlinecap")), QChar('A') ); // identity - the mapping line past the cap never took effect
}

void TstCharset::customCharsetLineLengthMeasuredInCharactersNotUtf8BytesOrCrlf()
{
  // Codex review round-5 finding: measuring a physical line's raw UTF-8
  // *byte* length against MaxCustomCharsetLineLength (4096) instead of its
  // actual *character* length wrongly rejected two entirely legitimate
  // cases:
  const QString dir = KomportCharset::customCharsetsDirectory();

  // (1) a line well under 4096 *characters* that contains multi-byte
  // UTF-8 characters, so its *byte* length exceeds 4096 - 1400 repeats of
  // a 3-byte-in-UTF-8 CJK character is 1400 characters but 4200 bytes.
  {
    const QString multibytePath = dir + QStringLiteral("/tstmultibyte.charset");
    const QString name = QString(1400, QChar(0x6F22)); // U+6F22, 3 bytes in UTF-8 -> 4200 bytes, 1400 chars
    QVERIFY( name.size() < 4096 ); // MaxCustomCharsetLineLength in komportcharset.cpp
    QVERIFY( name.toUtf8().size() > 4096 );
    {
      QFile f(multibytePath);
      QVERIFY( f.open(QIODevice::WriteOnly) );
      const QByteArray line1 = (QStringLiteral("# Name: ") + name + QStringLiteral("\n")).toUtf8();
      QVERIFY( f.write(line1) == line1.size() );
      const QByteArray line2 = "41=2588\n";
      QVERIFY( f.write(line2) == line2.size() );
    }
    auto cleanup = qScopeGuard( [&multibytePath]() {
      QFile::remove(multibytePath);
      KomportCharset::reloadCustomCharsets();
    } );
    KomportCharset::reloadCustomCharsets();

    const auto customs = KomportCharset::customCharsetEntries();
    auto it = std::find_if( customs.begin(), customs.end(),
        []( const QPair<QString,QString> &e ) { return e.first == QStringLiteral("tstmultibyte"); } );
    QVERIFY2( it != customs.end(), "the file must still load" );
    QCOMPARE( it->second, name ); // the "# Name: ..." line must have been parsed, not skipped as over-length
  }

  // (2) a line of exactly 4096 *characters* saved with CRLF line endings -
  // splitting only on '\n' leaves a trailing '\r', which must not be
  // counted against the length limit (trimmed() removes it, same as any
  // other trailing whitespace).
  {
    const QString crlfPath = dir + QStringLiteral("/tstcrlf.charset");
    const QString namePrefix = QStringLiteral("# Name: ");
    const QString filler( 4096 - namePrefix.size(), QLatin1Char('X') );
    {
      QFile f(crlfPath);
      QVERIFY( f.open(QIODevice::WriteOnly) );
      const QByteArray line1 = (namePrefix + filler).toUtf8() + "\r\n"; // CRLF
      QVERIFY( f.write(line1) == line1.size() );
      const QByteArray line2 = "41=2588\r\n";
      QVERIFY( f.write(line2) == line2.size() );
    }
    auto cleanup = qScopeGuard( [&crlfPath]() {
      QFile::remove(crlfPath);
      KomportCharset::reloadCustomCharsets();
    } );
    KomportCharset::reloadCustomCharsets();

    const auto customs = KomportCharset::customCharsetEntries();
    auto it = std::find_if( customs.begin(), customs.end(),
        []( const QPair<QString,QString> &e ) { return e.first == QStringLiteral("tstcrlf"); } );
    QVERIFY2( it != customs.end(), "the file must still load" );
    QCOMPARE( it->second, filler );
    QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0x41, QStringLiteral("tstcrlf")), QChar(0x2588) );
  }
}

void TstCharset::customCharsetUnrepresentableCharacterUsesTablesOwnQuestionMarkByte()
{
  // Codex review round-6 finding (High - CNC-transfer-relevant): an
  // unrepresentable character under a custom charset used to fall back
  // to the literal byte 0x3F unconditionally - wrong whenever the table
  // itself redefines what 0x3F displays as. This table redefines 0x3F to
  // display as a full block (not '?') and gives byte 0x40 the job of
  // displaying '?' instead - an unrepresentable character must now come
  // back as 0x40 (this table's own byte for '?'), not the old, wrong 0x3F.
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString filePath = dir + QStringLiteral("/tstquestionmark.charset");
  {
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly | QIODevice::Text) );
    QTextStream out(&f);
    out << "3F=2588\n"; // byte 0x3F no longer means '?' under this table - it means a full block
    out << "40=003F\n"; // byte 0x40 is what THIS table uses to display '?'
  }
  auto cleanup = qScopeGuard( [&filePath]() {
    QFile::remove(filePath);
    KomportCharset::reloadCustomCharsets();
  } );
  KomportCharset::reloadCustomCharsets();

  // sanity check on the setup: '?' really does display as 0x40 here, and 0x3F really does NOT display as '?'.
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0x40, QStringLiteral("tstquestionmark")), QChar('?') );
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0x3F, QStringLiteral("tstquestionmark")), QChar(0x2588) );

  const QChar farOutside(0x0100); // well outside what this table can represent - only 0x3F/0x40 are overridden, everything else stays identity (max byte 0xFF)
  QCOMPARE( KomportCharset::toWire(KomportCharset::Custom, farOutside, QStringLiteral("tstquestionmark")), static_cast<char>(0x40) );
}

void TstCharset::customCharsetLineCountCapExactBoundaryIsPrecise()
{
  // Codex review round-6 finding (Low, test quality): the previous
  // line-count-cap test only checked that a mapping placed *way* past the
  // cap didn't take effect - which would also pass if the whole file were
  // rejected outright, or if the real cutoff were badly wrong in either
  // direction. Puts distinct, verifiable mappings exactly ON the cap
  // (physical line 100000) and just past it (line 100001): the first
  // must take effect, the second must not, and the file must genuinely
  // have loaded (not been rejected wholesale).
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString filePath = dir + QStringLiteral("/tstlineboundary.charset");
  constexpr int kBlankLinesBeforeBoundary = 99999; // lines 1..99999
  {
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly) );
    QByteArray content;
    content.reserve(kBlankLinesBeforeBoundary + 20);
    for ( int i = 0; i < kBlankLinesBeforeBoundary; ++i ) content += '\n';
    content += "41=2588\n"; // physical line 100000 - exactly at the cap, must take effect
    content += "42=0041\n"; // physical line 100001 - one past the cap, must NOT take effect
    QVERIFY( f.write(content) == content.size() );
  }
  auto cleanup = qScopeGuard( [&filePath]() {
    QFile::remove(filePath);
    KomportCharset::reloadCustomCharsets();
  } );
  QTest::ignoreMessage( QtWarningMsg, QRegularExpression(QStringLiteral("more than 100000 lines")) );
  KomportCharset::reloadCustomCharsets();

  const auto customs = KomportCharset::customCharsetEntries();
  auto it = std::find_if( customs.begin(), customs.end(),
      []( const QPair<QString,QString> &e ) { return e.first == QStringLiteral("tstlineboundary"); } );
  QVERIFY2( it != customs.end(), "the file must genuinely have loaded, not been rejected wholesale" );
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0x41, QStringLiteral("tstlineboundary")), QChar(0x2588) ); // line 100000 - at the cap, took effect
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0x42, QStringLiteral("tstlineboundary")), QChar('B') );    // line 100001 - past the cap, identity
}

void TstCharset::customCharsetLineLengthCountsCodePointsNotUtf16Units()
{
  // Codex review round-6 finding (Low): QString::size() counts UTF-16
  // *code units*, not Unicode code points - a non-BMP ("astral") code
  // point occupies two UTF-16 units (a surrogate pair) despite being one
  // real character, so measuring line.size() alone could reject a
  // "# Name: ..." line that's actually well within the real 4096-
  // character limit. 2100 repeats of an astral emoji (U+1F600) is 2100
  // real characters but 4200 UTF-16 units.
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString filePath = dir + QStringLiteral("/tstastral.charset");
  const QVector<char32_t> codepoints( 2100, static_cast<char32_t>(0x1F600) );
  const QString name = QString::fromUcs4( codepoints.constData(), codepoints.size() );
  QVERIFY( name.size() > 4096 );          // UTF-16 code units - over the limit if measured naively
  QCOMPARE( name.toUcs4().size(), 2100 ); // real code-point count - comfortably under the limit
  {
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly) );
    const QByteArray line1 = (QStringLiteral("# Name: ") + name + QStringLiteral("\n")).toUtf8();
    QVERIFY( f.write(line1) == line1.size() );
    const QByteArray line2 = "41=2588\n";
    QVERIFY( f.write(line2) == line2.size() );
  }
  auto cleanup = qScopeGuard( [&filePath]() {
    QFile::remove(filePath);
    KomportCharset::reloadCustomCharsets();
  } );
  KomportCharset::reloadCustomCharsets();

  const auto customs = KomportCharset::customCharsetEntries();
  auto it = std::find_if( customs.begin(), customs.end(),
      []( const QPair<QString,QString> &e ) { return e.first == QStringLiteral("tstastral"); } );
  QVERIFY2( it != customs.end(), "the file must still load" );
  QCOMPARE( it->second, name ); // the "# Name: ..." line must have been parsed, not skipped as over-length
}

void TstCharset::customCharsetMalformedUtf8IsRejected()
{
  // Codex review round-6 finding (Low): the NUL-byte sniff only catches
  // UTF-16/UTF-32-*like* binary content - malformed UTF-8 containing no
  // NUL byte at all (e.g. a stray continuation/invalid byte) previously
  // passed straight through, silently "repaired" with U+FFFD by
  // QString::fromUtf8() rather than being rejected, weaker than the
  // documented "must be UTF-8" contract. 0xFF is never valid in any
  // position of a UTF-8 byte sequence.
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString filePath = dir + QStringLiteral("/tstbadutf8.charset");
  {
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly) );
    QByteArray content = "41=2588\n";
    content += static_cast<char>(0xFF);
    content += '\n';
    QVERIFY( f.write(content) == content.size() );
  }
  auto cleanup = qScopeGuard( [&filePath]() {
    QFile::remove(filePath);
    KomportCharset::reloadCustomCharsets();
  } );
  KomportCharset::reloadCustomCharsets();

  const auto customs = KomportCharset::customCharsetEntries();
  auto it = std::find_if( customs.begin(), customs.end(),
      []( const QPair<QString,QString> &e ) { return e.first == QStringLiteral("tstbadutf8"); } );
  QVERIFY2( it == customs.end(), "a file containing invalid UTF-8 must be rejected entirely, not partially repaired and loaded" );
}

void TstCharset::customCharsetExactly100000LinesWithTrailingNewlineDoesNotWarn()
{
  // Codex review round-6 finding (Low): a file with an ordinary single
  // trailing newline - the completely normal case, not a deliberately
  // authored blank final line - used to produce a spurious, misleading
  // "has more than 100000 lines" warning right at the exact 100000-line
  // boundary, purely an artifact of how the manual line scanner detected
  // "no more input" (see the fix's own comment in loadCustomCharsetFile()).
  // No data was ever actually lost by this, only the diagnostic was
  // wrong - verified here via a temporary message handler rather than
  // QTest::ignoreMessage (which only asserts a message DOES occur, there
  // is no built-in "assert this message does NOT occur").
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString filePath = dir + QStringLiteral("/tstexactcap.charset");
  {
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly) );
    QByteArray content;
    content.reserve(100000);
    for ( int i = 0; i < 100000; ++i ) content += '\n'; // exactly 100000 lines, one ordinary trailing newline
    QVERIFY( f.write(content) == content.size() );
  }
  auto cleanup = qScopeGuard( [&filePath]() {
    QFile::remove(filePath);
    KomportCharset::reloadCustomCharsets();
  } );

  static bool sSawLineCountWarning = false;
  sSawLineCountWarning = false;
  const QtMessageHandler previousHandler = qInstallMessageHandler(
      []( QtMsgType _type, const QMessageLogContext &, const QString &_msg ) {
        if ( _type == QtWarningMsg && _msg.contains(QStringLiteral("more than 100000 lines")) ) sSawLineCountWarning = true;
      } );
  KomportCharset::reloadCustomCharsets();
  qInstallMessageHandler(previousHandler);

  QVERIFY2( !sSawLineCountWarning, "a file with exactly 100000 lines and one ordinary trailing newline must not warn about exceeding the line-count cap" );
}

void TstCharset::customCharsetQuestionMarkFallbackWarnsWhenTableHasNoQuestionMarkByteEither()
{
  // Codex review round-7 finding (High - the remaining gap in round-6's
  // '?'-fallback fix): a table that redefines byte 0x3F to something
  // else (removing its identity mapping to '?') AND provides no OTHER
  // byte for '?' either leaves toWire()'s lookup of U+003F itself
  // missing - the fallback then has no genuinely correct byte to offer
  // (every one of the table's 256 outgoing bytes is already claimed by
  // something), so literal 0x3F remains the last resort, but now with a
  // warning making the situation diagnosable instead of silent.
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString filePath = dir + QStringLiteral("/tstnoqmark.charset");
  {
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly | QIODevice::Text) );
    QTextStream out(&f);
    out << "3F=2588\n"; // 0x3F no longer means '?' under this table - and nothing else is assigned to '?' either
  }
  auto cleanup = qScopeGuard( [&filePath]() {
    QFile::remove(filePath);
    KomportCharset::reloadCustomCharsets();
  } );
  KomportCharset::reloadCustomCharsets();

  // sanity check: '?' (U+003F) really is unrepresentable under this table now.
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::Custom, 0x3F, QStringLiteral("tstnoqmark")), QChar(0x2588) );

  const QChar farOutside(0x0100); // unrepresentable
  QTest::ignoreMessage( QtWarningMsg, QRegularExpression(QStringLiteral("no byte in this table represents '\\?'")) );
  QCOMPARE( KomportCharset::toWire(KomportCharset::Custom, farOutside, QStringLiteral("tstnoqmark")), static_cast<char>(0x3F) ); // last resort, now warned
}

void TstCharset::customCharsetTruncatedUtf8AtEndOfFileIsRejected()
{
  // Codex review round-7 finding (Low): QStringDecoder defaults to
  // stateful, streaming-oriented behavior - an incomplete multi-byte
  // sequence right at the very end of the (whole, one-shot) input was
  // held as "might be completed by a later chunk" rather than flagged as
  // an error, so a file truncated mid-character still loaded. A lone
  // UTF-8 lead byte (0xC3, which promises exactly one continuation byte)
  // with nothing after it is exactly that case.
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString filePath = dir + QStringLiteral("/tsttruncated.charset");
  {
    QFile f(filePath);
    QVERIFY( f.open(QIODevice::WriteOnly) );
    QByteArray content = "41=2588\n";
    content += static_cast<char>(0xC3); // valid UTF-8 lead byte, but the file ends right here - no continuation byte
    QVERIFY( f.write(content) == content.size() );
  }
  auto cleanup = qScopeGuard( [&filePath]() {
    QFile::remove(filePath);
    KomportCharset::reloadCustomCharsets();
  } );
  KomportCharset::reloadCustomCharsets();

  const auto customs = KomportCharset::customCharsetEntries();
  auto it = std::find_if( customs.begin(), customs.end(),
      []( const QPair<QString,QString> &e ) { return e.first == QStringLiteral("tsttruncated"); } );
  QVERIFY2( it == customs.end(), "a file truncated mid-character at EOF must be rejected as invalid UTF-8, not silently accepted" );
}

void TstCharset::customCharsetAstralLineLengthExactBoundaryIsPrecise()
{
  // Codex review round-7 finding (Low, test quality): the round-6 astral
  // line-length test used 2100 emoji (2100 code points), comfortably
  // under the 4096 limit - it proved UTF-16 units were no longer counted
  // directly, but not that the toUcs4() fallback is itself correct AT
  // its own boundary. Pins that exactly 4096 astral code points are
  // accepted and 4097 are rejected, mirroring
  // customCharsetLineExactlyAtLengthLimitIsAccepted()'s BMP boundary test.
  const QString dir = KomportCharset::customCharsetsDirectory();
  const QString namePrefix = QStringLiteral("# Name: ");
  const int prefixCodePoints = 8; // "# Name: " is 8 plain ASCII characters/code points

  auto makeAstralName = [&]( int _codePointCount ) {
    const QVector<char32_t> codepoints( _codePointCount, static_cast<char32_t>(0x1F600) );
    return QString::fromUcs4( codepoints.constData(), codepoints.size() );
  };

  {
    const QString atLimitPath = dir + QStringLiteral("/tstastrallimit.charset");
    const QString name = makeAstralName( 4096 - prefixCodePoints ); // whole line == exactly 4096 code points
    QCOMPARE( (namePrefix + name).toUcs4().size(), 4096 );
    {
      QFile f(atLimitPath);
      QVERIFY( f.open(QIODevice::WriteOnly) );
      const QByteArray line1 = (namePrefix + name + QStringLiteral("\n")).toUtf8();
      QVERIFY( f.write(line1) == line1.size() );
      const QByteArray line2 = "41=2588\n";
      QVERIFY( f.write(line2) == line2.size() );
    }
    auto cleanup = qScopeGuard( [&atLimitPath]() {
      QFile::remove(atLimitPath);
      KomportCharset::reloadCustomCharsets();
    } );
    KomportCharset::reloadCustomCharsets();

    const auto customs = KomportCharset::customCharsetEntries();
    auto it = std::find_if( customs.begin(), customs.end(),
        []( const QPair<QString,QString> &e ) { return e.first == QStringLiteral("tstastrallimit"); } );
    QVERIFY2( it != customs.end(), "the file must still load" );
    QCOMPARE( it->second, name ); // exactly 4096 code points must have been accepted, not rejected
  }
  {
    const QString overLimitPath = dir + QStringLiteral("/tstastralover.charset");
    const QString name = makeAstralName( 4097 - prefixCodePoints ); // whole line == exactly 4097 code points
    QCOMPARE( (namePrefix + name).toUcs4().size(), 4097 );
    {
      QFile f(overLimitPath);
      QVERIFY( f.open(QIODevice::WriteOnly) );
      const QByteArray line1 = (namePrefix + name + QStringLiteral("\n")).toUtf8();
      QVERIFY( f.write(line1) == line1.size() );
      const QByteArray line2 = "41=2588\n";
      QVERIFY( f.write(line2) == line2.size() );
    }
    auto cleanup = qScopeGuard( [&overLimitPath]() {
      QFile::remove(overLimitPath);
      KomportCharset::reloadCustomCharsets();
    } );
    KomportCharset::reloadCustomCharsets();

    const auto customs = KomportCharset::customCharsetEntries();
    auto it = std::find_if( customs.begin(), customs.end(),
        []( const QPair<QString,QString> &e ) { return e.first == QStringLiteral("tstastralover"); } );
    QVERIFY2( it != customs.end(), "the file itself must still load (only the # Name: line is skipped)" );
    QCOMPARE( it->second, QStringLiteral("tstastralover") ); // 4097 code points rejected -> falls back to the filename-derived default display name
  }
}

QTEST_MAIN(TstCharset)
#include "tst_charset.moc"
