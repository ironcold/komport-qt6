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

#include <QTest>

class TstCharset : public QObject
{
  Q_OBJECT
private slots:
  void standardCharsetIsIdentity();
  void cp437HighRangeMapsToBoxDrawingAndAccents();
  void cp437LowRangeMapsToControlPictureGlyphs();
  void cp437RoundTripsDistinctiveGlyphsBackToWire();
  void petsciiSubstitutesThreePunctuationBytes();
  void petsciiAsciiCompatibleRangePassesThrough();
  void petsciiGraphicsRangeIsDeliberatelyUnmapped();
  void unmappedCharacterFallsBackToLatin1OnWire();
  void namesAndIndexRoundTrip();
  void settingsKeyRoundTrips();
  void emulationTranslatesReceivedBytesThroughSelectedCharset();
  void emulationControlCodesUnaffectedByNonStandardCharset();
  void emulationDefaultsToStandardCharset();
};

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
  // CP437's famous "the low control range doubles as printable glyphs"
  // feature - card suits, smileys, etc. These specific byte values (03,
  // 04, 06) are NOT among the six the emulation itself still treats as
  // real control codes (BEL 07/BS 08/HT 09/LF 0A/CR 0D/ESC 1B), so they
  // ARE reachable through the real RX path - see the integration test
  // below.
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::CP437, 0x01), QChar(0x263A) ); // ☺
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::CP437, 0x03), QChar(0x2665) ); // ♥
  QCOMPARE( KomportCharset::toDisplay(KomportCharset::CP437, 0x06), QChar(0x2660) ); // ♠
}

void TstCharset::cp437RoundTripsDistinctiveGlyphsBackToWire()
{
  QCOMPARE( KomportCharset::toWire(KomportCharset::CP437, QChar(0x2588)), static_cast<char>(0xDB) );
  QCOMPARE( KomportCharset::toWire(KomportCharset::CP437, QChar(0x263A)), static_cast<char>(0x01) );
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

void TstCharset::unmappedCharacterFallsBackToLatin1OnWire()
{
  // U+00FE (þ, thorn) is a real Latin-1 code point but not part of
  // CP437's set of covered code points - toWire() must fall back to
  // .toLatin1() (today's pre-Milestone-7 behavior) rather than e.g.
  // silently sending a wrong/default byte.
  const QChar thorn(0x00FE);
  QCOMPARE( KomportCharset::toWire(KomportCharset::CP437, thorn), thorn.toLatin1() );
  QCOMPARE( KomportCharset::toWire(KomportCharset::CP437, thorn), static_cast<char>(0xFE) );
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

QTEST_MAIN(TstCharset)
#include "tst_charset.moc"
