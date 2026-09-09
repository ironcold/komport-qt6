/***************************************************************************
                          komportcharset.h  -  Komport Serial Port Communicator
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

#ifndef KOMPORTCHARSET_H
#define KOMPORTCHARSET_H

#include <QChar>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

/** Milestone 7: byte-level character-set translation between the raw
 *  serial stream and the terminal emulation, for retro/industrial gear
 *  that doesn't speak plain ASCII/Latin-1. Sits exactly where CLAUDE.md's
 *  data-flow diagram puts it: after the raw RX/TX diagnostics (hex
 *  monitor/session logger, which must keep seeing the *untranslated*
 *  wire bytes) and before/after the terminal emulation's own escape-
 *  sequence interpretation. In practice that means a single call site on
 *  each direction:
 *   - RX: KomportEmulation::slotReceivedChar()'s `default:` branch (the
 *     one point where a byte has already been established as *not* one
 *     of the six control codes the emulation itself still needs (BEL/BS/
 *     HT/LF/CR) and not part of an ESC/CSI sequence) - so control-code
 *     and escape-sequence handling is completely unaffected by the
 *     selected charset, only what actually gets drawn into a cell.
 *   - TX: KomportEmulation::slotKeyPressed()'s default case and
 *     slotSimKeyPressed() (typed/pasted characters, not the emulation's
 *     own generated cursor-key/reply escape sequences, which must stay
 *     literal ASCII/VT100 regardless of charset).
 *
 *  Milestone 7 addendum (2026-09-09, user request: "einen geeigneten
 *  Mechanismus vorsehen, so dass neue Tabellen einfach in ein
 *  entsprechendes Verzeichnis abgelegt werden und dann im Programm mit
 *  auswählbar sind"): beyond the two built-in extra charsets (CP437/
 *  PETSCII, hardcoded C++ tables below), users can drop a plain-text
 *  *.charset file into customCharsetsDirectory() to add another one
 *  without touching or rebuilding any code - see reloadCustomCharsets()
 *  for the file format and loadCustomCharsetFile() for the parser. */
class KomportCharset
{
public:
  enum Id {
    Standard = 0, /**< no translation - byte value == Unicode code point,
                    *  exactly today's pre-Milestone-7 behavior (Latin-1) */
    CP437,        /**< IBM PC / MS-DOS code page 437 ("OEM-US") - the
                    *  0x80-0xFF extended range only (accented Latin
                    *  letters, box-drawing/block glyphs). 0x00-0x7F is
                    *  deliberately identity (same as Standard), NOT
                    *  CP437's well-known low-range "control picture"
                    *  glyphs (☺♥♦♣♠ etc.) - an earlier version mapped
                    *  those too, but a Codex adversarial review found
                    *  that conflicts with several real VT100 control
                    *  codes this emulation doesn't special-case (e.g.
                    *  VT/FF at 0x0B/0x0C, which many real hosts use like
                    *  LF, drew a glyph and merely advanced the cursor
                    *  instead of doing a line feed). See .cpp for the
                    *  full rationale; TODO.md tracks restoring them
                    *  (behind an explicit "raw graphics mode" toggle or
                    *  per-code special-casing) as a possible future
                    *  refinement, not a currently planned one. */
    PETSCII,      /**< Commodore PETSCII, unshifted/"graphics" mode -
                    *  deliberately partial, see .cpp: the ASCII-compatible
                    *  range (letters/digits/most punctuation) plus the
                    *  three well-known substitutions (£/↑/←) are mapped;
                    *  the 0x60-0x7F/0xA0-0xFF CBM-specific block-graphics
                    *  range is NOT mapped in this pass - even the
                    *  authoritative df.lth.se PETSCII-Unicode reference
                    *  table (GPLv2, submitted to the Unicode consortium)
                    *  lists much of that range as UNDEFINED or requiring
                    *  Private-Use-Area code points, so this is a
                    *  documented scope boundary, not an oversight. */
    Custom,       /**< a user-supplied charset loaded from a *.charset
                    *  file in customCharsetsDirectory() - which specific
                    *  one is named separately (see Selection below and
                    *  the _customId parameters throughout this class),
                    *  since there can be any number of them, unlike the
                    *  three fixed built-ins above. */
  };

  /** the result of resolving a persisted settingsKey()-style string back
   *  into something toDisplay()/toWire()/KomportEmulation::setCharset()
   *  can act on. customId is only meaningful (and only ever non-empty)
   *  when id == Custom. */
  struct Selection {
    Id id = Standard;
    QString customId;
  };

  /** RX direction: translate one raw byte received over the wire into the
   *  QChar that should actually be drawn into the terminal grid.
   *  _customId is used only when _charset == Custom - see
   *  customCharsetEntries() for the available ids; an unknown/no-longer-
   *  loaded _customId falls back to Standard's identity behavior rather
   *  than crashing or guessing. */
  static QChar toDisplay(Id _charset, unsigned char _rawByte, const QString &_customId = QString());

  /** TX direction: translate one typed/pasted Unicode character into the
   *  raw byte that should actually be sent over the wire for the given
   *  charset. Standard falls back to _ch.toLatin1() for the full Latin-1
   *  range (its whole definition is "byte value == code point"). CP437
   *  relies solely on its own exhaustive reverse table - no separate
   *  fallback, since anything not in there genuinely isn't representable
   *  in CP437. PETSCII falls back to _ch.toLatin1() only for its narrow,
   *  genuinely ASCII-identical range plus the C0 control range/DEL (see
   *  the .cpp for the exact bytes - NOT "everything <= 0x7F", a round-2
   *  Codex review finding: several bytes in that range mean something
   *  else entirely under PETSCII). Custom charsets (_customId, see
   *  toDisplay() above) get the same exhaustive-reverse-table treatment
   *  as CP437, since a loaded custom table is always a full 256-entry
   *  table too (see loadCustomCharsetFile()). Any character a given
   *  charset cannot represent returns '?' rather than a silently wrong
   *  byte or - QChar::toLatin1()'s own behavior for non-Latin-1 input -
   *  a silent NUL. */
  static char toWire(Id _charset, QChar _ch, const QString &_customId = QString());

  /** display names for the Settings dialog dropdown, in Id order - the
   *  three fixed built-ins only, NOT including Custom (there can be any
   *  number of those - see customCharsetEntries() for the separate,
   *  dynamically-sized list the dropdown also populates itself from) */
  static QStringList names();
  /** _index into names() -> Id, clamped to Standard if out of range */
  static Id fromIndex(int _index);
  /** Id -> index into names() */
  static int toIndex(Id _charset);
  /** (Id, display name) pairs, in the same order as names() - the single
   *  source of truth the Settings dialog's dropdown actually populates
   *  itself from (each combo item carries its settingsKey() string
   *  directly as Qt::UserRole data - see settingsKey()/resolveSettingsKey()
   *  below; a Codex review finding on an earlier version that used the
   *  bare Id as UserRole data instead found that fragile against a future
   *  reordering, and it also can't represent Custom's many possible
   *  distinct ids the way a shared string keyspace can). */
  static QVector<QPair<Id, QString>> displayEntries();

  /** persisted profile value (Profiles/<name>/Charset) for one of the
   *  three built-ins - "Standard"/"CP437"/"PETSCII". Do not call with
   *  _charset == Custom (there is no single fixed key for it - a loaded
   *  custom charset's own id, from customCharsetEntries(), already *is*
   *  its settings key, use that string directly instead). */
  static QString settingsKey(Id _charset);
  /** resolve any settingsKey()-style string - one of the three built-in
   *  names above, or a loaded custom charset's id - back into a
   *  Selection. Unrecognized input (an old profile referencing a custom
   *  charset file that's since been deleted or renamed, a hand-edited
   *  config, ...) falls back to {Standard, QString()} rather than
   *  crashing or guessing - the same graceful-degradation policy already
   *  used throughout this codebase's profile loading (see
   *  KomportApp::loadProfile()). */
  static Selection resolveSettingsKey(const QString &_key);

  /** Directory scanned by reloadCustomCharsets() for *.charset files -
   *  derived from the real QSettings config file's own location
   *  (typically ~/.config/Komport-Qt6/charsets/ on Linux, right next to
   *  Komport-Qt6.conf - see the .cpp for why this isn't simply
   *  QStandardPaths::AppConfigLocation). Created if it doesn't exist yet,
   *  with a short bilingual (EN/DE) README.txt explaining the file
   *  format written into it at the same time, so there's always
   *  somewhere obvious to drop a file into - and once there, an
   *  immediate explanation of what to do. Exposed so the UI can tell the
   *  user exactly where that is (and open it directly) instead of them
   *  having to know/guess. */
  static QString customCharsetsDirectory();

  /** (re-)scan customCharsetsDirectory() for *.charset files and rebuild
   *  the in-memory registry customCharsetEntries()/toDisplay()/toWire()
   *  read from. Called once at KomportApp startup and again every time
   *  the Settings dialog opens, so a file dropped in while the app is
   *  already running becomes selectable without a restart.
   *
   *  File format (see loadCustomCharsetFile() for the actual parser):
   *  plain text, one override per line as "<hex byte 00-FF>=<hex Unicode
   *  code point>" (e.g. "DB=2588"). Any byte NOT listed keeps its default
   *  identity mapping (byte value == code point) - this is what makes
   *  control codes (0x00-0x1F, 0x7F) safe by default without the file
   *  author needing to think about VT100 semantics at all, the same
   *  reasoning already applied to CP437/PETSCII's own control ranges
   *  above. Lines starting with "#" are comments; a "# Name: <text>"
   *  comment line sets the dropdown display name (defaults to the
   *  filename without its extension if absent). A file whose id (its
   *  filename stem) collides with a built-in name ("Standard"/"CP437"/
   *  "PETSCII", case-insensitive) is skipped with a logged warning, not
   *  silently shadowed or silently dropped without explanation. */
  static void reloadCustomCharsets();

  /** (id, display name) pairs for every currently loaded custom charset,
   *  in filename order - refreshed by reloadCustomCharsets(). id is both
   *  this entry's settingsKey()-equivalent string and the _customId
   *  toDisplay()/toWire() expect. */
  static QVector<QPair<QString, QString>> customCharsetEntries();
};

#endif // KOMPORTCHARSET_H
