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
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
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
 *  reload and the next one). */
QVector<CustomEntry> &customRegistry()
{
  static QVector<CustomEntry> registry;
  return registry;
}

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
  QFile file(_path);
  if ( !file.open(QIODevice::ReadOnly | QIODevice::Text) ) {
    qWarning() << "KomportCharset: could not open" << _path << "(" << file.errorString() << ")";
    return false;
  }

  const QFileInfo info(_path);
  _out.id = info.completeBaseName(); // filename without its ".charset" extension
  _out.displayName = _out.id;        // fallback - "# Name: ..." below can override
  for ( int b = 0; b < 256; ++b ) _out.forward[b] = static_cast<char16_t>(b); // default: identity

  static const QRegularExpression nameDirective(
      QStringLiteral("^#\\s*Name\\s*:\\s*(.+?)\\s*$"), QRegularExpression::CaseInsensitiveOption );

  QTextStream in(&file);
  int lineNo = 0;
  while ( !in.atEnd() ) {
    const QString rawLine = in.readLine();
    ++lineNo;
    const QString line = rawLine.trimmed();
    if ( line.isEmpty() ) continue;
    if ( line.startsWith(QLatin1Char('#')) ) {
      const QRegularExpressionMatch m = nameDirective.match(line);
      if ( m.hasMatch() ) _out.displayName = m.captured(1);
      continue; // any other comment line is just a comment
    }
    const int eq = line.indexOf(QLatin1Char('='));
    if ( eq <= 0 ) {
      qWarning() << "KomportCharset:" << _path << "line" << lineNo
                 << "- expected '<hex byte>=<hex code point>', skipping:" << rawLine;
      continue;
    }
    bool byteOk = false, codeOk = false;
    const uint byteVal = line.left(eq).trimmed().toUInt( &byteOk, 16 );
    const uint codeVal = line.mid(eq + 1).trimmed().toUInt( &codeOk, 16 );
    if ( !byteOk || !codeOk || byteVal > 0xFF || codeVal > 0xFFFF ) {
      qWarning() << "KomportCharset:" << _path << "line" << lineNo
                 << "- byte/code point out of range (byte must be 00-FF, code point"
                    " 0000-FFFF - no surrogate-pair/astral support), skipping:" << rawLine;
      continue;
    }
    _out.forward[byteVal] = static_cast<char16_t>(codeVal);
  }

  for ( int b = 0; b < 256; ++b ) {
    const char16_t u = _out.forward[static_cast<std::size_t>(b)];
    if ( !_out.reverse.contains(u) ) _out.reverse.insert( u, static_cast<unsigned char>(b) );
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
 *  source code. Only writes it if no file of that name exists yet - a
 *  user who deletes it (or edits it) isn't fighting it being silently
 *  recreated on every launch. */
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
    for ( const CustomEntry &entry : customRegistry() ) {
      if ( entry.id == _customId ) return QChar( entry.forward[_rawByte] );
    }
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
    for ( const CustomEntry &entry : customRegistry() ) {
      if ( entry.id == _customId ) {
        auto it = entry.reverse.constFind( _ch.unicode() );
        return ( it != entry.reverse.constEnd() ) ? static_cast<char>( it.value() ) : '?';
      }
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
  for ( const CustomEntry &entry : customRegistry() ) {
    if ( entry.id == _key ) return { Custom, entry.id };
  }
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
  QDir().mkpath( dirPath ); // create if missing; a harmless no-op otherwise
  writeReadmeIfMissing( dirPath );
  return dirPath;
}

void KomportCharset::reloadCustomCharsets()
{
  auto &registry = customRegistry();
  registry.clear();

  QDir dir( customCharsetsDirectory() );
  const QStringList files = dir.entryList( QStringList{ QStringLiteral("*.charset") }, QDir::Files, QDir::Name );
  for ( const QString &fileName : files ) {
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
    registry.append( entry );
  }
}

QVector<QPair<QString, QString>> KomportCharset::customCharsetEntries()
{
  QVector<QPair<QString, QString>> result;
  for ( const CustomEntry &entry : customRegistry() ) result.append( { entry.id, entry.displayName } );
  return result;
}
