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
#include <QString>
#include <QStringList>

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
    CP437,        /**< IBM PC / MS-DOS code page 437 ("OEM-US") - full
                    *  0x00-0xFF table, including the well-known low-range
                    *  "control picture" glyphs (☺♥♦♣♠ etc.) at the byte
                    *  values not already claimed by BEL/BS/HT/LF/CR/ESC */
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
   *  charset. Falls back to _ch.toLatin1() (today's exact behavior) for
   *  any character not part of that charset's own distinctive mapping -
   *  covers plain ASCII input (the vast majority of real typing)
   *  identically under every charset. */
  static char toWire(Id _charset, QChar _ch);

  /** display names for the Settings dialog dropdown, in Id order */
  static QStringList names();
  /** _index into names() -> Id, clamped to Standard if out of range */
  static Id fromIndex(int _index);
  /** Id -> index into names() */
  static int toIndex(Id _charset);

  /** persisted profile value (Profiles/<name>/Charset) <-> Id - separate
   *  from the UI's names()/fromIndex()/toIndex() so a future change to
   *  the dropdown's wording/order can't silently break saved profiles */
  static QString settingsKey(Id _charset);
  static Id fromSettingsKey(const QString &_key);
};

#endif // KOMPORTCHARSET_H
