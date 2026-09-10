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
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTextStream>
#include <QRegularExpression>
#include <QStringConverter>
#include <QDebug>
#include <iterator>
#include <set>
#include <array>

namespace {

/** IBM PC / MS-DOS code page 437 ("OEM-US") - byte value is the array
 *  index, value is the Unicode code point.
 *
 *  Scope correction after a Codex adversarial review of the first version
 *  (which mapped 0x00-0x1F/0x7F to CP437's separate, well-known "control
 *  picture" glyphs - smileys, card suits, arrows, the IBM PC's video-
 *  memory character generator's actual behavior for those byte values):
 *  that range genuinely conflicts with this emulation's own VT100/VT102
 *  control-code interpretation for the *same* byte values it doesn't
 *  already special-case (KomportEmulation::slotReceivedChar() only
 *  intercepts BEL/BS/HT/LF/CR/ESC - every other C0 control byte, plus
 *  DEL, fell through into this table and got drawn as a CP437 glyph
 *  instead of being treated as a control code, e.g. VT/FF (0x0B/0x0C,
 *  which many real hosts use like LF) would draw ♂/♀ and only advance the
 *  cursor by one column instead of doing a line feed). A native DOS text
 *  console never had this conflict (it wasn't simultaneously interpreting
 *  ANSI escape codes over the very same control-byte range), but this
 *  emulation is a VT100/VT102 interpreter first - so 0x00-0x7F is
 *  deliberately identity here (matches "Standard" exactly, no glyphs),
 *  and only the unambiguous 0x80-0xFF extended range is real CP437.
 *  Tracked as a known, documented scope limitation in TODO.md - restoring
 *  the low-range glyphs would need either a dedicated "raw graphics mode"
 *  toggle or per-control-code special-casing, out of scope for this pass.
 *
 *  A custom *.charset file (see reloadCustomCharsets()) that wants those
 *  glyphs back can add them for itself - the file format has no such
 *  built-in restriction, this restriction is specific to the built-in
 *  CP437 table's own scope decision. */
const char16_t kCp437[256] = {
  // 0x00-0x0F
  0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
  0x0008,0x0009,0x000A,0x000B,0x000C,0x000D,0x000E,0x000F,
  // 0x10-0x1F
  0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
  0x0018,0x0019,0x001A,0x001B,0x001C,0x001D,0x001E,0x001F,
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
  0x0078,0x0079,0x007A,0x007B,0x007C,0x007D,0x007E,0x007F,
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

/** reverse of kCp437, built once - Unicode char -> CP437 byte. With the
 *  0x00-0x7F identity range (see kCp437's own comment), every code point
 *  in this table is unique - no duplicate/tie-break concern - but the
 *  "keep the first insert" rule below is still a deliberate, defensive
 *  choice in case that ever stops being true (e.g. a future extension of
 *  the 0x80-0xFF range). Codex review finding (first version of this
 *  table): before the 0x00-0x7F scope correction above, 0x00 and 0x20
 *  both mapped to space, and this "keep first" rule silently made *every*
 *  typed space translate to NUL on the wire (0x00 sorts first) - fixed at
 *  the root by removing the duplicate rather than special-casing the
 *  tie-break, but documented here so it isn't reintroduced by accident. */
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

/** one loaded *.charset file - always a full 256-entry table (unlisted
 *  bytes default to identity, see loadCustomCharsetFile()), same shape
 *  as kCp437 above, plus its own eagerly-built reverse lookup (same
 *  "first insert wins" tie-break as cp437Reverse(), see its comment). */
struct CustomEntry {
  QString id;          // filename stem - also the persisted settings key
  QString displayName; // dropdown label - "# Name: ..." in the file, or id
  std::array<char16_t, 256> forward{};
  QMap<char16_t, unsigned char> reverse;
};

/** in-memory registry of currently loaded custom charsets, rebuilt by
 *  KomportCharset::reloadCustomCharsets(). A function-local static
 *  mutable reference, same idiom already used for the tables above -
 *  this whole class is otherwise stateless/static, this is the one
 *  deliberate exception (the registry has to live somewhere between a
 *  reload and the next one).
 *
 *  Codex review finding: a QVector here (as the first version had) meant
 *  toDisplay()/toWire() linearly scanned every loaded custom charset for
 *  every single translated byte - fine for a handful of files, but an
 *  unnecessary O(n) cost that grows with however many *.charset files
 *  happen to be sitting in the directory, on the hottest path this class
 *  has (one lookup per received/typed byte). QMap<id, CustomEntry> gives
 *  O(log n) lookup by id instead, and - as a side benefit neither
 *  strictly required nor a regression risk - a stable, alphabetically
 *  sorted dropdown order for free, without needing a separate
 *  "remember file order" mechanism on top. */
QMap<QString, CustomEntry> &customRegistry()
{
  static QMap<QString, CustomEntry> registry;
  return registry;
}

/** Codex review finding: loadCustomCharsetFile()/reloadCustomCharsets()
 *  had no bound on file size, individual line length, or how many
 *  *.charset files get loaded - a huge or pathological file/directory
 *  could make every app startup (or every Settings-dialog reopen, which
 *  reloads the registry too) slow or memory-hungry. Same "clamp rather
 *  than trust unbounded input" philosophy already used elsewhere in this
 *  codebase (MaxCtlSequenceLength in komportemulation.cpp,
 *  MaxLineBufferLength in komportsessionlogger.cpp, MaxDimension/MaxCells
 *  in komportcellarray.cpp, ...) - generous enough that no legitimate
 *  hand-written *.charset file (at most 256 real override lines) could
 *  ever hit them, just bounding the pathological case. */
constexpr qint64 MaxCustomCharsetFileSize = 1 * 1024 * 1024; // 1 MiB
constexpr int MaxCustomCharsetLineLength = 4096;
constexpr int MaxCustomCharsetLinesRead = 100000; // defense in depth even if a huge file somehow had short lines
constexpr int MaxCustomCharsetCount = 256;

/** parse one *.charset file into _out. Returns false (logs a qWarning())
 *  only if the file itself couldn't be opened at all - a malformed
 *  individual *line* is logged and skipped, not treated as a reason to
 *  reject the whole file (consistent with this codebase's general
 *  policy of clamping/defaulting bad individual values rather than
 *  failing outright, e.g. KomportApp::loadProfile()'s handling of a
 *  corrupted profile field). See KomportCharset::reloadCustomCharsets()'s
 *  header comment for the format itself. */
bool loadCustomCharsetFile(const QString &_path, CustomEntry &_out)
{
  const QFileInfo info(_path);
  // Codex review finding: an unbounded file could make loading slow/
  // memory-hungry - reject oversized files up front rather than reading
  // them at all. No legitimate hand-written table (at most 256 real
  // override lines) comes anywhere close to this.
  if ( info.size() > MaxCustomCharsetFileSize ) {
    qWarning() << "KomportCharset:" << _path << "is" << info.size()
               << "bytes, exceeding the" << MaxCustomCharsetFileSize
               << "byte limit for a *.charset file - skipping";
    return false;
  }

  QFile file(_path);
  if ( !file.open(QIODevice::ReadOnly) ) {
    qWarning() << "KomportCharset: could not open" << _path << "(" << file.errorString() << ")";
    return false;
  }
  // Codex review round-3 finding: QTextStream::status() after the fact does
  // NOT reliably detect a genuinely binary/malformed-encoding file in this
  // parsing path - Qt's UTF-8 decoder silently substitutes U+FFFD for
  // invalid byte sequences rather than raising a stream error, so a real
  // binary file that happens to decode without a hard I/O error still
  // "succeeds". Reading the whole file up front (size is already capped
  // above, so this is bounded) and rejecting outright if it contains an
  // embedded NUL byte is the standard, simple binary-content heuristic -
  // no legitimate hand-written *.charset text file has any reason to
  // contain one.
  //
  // Codex review round-4 finding: the NUL-byte heuristic also rejects a
  // *legitimate* UTF-16-encoded text file, since ordinary ASCII characters
  // are NUL-padded in that encoding (e.g. 'A' = 0x41 0x00) - true, but this
  // is a deliberate scope decision rather than a bug: the *.charset format
  // is documented (TODO.md, README.md, the auto-written README.txt) only
  // with plain ASCII/UTF-8 examples, and every existing file this codebase
  // ships or the test suite writes is plain UTF-8. Declaring the format
  // UTF-8-only (documented explicitly, see the doc comment on this
  // function's own declaration in komportcharset.h) turns the NUL-byte
  // check from an incomplete binary-content heuristic into a correct
  // encoding-scope check instead.
  // Codex review round-4 finding: the QFileInfo::size() check above happens
  // *before* file.open()/readAll() - a file that grows past the limit in
  // between (replaced/appended to concurrently) wasn't re-checked, and a
  // partial read caused by a genuine I/O error midway through readAll()
  // wasn't detected either, so either case would have silently been parsed
  // as if it were the complete, valid file.
  //
  // Codex review round-5 finding: reading via readAll() and checking the
  // *result's* size afterward still let the read itself consume an
  // unbounded amount if the file kept growing while being read - the
  // TOCTOU acceptance gap was closed, but not the actual resource bound.
  // read(MaxCustomCharsetFileSize + 1) caps what's ever pulled into memory
  // at once, regardless of how large the underlying file grows meanwhile;
  // a result longer than the limit is still rejected exactly as before.
  const QByteArray raw = file.read( MaxCustomCharsetFileSize + 1 );
  if ( raw.size() > MaxCustomCharsetFileSize ) {
    qWarning() << "KomportCharset:" << _path << "grew past the" << MaxCustomCharsetFileSize
               << "byte limit for a *.charset file while being read - skipping";
    return false;
  }
  if ( file.error() != QFile::NoError ) {
    qWarning() << "KomportCharset:" << _path << "- read error (" << file.errorString() << "), skipping";
    return false;
  }
  if ( raw.contains('\0') ) {
    qWarning() << "KomportCharset:" << _path << "- contains an embedded NUL byte, looks binary rather than a text"
                  " *.charset file, skipping";
    return false;
  }
  // Codex review round-6 finding (Low): the NUL-byte sniff above only
  // catches UTF-16/UTF-32-*like* binary content - it says nothing about
  // NUL-free malformed UTF-8, which QString::fromUtf8() (used per-line
  // below) silently repairs with U+FFFD replacement characters rather
  // than rejecting, quietly weaker than the "must be UTF-8" contract this
  // format is documented with (komportcharset.h). QStringDecoder's own
  // error tracking is Qt's actual strict-UTF-8-validation primitive
  // (QString::fromUtf8() has no equivalent) - decode the whole buffer
  // once here and reject the file outright if any invalid sequence was
  // found, rather than only *some* of its lines silently losing content.
  {
    QStringDecoder utf8Decoder( QStringConverter::Utf8 );
    const QString strictlyDecoded = utf8Decoder.decode( raw );
    Q_UNUSED( strictlyDecoded );
    if ( utf8Decoder.hasError() ) {
      qWarning() << "KomportCharset:" << _path << "- not valid UTF-8, skipping";
      return false;
    }
  }

  _out.id = info.completeBaseName(); // filename without its ".charset" extension
  _out.displayName = _out.id;        // fallback - "# Name: ..." below can override
  for ( int b = 0; b < 256; ++b ) _out.forward[b] = static_cast<char16_t>(b); // default: identity

  static const QRegularExpression nameDirective(
      QStringLiteral("^#\\s*Name\\s*:\\s*(.+?)\\s*$"), QRegularExpression::CaseInsensitiveOption );

  // Codex review round-4 finding: QTextStream::readLine(maxlen) cannot
  // itself distinguish "this maxlen-long chunk is the split-off head of a
  // longer physical line" from "this physical line just happens to be
  // exactly maxlen characters long" - Qt's own scanner stops at exactly
  // maxlen either way (confirmed against Qt's readLine() implementation),
  // so the round-3 fix's "treat exactly-maxlen as split, discard" rule
  // necessarily also rejected the second, entirely legitimate case.
  //
  // Codex review round-5 finding: the round-4 fix's raw.split('\n') looked
  // like the natural way to get each physical line's real length, but
  // QByteArray::split() materializes *every* physical line up front,
  // before the MaxCustomCharsetLinesRead cap below ever gets a chance to
  // apply - a permitted (<=1 MiB) file consisting mostly of blank lines
  // could still balloon into well over a million QByteArray elements. A
  // manual indexOf('\n', ...) scan below advances one physical line at a
  // time and stops as soon as the line-count cap is hit, exactly like the
  // original streaming readLine() loop did, while still measuring each
  // line's real length up front rather than through readLine(maxlen)'s
  // chunking. (A lone '\r'-only line ending, classic-Mac style, isn't
  // recognized as a break here either way - real enough for a plain UTF-8
  // *.charset file that this format has never claimed to support, and the
  // resulting single giant "line" is still caught by the length/line-count
  // limits below rather than silently misparsed.)
  //
  // Also a round-5 finding: measuring lineBytes.size() (raw UTF-8 *byte*
  // count) against MaxCustomCharsetLineLength silently rejected a fully
  // legitimate line well under 4096 real characters if it contained any
  // multi-byte UTF-8 (e.g. a "# Name: ..." with umlauts), and separately
  // over-counted a line by one for every CRLF-terminated file (the
  // trailing '\r' left behind by splitting only on '\n'). Decoding first
  // and measuring the trimmed QString's actual character length fixes
  // both - '\r' is whitespace, trimmed() already removes it.
  int lineNo = 0;
  qsizetype searchFrom = 0;
  // Codex review round-6 finding (Low): "searchFrom <= raw.size()" let a
  // file ending in a single trailing '\n' produce one extra, phantom
  // empty "line" beyond the real content (searchFrom lands exactly on
  // raw.size() right after consuming that final newline, and <= still
  // let the loop run once more for it) - harmless in practice (an empty
  // line is skipped either way), but it could trip the
  // MaxCustomCharsetLinesRead cap one line early and print a misleading
  // "more than N lines" warning for a file at exactly the real limit.
  // "searchFrom == 0" only matters for a genuinely empty file (raw.size()
  // == 0, searchFrom stays 0 for that one intentional pass); for every
  // other file, requiring strictly "<" stops right after the last real
  // line's content was consumed, whether or not it ended in '\n' - see
  // the two comment blocks above for the "no trailing newline" and
  // "deliberate blank final line" cases, both still handled correctly.
  while ( searchFrom < raw.size() || searchFrom == 0 ) {
    if ( lineNo >= MaxCustomCharsetLinesRead ) {
      qWarning() << "KomportCharset:" << _path << "has more than" << MaxCustomCharsetLinesRead
                 << "lines - ignoring the rest";
      break;
    }
    ++lineNo;
    const qsizetype newlineAt = raw.indexOf( '\n', searchFrom );
    const QByteArray lineBytes = ( newlineAt < 0 )
        ? raw.mid( searchFrom )                        // last line, no trailing newline
        : raw.mid( searchFrom, newlineAt - searchFrom );
    searchFrom = ( newlineAt < 0 ) ? raw.size() + 1 : newlineAt + 1; // +1 past raw.size() ends the while loop after this line

    const QString line = QString::fromUtf8(lineBytes).trimmed();
    // Codex review round-6 finding (Low): QString::size() counts UTF-16
    // *code units*, not Unicode code points - a non-BMP ("astral") code
    // point (e.g. an emoji) takes two UTF-16 units (a surrogate pair) but
    // is one real character, so line.size() alone could over-count and
    // reject a line that's actually well within the real 4096-character
    // limit. line.size() is always >= the true code-point count, though,
    // so it's a cheap, always-safe *pre*-check - the exact, potentially
    // more expensive toUcs4() count only needs to run for the rare line
    // that's even a candidate for rejection in the first place (mapped
    // byte values themselves stay BMP-only per the surrogate-rejection
    // check below, so this only realistically matters for "# Name: ..."
    // display-name lines).
    if ( line.size() > MaxCustomCharsetLineLength && line.toUcs4().size() > MaxCustomCharsetLineLength ) {
      qWarning() << "KomportCharset:" << _path << "line" << lineNo
                 << "- exceeds the" << MaxCustomCharsetLineLength
                 << "character line-length limit, skipping";
      continue;
    }
    if ( line.isEmpty() ) continue;
    if ( line.startsWith(QLatin1Char('#')) ) {
      const QRegularExpressionMatch m = nameDirective.match(line);
      if ( m.hasMatch() ) _out.displayName = m.captured(1);
      continue; // any other comment line is just a comment
    }
    const int eq = line.indexOf(QLatin1Char('='));
    if ( eq <= 0 ) {
      qWarning() << "KomportCharset:" << _path << "line" << lineNo
                 << "- expected '<hex byte>=<hex code point>', skipping:" << line;
      continue;
    }
    bool byteOk = false, codeOk = false;
    const uint byteVal = line.left(eq).trimmed().toUInt( &byteOk, 16 );
    const uint codeVal = line.mid(eq + 1).trimmed().toUInt( &codeOk, 16 );
    // Codex review finding: 0xD800-0xDFFF are UTF-16 surrogate halves,
    // not valid standalone Unicode scalar values on their own - QChar
    // will happily hold one anyway (it's just a 16-bit code unit), but
    // accepting one here would silently create a table entry that can
    // never correctly round-trip through real Unicode-aware text
    // handling. Rejected the same way as any other out-of-range value.
    const bool isSurrogate = codeOk && codeVal >= 0xD800 && codeVal <= 0xDFFF;
    if ( !byteOk || !codeOk || byteVal > 0xFF || codeVal > 0xFFFF || isSurrogate ) {
      qWarning() << "KomportCharset:" << _path << "line" << lineNo
                 << "- byte/code point out of range (byte must be 00-FF, code point"
                    " 0000-FFFF excluding the D800-DFFF surrogate range - no"
                    " surrogate-pair/astral support), skipping:" << line;
      continue;
    }
    _out.forward[byteVal] = static_cast<char16_t>(codeVal);
  }

  // Codex review round-5 finding (High - CNC-transfer-relevant, see
  // reloadCustomCharsets()'s own header comment on why this file being
  // wrong matters beyond just display): nothing previously warned when
  // two *different* bytes mapped to the same displayed Unicode character
  // (e.g. a file with both "01=0041" and "41=0041" explicitly, or one
  // explicit override that happens to collide with the identity default
  // of some other byte) - the "first insert wins" reverse-table tie-break
  // (same idiom as cp437Reverse(), see its own comment) then silently
  // decided which of the two bytes actually gets sent when the user
  // types/pastes/macros that displayed character, with nothing telling
  // the file's author their table was ambiguous. Warn once per collision
  // so a genuinely ambiguous custom table is at least visible, rather
  // than only a live TX difference from what was displayed.
  for ( int b = 0; b < 256; ++b ) {
    const char16_t u = _out.forward[static_cast<std::size_t>(b)];
    const auto existing = _out.reverse.constFind(u);
    if ( existing == _out.reverse.constEnd() ) {
      _out.reverse.insert( u, static_cast<unsigned char>(b) );
    } else {
      // Codex review round-6 finding (Low): QDebug's Qt::hex/Qt::dec
      // stream state is easy to lose track of across a long chained
      // qWarning() call - an earlier version of this message switched
      // back to Qt::dec before the final byte value, so that one number
      // printed in decimal while every other one in the same message
      // printed in hex, a misleading mix. Building each hex string
      // explicitly via QString::number(...,16) instead of relying on
      // QDebug's stream state sidesteps the whole class of bug.
      const QString existingHex = QString::number( existing.value(), 16 );
      const QString bHex = QString::number( b, 16 );
      const QString codePointHex = QString::number( u, 16 );
      qWarning().noquote() << QStringLiteral("KomportCharset: %1 - both byte 0x%2 and byte 0x%3 map to the same"
          " character U+%4 - ambiguous table, typing/pasting/sending that character will transmit byte 0x%2"
          " (the numerically lower one), not byte 0x%3")
          .arg( _path, existingHex, bHex, codePointHex );
    }
  }
  return true;
}

/** Milestone 7 addendum (user request: "die Benutzung auch sauber
 *  dokumentieren, so dass das für jeden sofort verständlich ist"):
 *  writes a short, bilingual (EN/DE) usage README directly into
 *  customCharsetsDirectory() the first time it's created, so anyone who
 *  finds their way into the folder (e.g. via the Settings dialog's
 *  "Custom Charsets Folder..." button) sees the file format explained
 *  right there, without needing to already know about TODO.md or the
 *  source code. Only writes it if no file of that name exists *right
 *  now* - editing it in place is respected (won't be overwritten while
 *  it's still there). Codex review finding, comment corrected: an
 *  earlier version of this comment claimed deleting the file would keep
 *  it gone permanently - not true, since this check re-runs on every
 *  call (every app start, every Settings-dialog open) and can only see
 *  "does it exist right now", not "did a user delete it on purpose" - a
 *  deleted README.txt *will* reappear on the next call, the same way
 *  customCharsetsDirectory()'s own mkpath() call below recreates the
 *  whole directory if that got deleted too. Consistent, if not what
 *  that first version promised. */
void writeReadmeIfMissing(const QString &_dirPath)
{
  const QString readmePath = _dirPath + QStringLiteral("/README.txt");
  if ( QFile::exists(readmePath) ) return;

  QFile f(readmePath);
  if ( !f.open(QIODevice::WriteOnly | QIODevice::Text) ) {
    qWarning() << "KomportCharset: could not write" << readmePath << "(" << f.errorString() << ")";
    return;
  }
  QTextStream out(&f);
  out <<
    "Komport-Qt6 - Custom Character Sets / Benutzerdefinierte Zeichensaetze\n"
    "========================================================================\n"
    "\n"
    "EN: Drop a *.charset file into this folder to add a new selectable\n"
    "    character set to Komport-Qt6's Settings dialog (Terminal tab) -\n"
    "    no code change or rebuild needed. Re-open the Settings dialog\n"
    "    (or restart the app) to pick up a new or changed file.\n"
    "\n"
    "DE: Eine *.charset-Datei in diesen Ordner legen, um einen neuen,\n"
    "    auswaehlbaren Zeichensatz im Settings-Dialog (Terminal-Tab) von\n"
    "    Komport-Qt6 hinzuzufuegen - ohne Code-Aenderung oder Neubau. Den\n"
    "    Settings-Dialog neu oeffnen (oder die App neu starten), um eine\n"
    "    neue oder geaenderte Datei zu uebernehmen.\n"
    "\n"
    "File format / Dateiformat:\n"
    "---------------------------\n"
    "# Name: <display name shown in the dropdown / im Dropdown angezeigter Name>\n"
    "<hex byte 00-FF>=<hex Unicode code point 0000-FFFF>\n"
    "...\n"
    "\n"
    "EN:\n"
    "- The file must be plain UTF-8 text (plain ASCII, used in every example\n"
    "  here, is valid UTF-8 too) - not UTF-16/UTF-32. A file containing a\n"
    "  NUL byte (which a real UTF-8/ASCII text file never does) is rejected\n"
    "  outright as not being one.\n"
    "- The filename (without \".charset\") becomes the internal id and is\n"
    "  what gets stored in a saved profile.\n"
    "- Any byte you don't list keeps its default identity mapping (byte\n"
    "  value == Unicode code point) - this is what keeps control codes\n"
    "  (0x00-0x1F, 0x7F) safe without you needing to think about\n"
    "  terminal/VT100 semantics at all.\n"
    "- Lines starting with \"#\" are comments; a malformed individual line\n"
    "  is skipped (and logged), not the whole file.\n"
    "- The reverse direction (typing/pasting -> what gets sent over the\n"
    "  wire) is derived automatically from the same table - no separate\n"
    "  section needed.\n"
    "\n"
    "DE:\n"
    "- Die Datei muss reines UTF-8 sein (reines ASCII, wie in jedem Beispiel\n"
    "  hier verwendet, ist ebenfalls gueltiges UTF-8) - nicht UTF-16/UTF-32.\n"
    "  Eine Datei mit einem NUL-Byte (das in einer echten UTF-8/ASCII-\n"
    "  Textdatei nie vorkommt) wird deshalb komplett abgelehnt.\n"
    "- Der Dateiname (ohne \".charset\") wird zur internen ID und ist das,\n"
    "  was in einem gespeicherten Profil abgelegt wird.\n"
    "- Jedes nicht aufgefuehrte Byte bleibt bei der Standard-Identitaets-\n"
    "  Zuordnung (Byte-Wert == Unicode-Codepoint) - das haelt Steuercodes\n"
    "  (0x00-0x1F, 0x7F) sicher, ohne dass man ueberhaupt an Terminal-/\n"
    "  VT100-Semantik denken muss.\n"
    "- Zeilen, die mit \"#\" beginnen, sind Kommentare; eine fehlerhafte\n"
    "  einzelne Zeile wird uebersprungen (und geloggt), nicht die ganze Datei.\n"
    "- Die Rueckrichtung (Tippen/Einfuegen -> was ueber die Leitung\n"
    "  gesendet wird) wird automatisch aus derselben Tabelle abgeleitet -\n"
    "  kein separater Abschnitt noetig.\n"
    "\n"
    "Example / Beispiel (would live in amiga.charset):\n"
    "----------------------------------------------------\n"
    "# Name: Amiga (example)\n"
    "DB=2588\n"
    "41=03B1\n"
    "\n"
    "Full reference / Vollstaendige Referenz: TODO.md section/Abschnitt 1\n"
    "in the Komport-Qt6 source repository, or the code comment on\n"
    "KomportCharset::reloadCustomCharsets() (komport/komportcharset.h).\n";
}

} // namespace

QChar KomportCharset::toDisplay(Id _charset, unsigned char _rawByte, const QString &_customId)
{
  if ( _charset == Custom ) {
    const auto it = customRegistry().constFind( _customId );
    if ( it != customRegistry().constEnd() ) return QChar( it->forward[_rawByte] );
    // unknown/no-longer-loaded custom id (e.g. the file was deleted after
    // a profile was saved referencing it) - Standard's identity behavior
    // rather than crashing or guessing at a replacement.
    return QChar( static_cast<uchar>(_rawByte) );
  }
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
    case Custom: // unreachable - handled above
    default:
      return QChar( static_cast<uchar>(_rawByte) );
  }
}

char KomportCharset::toWire(Id _charset, QChar _ch, const QString &_customId)
{
  // Codex review finding: falling back to _ch.toLatin1() unconditionally
  // for *any* charset was silently wrong in two ways. (1) For CP437, its
  // own reverse table is exhaustive of everything CP437 can actually
  // display - if a character isn't in there, CP437 genuinely cannot
  // represent it, and sending the character's raw Latin-1 byte value
  // anyway sent a real but *different* CP437 glyph (e.g. þ, U+00FE, isn't
  // representable in CP437 at all, but byte 0xFE happens to display as ■
  // under CP437 - silent corruption, not a reasonable fallback). (2) For
  // any charset, a character outside Latin-1 entirely makes
  // QChar::toLatin1() return 0 (NUL) per Qt's own documented behavior -
  // indistinguishable from a deliberately-typed real NUL character, and a
  // NUL byte sent to real serial gear is far more likely to be
  // disruptive than a visibly-wrong placeholder. '?' now marks "this
  // charset cannot represent this character" explicitly instead. Custom
  // charsets get the same exhaustive-reverse-table treatment as CP437,
  // since a loaded custom table is always a full 256-entry table too.
  if ( _charset == Custom ) {
    const auto entryIt = customRegistry().constFind( _customId );
    if ( entryIt != customRegistry().constEnd() ) {
      const auto it = entryIt->reverse.constFind( _ch.unicode() );
      if ( it != entryIt->reverse.constEnd() ) return static_cast<char>( it.value() );
      // Codex review round-6 finding (High - CNC-transfer-relevant): the
      // literal byte 0x3F ('?') is only a safe "can't represent this"
      // placeholder under charsets where 0x3F actually displays as '?'
      // (true for Standard/CP437/PETSCII's identity fallback range) - a
      // CUSTOM table is free to redefine byte 0x3F to mean something else
      // entirely (e.g. an explicit "3F=2588" entry displaying it as a
      // full block), in which case sending literal 0x3F for an
      // unrepresentable character would silently transmit whatever THAT
      // table defines 0x3F as, not a generic placeholder. Look up what
      // byte this table itself uses to represent a literal '?' (U+003F)
      // instead - guaranteed to exist unless the table both redefines
      // byte 0x3F to something else AND provides no other byte for '?'
      // either (every byte's forward mapping, identity-default or
      // explicit, feeds the reverse table - see loadCustomCharsetFile() -
      // so U+003F is covered unless the file goes out of its way to avoid
      // it). Falls back to the literal byte only in that last, genuinely
      // pathological case, where there is no better answer available.
      const auto qMarkIt = entryIt->reverse.constFind( QChar(u'?').unicode() );
      return ( qMarkIt != entryIt->reverse.constEnd() ) ? static_cast<char>( qMarkIt.value() ) : '?';
    }
    // unknown/no-longer-loaded custom id - Standard's own fallback policy.
    return ( _ch.unicode() <= 0xFF ) ? _ch.toLatin1() : '?';
  }
  switch ( _charset ) {
    case CP437:
      {
        const auto &rev = cp437Reverse();
        auto it = rev.constFind( _ch.unicode() );
        return ( it != rev.constEnd() ) ? static_cast<char>( it.value() ) : '?';
      }
    case PETSCII:
      {
        // Unlike CP437, petsciiForward()/petsciiReverse() deliberately
        // only tabulate the *distinctive* substitutions (see its own
        // comment) - the genuinely ASCII-identical range (digits, most
        // punctuation, '@', uppercase letters, '[' and ']') is
        // intentionally absent from the table and must still fall
        // through to the Latin-1 byte value.
        //
        // Codex review finding (round 2): the first version of this
        // fallback used "anything <= 0x7F" as its identity range, which
        // was wrong - it let a literal '\' (U+005C) fall through to byte
        // 0x5C, which PETSCII actually displays as £ (the exact
        // silent-wrong-byte class this function exists to prevent); same
        // problem for '^'/'_' (-> ↑/←), and for any lowercase letter
        // (0x61-0x7A sits inside PETSCII's unmapped graphics range, not
        // real lowercase text). Narrowed to exactly the printable
        // 0x20-0x5B plus 0x5D range.
        //
        // Codex review finding (round 3): that narrowing went too far the
        // other way - it also excluded the C0 control range (0x00-0x1F)
        // and DEL (0x7F), so pasting multi-line clipboard text under
        // PETSCII turned every '\r'/'\n' into '?' ("line1\nline2" became
        // "line1?line2"). Applying the *same* reasoning already used for
        // CP437's control range (see KomportCharset::Id::CP437's comment
        // and the kCp437 table comment: this emulation is a VT100
        // interpreter first, control bytes stay literal/identity rather
        // than getting reinterpreted through a retro charset, regardless
        // of which charset is selected) - the control range plus DEL is
        // now identity for PETSCII's fallback too, consistent with
        // CP437's already-established scope decision rather than
        // attempting authentic PETSCII control-code semantics (real C64
        // PETSCII's LF-equivalent is at a completely different byte,
        // 0x8D, not 0x0A - reproducing that faithfully was never this
        // milestone's goal, see TODO.md).
        const auto &rev = petsciiReverse();
        auto it = rev.constFind( _ch.unicode() );
        if ( it != rev.constEnd() ) return static_cast<char>( it.value() );
        const ushort u = _ch.unicode();
        const bool controlOrAsciiIdentityRange = ( u <= 0x5B ) || u == 0x5D || u == 0x7F;
        return controlOrAsciiIdentityRange ? _ch.toLatin1() : '?';
      }
    case Standard:
    case Custom: // unreachable - handled above
    default:
      // Standard's whole definition is "byte value == code point", so
      // toLatin1() is correct here for the full Latin-1 range, not just a
      // fallback - preserves the exact pre-Milestone-7 behavior. Only
      // guard against the NUL-for-non-Latin1-input ambiguity above.
      return ( _ch.unicode() <= 0xFF ) ? _ch.toLatin1() : '?';
  }
}

// Codex review finding (round 2): names()/fromIndex()/toIndex() used to be
// three independent implementations that merely had to *agree* with
// displayEntries() by convention, with nothing enforcing that - a real
// maintenance hazard even though nothing had actually drifted yet. All
// three now derive directly from displayEntries(), the one place that
// pairs an Id with its display name, so they cannot drift from it.
// Custom is deliberately excluded - see customCharsetEntries() instead.
QVector<QPair<KomportCharset::Id, QString>> KomportCharset::displayEntries()
{
  return {
    { Standard, QStringLiteral("Standard") },
    { CP437,    QStringLiteral("IBM CP437") },
    { PETSCII,  QStringLiteral("PETSCII") },
  };
}

QStringList KomportCharset::names()
{
  QStringList result;
  for ( const auto &entry : displayEntries() ) result << entry.second;
  return result;
}

KomportCharset::Id KomportCharset::fromIndex(int _index)
{
  const auto entries = displayEntries();
  if ( _index >= 0 && _index < entries.size() ) return entries.at(_index).first;
  return Standard;
}

int KomportCharset::toIndex(Id _charset)
{
  const auto entries = displayEntries();
  for ( int i = 0; i < entries.size(); ++i ) {
    if ( entries.at(i).first == _charset ) return i;
  }
  return 0; // Standard's own index - see displayEntries()
}

QString KomportCharset::settingsKey(Id _charset)
{
  switch ( _charset ) {
    case CP437:   return QStringLiteral("CP437");
    case PETSCII: return QStringLiteral("PETSCII");
    case Custom:
      qWarning() << "KomportCharset::settingsKey() called with Custom - there is no single"
                    " fixed key for it, use the loaded custom charset's own id"
                    " (customCharsetEntries()) directly as the settings key instead";
      return QStringLiteral("Standard");
    case Standard:
    default:      return QStringLiteral("Standard");
  }
}

KomportCharset::Selection KomportCharset::resolveSettingsKey(const QString &_key)
{
  if ( _key == QStringLiteral("CP437") ) return { CP437, QString() };
  if ( _key == QStringLiteral("PETSCII") ) return { PETSCII, QString() };
  if ( _key == QStringLiteral("Standard") ) return { Standard, QString() };
  if ( customRegistry().contains(_key) ) return { Custom, _key };
  // Unrecognized (an old profile referencing a custom charset file that's
  // since been deleted/renamed, a hand-edited config, ...) - same
  // graceful-degradation policy as the rest of this codebase's profile
  // loading: fall back to a safe, known-good default rather than
  // crashing or guessing.
  return { Standard, QString() };
}

QString KomportCharset::customCharsetsDirectory()
{
  // Derived directly from the real QSettings config file's own location
  // (~/.config/Komport-Qt6/Komport-Qt6.conf on Linux -> .../Komport-Qt6/
  // charsets/) rather than QStandardPaths::AppConfigLocation - that
  // resolves to "~/.config/<organizationName>/<applicationName>" (BOTH,
  // not just applicationName - both happen to be "Komport-Qt6", see
  // main.cpp), landing in a *nested* .../Komport-Qt6/Komport-Qt6/
  // directory rather than next to the actual .conf file. Using
  // QSettings()'s own fileName() instead guarantees this always sits
  // right beside whatever config file is *actually* in use - including
  // under the test suite's own separate organization/application names
  // (see tst_charset.cpp's initTestCase()), which keeps every test run
  // completely isolated from the real user's own
  // ~/.config/Komport-Qt6/charsets/. */
  const QString configDir = QFileInfo( QSettings().fileName() ).absolutePath();
  const QString dirPath = configDir + QStringLiteral("/charsets");
  // Codex review finding: mkpath()'s result was ignored - a read-only
  // filesystem, or a *file* already sitting at this path instead of a
  // directory, would silently hand back a path that doesn't actually
  // work for anything using it afterwards (reloadCustomCharsets()'s own
  // QDir::entryList() on a nonexistent/non-directory path just returns
  // an empty list, no crash, but with no diagnostic explaining why
  // custom charsets never show up). Logged, not otherwise handled -
  // there's no better fallback location to offer instead.
  if ( !QDir().mkpath(dirPath) ) {
    qWarning() << "KomportCharset: could not create" << dirPath
               << "- custom *.charset files will not be found until this is fixed"
                  " (read-only filesystem? a file already exists at this path?)";
  }
  writeReadmeIfMissing( dirPath );
  return dirPath;
}

namespace {

/** true if _name (case-insensitively) matches a built-in charset's display
 *  name or any display name already sitting in _registry - shared by the
 *  original-name check and the disambiguated-name re-check below (Codex
 *  review round 3 finding: re-check needed, see its call sites). */
bool displayNameCollides( const QString &_name, const QMap<QString, CustomEntry> &_registry )
{
  for ( const auto &builtin : KomportCharset::displayEntries() ) {
    if ( builtin.second.compare(_name, Qt::CaseInsensitive) == 0 ) return true;
  }
  for ( auto it = _registry.constBegin(); it != _registry.constEnd(); ++it ) {
    if ( it->displayName.compare(_name, Qt::CaseInsensitive) == 0 ) return true;
  }
  return false;
}

} // namespace

void KomportCharset::reloadCustomCharsets()
{
  auto &registry = customRegistry();
  registry.clear();

  const QString dirPath = customCharsetsDirectory();
  QDir dir( dirPath );
  // Codex review round-3 finding: a cap that only counted *successfully
  // registered* entries let a directory full of oversized/corrupt/
  // colliding files be opened and parsed in full before the cap ever took
  // effect, defeating the point of bounding reload cost.
  //
  // Codex review round-4 finding: even after fixing that, QDir::entryList()
  // itself still materializes and sorts *every* matching filename up
  // front, regardless of the cap - a directory with a huge number of files
  // paid that full listing/sorting cost before any per-file cap could
  // apply. QDirIterator (unsorted, lazy - one entry at a time, no sort) plus
  // a std::set capped at MaxCustomCharsetCount entries ("keep the smallest
  // N filenames seen so far, evicting the current largest kept entry when
  // a smaller one arrives") gets the exact same deterministic result
  // QDir::Name would have (the alphabetically-first MaxCustomCharsetCount
  // filenames) without ever holding or sorting more than
  // MaxCustomCharsetCount filenames in memory at once - this also means
  // the loop below can never examine more than MaxCustomCharsetCount files
  // in the first place, closing the round-3 finding at the same time.
  std::set<QString> files;
  int totalFilesSeen = 0;
  {
    QDirIterator dirIt( dirPath, QStringList{ QStringLiteral("*.charset") }, QDir::Files );
    while ( dirIt.hasNext() ) {
      dirIt.next();
      ++totalFilesSeen;
      const QString fileName = dirIt.fileName();
      if ( static_cast<int>(files.size()) < MaxCustomCharsetCount ) {
        files.insert( fileName );
      } else if ( fileName < *files.rbegin() ) {
        files.erase( std::prev(files.end()) );
        files.insert( fileName );
      }
    }
  }
  if ( totalFilesSeen > MaxCustomCharsetCount ) {
    qWarning() << "KomportCharset:" << dirPath << "has" << totalFilesSeen << "*.charset files, more than the"
               << MaxCustomCharsetCount << "file limit - keeping only the alphabetically first"
               << MaxCustomCharsetCount << "and ignoring the rest";
  }

  for ( const QString &fileName : files ) { // std::set<QString> iterates in ascending (alphabetical) order
    CustomEntry entry;
    if ( !loadCustomCharsetFile( dir.filePath(fileName), entry ) ) continue;
    if ( entry.id.compare( QStringLiteral("Standard"), Qt::CaseInsensitive ) == 0
      || entry.id.compare( QStringLiteral("CP437"), Qt::CaseInsensitive ) == 0
      || entry.id.compare( QStringLiteral("PETSCII"), Qt::CaseInsensitive ) == 0 ) {
      qWarning() << "KomportCharset:" << fileName << "- id" << entry.id
                 << "collides with a built-in charset name, skipping this file"
                    " (rename it to something else)";
      continue;
    }
    // Codex review finding: the id-collision check above only protects
    // the *persisted key* (unique by construction - QDir::entryList()
    // never lists two different files under the exact same name) - it
    // said nothing about the *displayed* name, which comes from an
    // unchecked "# Name: ..." line and could just as easily claim to be
    // "Standard" or duplicate another custom entry's name, making the
    // dropdown show two indistinguishable rows even though the
    // underlying ids (and therefore the actual behavior) differ.
    // Disambiguate rather than reject outright - the file is still
    // perfectly loadable and usable, this is purely a display concern.
    //
    // Codex review round-3 finding: the first version of this check only
    // validated the *original* declared name, then applied the " (<id>)"
    // suffix unconditionally without re-checking whether that generated
    // string itself now collided with an already-registered entry (e.g.
    // one file's raw, uncollided "# Name: Standard (b)" happening to
    // equal what a *different* file "b.charset" generates after its own
    // "Standard" collision gets suffixed with its own id "b"). Loops,
    // re-validating the candidate after every disambiguation attempt,
    // rather than trusting a single suffix pass to always be enough.
    if ( displayNameCollides(entry.displayName, registry) ) {
      const QString original = entry.displayName;
      entry.displayName = QStringLiteral("%1 (%2)").arg(original, entry.id);
      int disambiguationCounter = 2;
      while ( displayNameCollides(entry.displayName, registry) ) {
        entry.displayName = QStringLiteral("%1 (%2) [%3]").arg(original, entry.id).arg(disambiguationCounter);
        ++disambiguationCounter;
      }
    }
    registry.insert( entry.id, entry );
  }
}

QVector<QPair<QString, QString>> KomportCharset::customCharsetEntries()
{
  QVector<QPair<QString, QString>> result;
  for ( const CustomEntry &entry : customRegistry() ) result.append( { entry.id, entry.displayName } );
  return result;
}
