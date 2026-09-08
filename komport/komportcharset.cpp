/***************************************************************************
                          komportcharset.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : 2026 (new in the Qt6 port, Milestone 7)
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de
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

#include <QMap>

namespace {

/** IBM PC / MS-DOS code page 437 ("OEM-US"), the full 256-entry table.
 *  This is one of the most stable, widely-referenced character encodings
 *  in computing history (the original IBM PC ROM font) - byte value is
 *  the array index, value is the Unicode code point. 0x00-0x1F and 0x7F
 *  include CP437's famous "control picture" glyphs (smileys, card suits,
 *  arrows, ...) - at runtime, KomportEmulation::slotReceivedChar() only
 *  ever reaches this table for the byte values it doesn't already handle
 *  as actual control codes itself (BEL 0x07, BS 0x08, HT 0x09, LF 0x0A,
 *  CR 0x0D, ESC 0x1B all stay control codes exactly as under "Standard"
 *  regardless of charset - see the header comment) - the table still
 *  carries "authentic" CP437 entries for those six positions rather than
 *  leaving gaps, purely so it reads as a complete, textbook-accurate
 *  CP437 table and is directly testable byte-for-byte, even though six
 *  of its 256 entries are provably unreachable through the real RX path.
 *  0x00 maps to a plain space rather than CP437's traditional "blank"
 *  glyph for NUL, to avoid putting an embedded U+0000 into a QString. */
const char16_t kCp437[256] = {
  // 0x00-0x0F
  0x0020,0x263A,0x263B,0x2665,0x2666,0x2663,0x2660,0x2022,
  0x25D8,0x25CB,0x25D9,0x2642,0x2640,0x266A,0x266B,0x263C,
  // 0x10-0x1F
  0x25BA,0x25C4,0x2195,0x203C,0x00B6,0x00A7,0x25AC,0x21A8,
  0x2191,0x2193,0x2192,0x2190,0x221F,0x2194,0x25B2,0x25BC,
  // 0x20-0x2F
  0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,
  0x0028,0x0029,0x002A,0x002B,0x002C,0x002D,0x002E,0x002F,
  // 0x30-0x3F
  0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,
  0x0038,0x0039,0x003A,0x003B,0x003C,0x003D,0x003E,0x003F,
  // 0x40-0x4F
  0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,
  0x0048,0x0049,0x004A,0x004B,0x004C,0x004D,0x004E,0x004F,
  // 0x50-0x5F
  0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,
  0x0058,0x0059,0x005A,0x005B,0x005C,0x005D,0x005E,0x005F,
  // 0x60-0x6F
  0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,
  0x0068,0x0069,0x006A,0x006B,0x006C,0x006D,0x006E,0x006F,
  // 0x70-0x7F
  0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,
  0x0078,0x0079,0x007A,0x007B,0x007C,0x007D,0x007E,0x2302,
  // 0x80-0x8F
  0x00C7,0x00FC,0x00E9,0x00E2,0x00E4,0x00E0,0x00E5,0x00E7,
  0x00EA,0x00EB,0x00E8,0x00EF,0x00EE,0x00EC,0x00C4,0x00C5,
  // 0x90-0x9F
  0x00C9,0x00E6,0x00C6,0x00F4,0x00F6,0x00F2,0x00FB,0x00F9,
  0x00FF,0x00D6,0x00DC,0x00A2,0x00A3,0x00A5,0x20A7,0x0192,
  // 0xA0-0xAF
  0x00E1,0x00ED,0x00F3,0x00FA,0x00F1,0x00D1,0x00AA,0x00BA,
  0x00BF,0x2310,0x00AC,0x00BD,0x00BC,0x00A1,0x00AB,0x00BB,
  // 0xB0-0xBF
  0x2591,0x2592,0x2593,0x2502,0x2524,0x2561,0x2562,0x2556,
  0x2555,0x2563,0x2551,0x2557,0x255D,0x255C,0x255B,0x2510,
  // 0xC0-0xCF
  0x2514,0x2534,0x252C,0x251C,0x2500,0x253C,0x255E,0x255F,
  0x255A,0x2554,0x2569,0x2566,0x2560,0x2550,0x256C,0x2567,
  // 0xD0-0xDF
  0x2568,0x2564,0x2565,0x2559,0x2558,0x2552,0x2553,0x256B,
  0x256A,0x2518,0x250C,0x2588,0x2584,0x258C,0x2590,0x2580,
  // 0xE0-0xEF
  0x03B1,0x00DF,0x0393,0x03C0,0x03A3,0x03C3,0x00B5,0x03C4,
  0x03A6,0x0398,0x03A9,0x03B4,0x221E,0x03C6,0x03B5,0x2229,
  // 0xF0-0xFF
  0x2261,0x00B1,0x2265,0x2264,0x2320,0x2321,0x00F7,0x2248,
  0x00B0,0x2219,0x00B7,0x221A,0x207F,0x00B2,0x25A0,0x00A0,
};

/** PETSCII, unshifted/"graphics" mode - deliberately partial, see header
 *  comment on KomportCharset::Id::PETSCII for why the 0x60-0x7F/0xA0-0xFF
 *  block-graphics range isn't included here. Only the entries that
 *  differ from plain ASCII identity are listed; everything else in
 *  0x20-0x5F (digits, most punctuation, '@', uppercase letters) already
 *  matches ASCII byte-for-byte, so toDisplay()/toWire() fall through to
 *  identity/toLatin1() for those without needing a table entry. */
const QMap<unsigned char, char16_t> &petsciiForward()
{
  static const QMap<unsigned char, char16_t> table{
    { 0x5C, 0x00A3 }, // £ (British pound) instead of ASCII backslash
    { 0x5E, 0x2191 }, // ↑ (up-arrow) instead of ASCII caret
    { 0x5F, 0x2190 }, // ← (left-arrow) instead of ASCII underscore
  };
  return table;
}

/** reverse of petsciiForward(), built once - Unicode char -> PETSCII byte */
const QMap<char16_t, unsigned char> &petsciiReverse()
{
  static const QMap<char16_t, unsigned char> table = [] {
    QMap<char16_t, unsigned char> r;
    const auto &fwd = petsciiForward();
    for ( auto it = fwd.constBegin(); it != fwd.constEnd(); ++it ) {
      r.insert( it.value(), it.key() );
    }
    return r;
  }();
  return table;
}

/** reverse of kCp437, built once - Unicode char -> CP437 byte. Several
 *  CP437 code points repeat (e.g. duplicate box-drawing corners aren't
 *  actually duplicated here, but 0x20 and 0x00 both map to space) - a
 *  QMap insert keeps whichever byte was inserted *first*; since the loop
 *  runs in ascending byte order, that's always the lowest/first-defined
 *  byte for a given code point, a stable and reasonable tie-break. */
const QMap<char16_t, unsigned char> &cp437Reverse()
{
  static const QMap<char16_t, unsigned char> table = [] {
    QMap<char16_t, unsigned char> r;
    for ( int b = 0; b < 256; ++b ) {
      const char16_t u = kCp437[b];
      if ( !r.contains(u) ) r.insert( u, static_cast<unsigned char>(b) );
    }
    return r;
  }();
  return table;
}

} // namespace

QChar KomportCharset::toDisplay(Id _charset, unsigned char _rawByte)
{
  switch ( _charset ) {
    case CP437:
      return QChar( kCp437[_rawByte] );
    case PETSCII:
      {
        const auto &fwd = petsciiForward();
        auto it = fwd.constFind( _rawByte );
        if ( it != fwd.constEnd() ) return QChar( it.value() );
        return QChar( static_cast<uchar>(_rawByte) ); // identity fallback
      }
    case Standard:
    default:
      return QChar( static_cast<uchar>(_rawByte) );
  }
}

char KomportCharset::toWire(Id _charset, QChar _ch)
{
  switch ( _charset ) {
    case CP437:
      {
        const auto &rev = cp437Reverse();
        auto it = rev.constFind( _ch.unicode() );
        if ( it != rev.constEnd() ) return static_cast<char>( it.value() );
        break;
      }
    case PETSCII:
      {
        const auto &rev = petsciiReverse();
        auto it = rev.constFind( _ch.unicode() );
        if ( it != rev.constEnd() ) return static_cast<char>( it.value() );
        break;
      }
    case Standard:
    default:
      break;
  }
  return _ch.toLatin1(); // fallback: today's exact pre-Milestone-7 behavior
}

QStringList KomportCharset::names()
{
  return { QStringLiteral("Standard"), QStringLiteral("IBM CP437"), QStringLiteral("PETSCII") };
}

KomportCharset::Id KomportCharset::fromIndex(int _index)
{
  switch ( _index ) {
    case CP437:   return CP437;
    case PETSCII: return PETSCII;
    default:      return Standard;
  }
}

int KomportCharset::toIndex(Id _charset)
{
  return static_cast<int>( _charset );
}

QString KomportCharset::settingsKey(Id _charset)
{
  switch ( _charset ) {
    case CP437:   return QStringLiteral("CP437");
    case PETSCII: return QStringLiteral("PETSCII");
    case Standard:
    default:      return QStringLiteral("Standard");
  }
}

KomportCharset::Id KomportCharset::fromSettingsKey(const QString &_key)
{
  if ( _key == QStringLiteral("CP437") ) return CP437;
  if ( _key == QStringLiteral("PETSCII") ) return PETSCII;
  return Standard;
}
