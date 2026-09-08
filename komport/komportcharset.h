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
 *  All functions are static/stateless (a small lookup table, no instance
 *  state) - KomportEmulation just holds which Id is currently selected. */
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
  };

  /** RX direction: translate one raw byte received over the wire into the
   *  QChar that should actually be drawn into the terminal grid. */
  static QChar toDisplay(Id _charset, unsigned char _rawByte);

  /** TX direction: translate one typed/pasted Unicode character into the
   *  raw byte that should actually be sent over the wire for the given
   *  charset. Standard falls back to _ch.toLatin1() for the full Latin-1
   *  range (its whole definition is "byte value == code point"). CP437
   *  relies solely on its own exhaustive reverse table - no separate
   *  fallback, since anything not in there genuinely isn't representable
   *  in CP437. PETSCII falls back to _ch.toLatin1() only for its narrow,
   *  genuinely ASCII-identical range (see the .cpp for the exact bytes -
   *  NOT "everything <= 0x7F", a round-2 Codex review finding: several
   *  bytes in that range mean something else entirely under PETSCII).
   *  Any character a given charset cannot represent returns '?' rather
   *  than a silently wrong byte or - QChar::toLatin1()'s own behavior for
   *  non-Latin-1 input - a silent NUL. */
  static char toWire(Id _charset, QChar _ch);

  /** display names for the Settings dialog dropdown, in Id order */
  static QStringList names();
  /** _index into names() -> Id, clamped to Standard if out of range */
  static Id fromIndex(int _index);
  /** Id -> index into names() */
  static int toIndex(Id _charset);
  /** (Id, display name) pairs, in the same order as names() - the single
   *  source of truth the Settings dialog's dropdown actually populates
   *  itself from (each combo item carries its Id directly as Qt::UserRole
   *  data). Codex review finding on names()/fromIndex()/toIndex() above:
   *  a caller that populated a combo box from names() alone and read the
   *  selection back via fromIndex(currentIndex()) was implicitly relying
   *  on row position matching Id's numeric value - correct as long as
   *  both stay in the same order, but nothing enforced that. This pairs
   *  them at the source instead, so a combo box built from it can be read
   *  back via currentData() regardless of row order. */
  static QVector<QPair<Id, QString>> displayEntries();

  /** persisted profile value (Profiles/<name>/Charset) <-> Id - separate
   *  from the UI's names()/fromIndex()/toIndex() so a future change to
   *  the dropdown's wording/order can't silently break saved profiles */
  static QString settingsKey(Id _charset);
  static Id fromSettingsKey(const QString &_key);
};

#endif // KOMPORTCHARSET_H
