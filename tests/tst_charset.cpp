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

QTEST_MAIN(TstCharset)
#include "tst_charset.moc"
