/***************************************************************************
                          komportemulation.cpp  -  Impliments vt102 emulation
                             -------------------
    begin                : Thu Feb 20 2003
    copyright            : (C) 2003 by Mike Sharkey
    email                : michael@sharkey.servebeer.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/**
Article 3073 of comp.terminals:
Path: cs.utk.edu!stc06.CTD.ORNL.GOV!fnnews.fnal.gov!uwm.edu!news.alpha.net!news.mathworks.com!europa.eng.gtefsd.com!howland.reston.ans.net!EU.net!uunet!sunic!trane.uninett.no!due.uninett.no!usenet
From: lars@mi.uib.no (Lars Johanson)
Newsgroups: comp.terminals
Subject: vt102 escape codes
Date: 24 Oct 1994 16:29:21 GMT
Organization: Haukeland Sykehus
Lines: 399
Message-ID: <38gnd1$cio@due.uninett.no>
NNTP-Posting-Host: lpjo.onh.haukeland.no
X-Newsreader: WinVN 0.92.6+


Hi.

I have seen some people asking for escape sequencies for the
vt100-terminal. I asked the DIGITAL company in Sweden, and
received this (hopefully not copyrighted). It is a vt102 terminal,
but there seems to be little difference. I've tried to mark
functions special to vt102 with:		// Not in vt100

Here you are:



Escape codes for vt102 terminal.

All numbers below are octal.<n> means numeric value,<c> means character string.
If <n> is missing it is 0 or in cursor movements 1.

Reset and set modes
  Set Modes
    Esc  [ <c> ; ... ; <c> h
    033 133   073   073   150
  Reset Modes
    Esc  [ <c> ; ... ; <c> l
    033 133   073   073   154

  Where <c> is
    '2'= Lock keyboard (set); Unlock keyboard (reset)
    '4'= Insert mode (set); Replace mode (reset)
   '12'= Echo on (set); Echo off (reset)
   '20'= Return = CR+LF (set); Return = CR (reset)
   '?1'= Cursorkeys application (set); Cursorkeys normal (reset)
   '?2'= Ansi (set); VT52 (reset)
   '?3'= 132 char/row (set); 80 char/row (reset)
   '?4'= Jump scroll (set); Smooth scroll (reset)
   '?5'= Reverse screen (set); Normal screen (reset)
   '?6'= Sets relative coordinates (set); Sets absolute coordinates (reset)
   '?7'= Auto wrap (set); Auto wrap off (reset)
   '?8'= Auto repeat on (set); Auto repeat off (reset)
  '?18'= Send FF to printer after print screen (set); No char after PS (reset)
  '?19'= Print screen prints full screen (set); PS prints scroll region (reset)
  '?25'= Cursor on (set); Cursor off (reset)

Set scrolling region (n1=upper,n2=lower)
  Esc  [ <n1> ; <n2> r
  033 133    073    162


Cursor movement (<n>=how many chars or lines), cursor stop at margin.
  Up
    Esc  [ <n> A
    033 133   101
  Down
    Esc  [ <n> B
    033 133   102
  Right
    Esc  [ <n> C
    033 133   103
  Left
    Esc  [  n  D
    033 133   104
  Cursor position  (<n1>=y,<n2>=x, from top of screen or scroll region)
       Esc  [ <n1> ; <n2> H
       033 133    073    110
    Or Esc  [ <n1> ; <n2> f
       033 133    073    146
  Index (cursor down with scroll up when at margin)
    Esc  D
    033 104
  Reverse index (cursor up with scroll down when at margin)
    Esc  M
    033 115
  Next line (CR+Index)
    Esc  E
    033 105
  Save cursor and attribute
    Esc  7
    033 067
  Restore cursor and attribute
    Esc  8
    033 070


Keybad character selection
  Application keypad mode
    Esc  =
    033 075
  Numeric keypad mode
    Esc  >
    033 076

  Keypadkeys codes generated
                  Numeric      Application                VT52 Application
    0             0 (060)      Esc O p (033 117 160)      Esc ? p (033 077 160)
    1             1 (061)      Esc O q (033 117 161)      Esc ? q (033 077 161)
    2             2 (062)      Esc O r (033 117 162)      Esc ? r (033 077 162)
    3             3 (063)      Esc O s (033 117 163)      Esc ? s (033 077 163)
    4             4 (064)      Esc O t (033 117 164)      Esc ? t (033 077 164)
    5             5 (065)      Esc O u (033 117 165)      Esc ? u (033 077 165)
    6             6 (066)      Esc O v (033 117 166)      Esc ? v (033 077 166)
    7             7 (067)      Esc O w (033 117 167)      Esc ? w (033 077 167)
    8             8 (070)      Esc O x (033 117 170)      Esc ? x (033 077 170)
    9             9 (071)      Esc O y (033 117 171)      Esc ? y (033 077 171)
    - (minus)     - (055)      Esc O m (033 117 155)      Esc ? m (033 077 155)
    , (comma)     , (054)      Esc O l (033 117 154)      Esc ? l (033 077 154)
    . (period)    . (056)      Esc O n (033 117 156)      Esc ? n (033 077 156)
    Enter         CR (015)*    Esc O M (033 117 115)      Esc ? M (033 077 115)
    PF1           Esc O P      Esc O P (033 117 120)      Esc P (033 120)
    PF2           Esc O Q      Esc O Q (033 117 121)      Esc Q (033 121)
    PF3           Esc O R      Esc O R (033 117 122)      Esc R (033 122)
    PF4           Esc O S      Esc O S (033 117 123)      Esc S (033 123)
  * Or CR+LF (015 012)

  Cursorkeys codes generated (changed by set and reset modes '?1')
          normal         application
    Up    Esc  [   A     Esc  O   A
          033 133 101    033 117 101
    Down  Esc  [   B     Esc  O   B
          033 133 102    033 117 102
    Right Esc  [   C     Esc  O   C
          033 133 103    033 117 103
    Left  Esc  [   D     Esc  O   D
          033 133 104    033 117 104


Select character set
  UK as G0
    Esc  (   A
    033 050 101
  US as G0
    Esc  (   B
    033 050 102
  Special characters and line drawing character set as G0
    Esc  (   0
    033 050 060
  Alternate ROM as G0					// Not in vt100
    Esc  (   1
    033 050 061
  Alternate ROM special characters character set as G0	// Not in vt100
    Esc  (   2
    033 050 062

  UK as G1
    Esc  )   A
    033 051 101
  US as G1
    Esc  )   B
    033 051 102
  Special characters and line drawing character set as G1
    Esc  )   0
    033 051 060
  Alternate ROM as G1					// Not in vt100
    Esc  )   1
    033 051 061
  Alternate ROM special characters character set as G1	// Not in vt100
    Esc  )   2
    033 051 062

  Selects G2 for one character				// Not in vt100
    Esc  N
    033 115
  Selects G3 for one character				// Not in vt100
    Esc  O
    033 117


Set graphic rendition
  Esc  [ <n> ; <n> m
  033 133   073   156

  Where <n> is
   0 = Turn off attributes
   1 = Bold (Full)
   2 = Half				// Not in vt100
   4 = Underline
   5 = Blink
   7 = Reverse
  21 = Normal intensity
  22 = Normal intensity
  24 = Cancel underlined
  25 = Cancel blinking
  27 = Cancel reverse

Tab stops
  Set horizontal tab
    Esc  H
    033 110
  Clear horizontal tab
       Esc  [   g
       033 133 147
    Or Esc  [   0   g
       033 133 060 147
  Clear all horizontal tabs
    Esc  [   3   g
    033 133 063 147


Line attributes
  Double-height
    Top half
      Esc  #   3
      033 043 063
    Bottom half
      Esc  #   4
      033 043 064
  Single-width, single-height
    Esc  #   5
    033 043 065
  Double-width
    Esc  #   6
    033 043 066


Erasing
  Erase in line
    End of line (including cursor position)
         Esc  [   K
         033 133 113
      Or Esc  [   0   K
         033 133 060 113
    Beginning of line (including cursor position)
      Esc  [   1   K
      033 133 061 113
    Complete line
      Esc  [   2   K
      033 133 062 113
  Erase in display
    End of screen (including cursor position)
         Esc  [   J
         033 133 112
      Or Esc  [   0   J
         033 133 060 112
    Beginning of screen (including cursor position)
      Esc  [   1   J
      033 133 061 112
    Complete display
      Esc  [   2   J
      033 133 062 112


Computer editing
  Delete characters (<n> characters right from cursor
    Esc  [ <n> P
    033 133   120
  Inser line (<n> lines)
    Esc  [ <n> L
    033 133   114
  Delete line (<n> lines)
    Esc  [ <n> M
    033 133   115


Printing
  Esc  [ <c> i
  033 133    151

  Where <c> is
      ''= Same as '0'
     '0'= Prints screen (full or scroll region)
     '4'= Printer controller off
     '5'= Printer controller on (Print all received chars to printer)
    '?1'= Print cursor line
    '?4'= Auto print off
    '?5'= Auto print on (Prints line to printer when you exit from it)


Reports
  Device status
    Esc  [ <c> n
    033 133   156

  Where <c> is
      '0'=Response Ready, no malfunctions detected
      '3'=Malfunction, error in self-test.
      '5'=Status report request
      '6'=Request cursor position.
    '?10'=Response to printer status request, All ok.
    '?11'=Response to printer status request, Printer is not ready.
    '?13'=Response to printer status request, No printer.
    '?15'=Status report request from printer

  Cursor position raport (Response to request cursor position)
    Esc  [ <n1> ; <n2> R
    033 133    073    122
  Request terminal to identify itself (esc Z may not be supported in future)
    Esc  [   c
    033 133 143
    Esc  [   0   c
    033 133 060 143
    Esc  Z
    033 132
  Response to terminal identify (VT102)
    Esc  [   ?   6   c
    033 133 077 066 143


Reset to initial state
  Esc  c
  033 143


Tests
  Invoke confidence test
    Esc  [   2   ; <n> y
    033 133 062 073   171

  Where <n> is
     '1'= Power-up test
     '2'= Data loopback test
     '4'= EIA loopback test
     '9'= Power-up tests (continuously)
    '10'= Data loopback tests (continuously)
    '12'= EIA loopback tests (continuously)
    '16'= Printer loopback test
    '24'= Printer loopback tests (continuously)


Screen adjustments
  Esc  #   8
  033 043 070


Keyboard indicator
  Led L1 off
    Esc  [   0   q
    033 133 060 181
  Led L1 on
    Esc  [   1   q
    033 133 061 181



VT52 sequences
  Ansi mode
    Esc  <
    033 074
  Cursor positioning
    Up    Esc  A
          033 101
    Down  Esc  B
          033 102
    Right Esc  C
          033 103
    Left  Esc  D
          033 104
    Home  Esc  H
          033 110
    Direct cursor address
      Esc  Y  <line+040> <columns+040>
      033 131
    Reverse linefeed       Esc  I
                           033 111
    Erase to end of line   Esc  K
                           033 113
    Erase to end of screen Esc  J
                           033 112
    Auto print on          Esc  ^
                           033 136
    Auto print off         Esc
                           033 137
    Printer controller on  Esc  W
                           033 127
    Printer controller off Esc  X
                           033 130
    Print cursor line      Esc  V
                           033 135
    Print screen           Esc  ]
                           033 135
    Indentify request      Esc  Z
                           033 132
    Response to indetify   Esc  /   Z
     request (VT52)        033 057 132
    Special charset (same  Esc  F
     as line draw in VT102 033 106
    Normal char set        Esc  G
                           033 107


Control characters
  000 = Null (fill character)
  003 = ETX (Can be selected half-duplex turnaround char)
  004 = EOT (Can be turnaround or disconnect char, if turn, then DLE-EOT=disc.)
  005 = ENQ (Transmits answerback message)
  007 = BEL (Generates bell tone)
  010 = BS  (Moves cursor left)
  011 = HT  (Moves cursor to next tab)
  012 = LF  (Linefeed or New line operation)
  013 = VT  (Processed as LF)
  014 = FF  (Processed as LF, can be selected turnaround char)
  015 = CR  (Moves cursor to left margin, can be turnaround char)
  016 = SO  (Selects G1 charset)
  017 = SI  (Selects G0 charset)
  021 = DC1 (XON, causes terminal to continue transmit)
  023 = DC3 (XOFF, causes terminal to stop transmitting)
  030 = CAN (Cancels escape sequence)
  032 = SUB (Processed as CAN)
  033 = ESC (Processed as sequence indicator)



**/

/**

Parameters used in ANSI escape sequences

Pn
    Numeric parameter. Specifies a decimal number.

Ps
    Selective parameter. Specifies a decimal number that you use to select
    a function. You can specify more than one function by separating the
    parameters with semicolons.

PL
    Line parameter. Specifies a decimal number that represents one of the
    lines on your display or on another device.

Pc
    Column parameter. Specifies a decimal number that represents one of the
    columns on your screen or on another device.

ESC[PL;PcH
    Cursor Position: Moves the cursor to the specified position
    (coordinates). If you do not specify a position, the cursor moves to the
    home position��the upper-left corner of the screen (line 0, column
    0). This escape sequence works the same way as the following Cursor
    Position escape sequence.

ESC[PL;Pcf
    Cursor Position: Works the same way as the preceding Cursor Position
    escape sequence.

ESC[PnA
    Cursor Up: Moves the cursor up by the specified number of lines without
    changing columns. If the cursor is already on the top line, ANSI.SYS
    ignores this sequence.

ESC[PnB
    Cursor Down: Moves the cursor down by the specified number of lines
    without changing columns. If the cursor is already on the bottom line,
    ANSI.SYS ignores this sequence.

ESC[PnC
    Cursor Forward: Moves the cursor forward by the specified number of
    columns without changing lines. If the cursor is already in the
    rightmost column, ANSI.SYS ignores this sequence.

ESC[PnD
    Cursor Backward: Moves the cursor back by the specified number of
    columns without changing lines. If the cursor is already in the leftmost
    column, ANSI.SYS ignores this sequence.

ESC[s
    Save Cursor Position: Saves the current cursor position. You can move
    the cursor to the saved cursor position by using the Restore Cursor
    Position sequence.

ESC[u
    Restore Cursor Position: Returns the cursor to the position stored
    by the Save Cursor Position sequence.

ESC[2J
    Erase Display: Clears the screen and moves the cursor to the home
    position (line 0, column 0).

ESC[K
    Erase Line: Clears all characters from the cursor position to the
    end of the line (including the character at the cursor position).

ESC[Ps;...;Psm
    Set Graphics Mode: Calls the graphics functions specified by the
    following values. These specified functions remain active until the next
    occurrence of this escape sequence. Graphics mode changes the colors and
    attributes of text (such as bold and underline) displayed on the
    screen.

    Text attributes
       0    All attributes off
       1    Bold on
       4    Underscore on
       5    Blink on
       7    Reverse video on
       8    Concealed on

    Foreground colors
       30    Black
       31    Red
       32    Green
       33    Yellow
       34    Blue
       35    Magenta
       36    Cyan
       37    White

    Background colors
       40    Black
       41    Red
       42    Green
       43    Yellow
       44    Blue
       45    Magenta
       46    Cyan
       47    White

    Parameters 30 through 47 meet the ISO 6429 standard.
    
**/

#include "komportemulation.h"

#include <QApplication>
#include <QDebug>

#define ASCII_BEL   0x07
#define ASCII_BS    0x08
#define ASCII_HT    0x09
#define ASCII_LF    0x0A
#define ASCII_CR    0x0D
#define ASCII_ESC   0x1B

namespace {

  // A generous cap, far larger than any plausible terminal screen, applied
  // to every parsed CSI repeat-count/position parameter. Without it, a
  // host sending e.g. "ESC[2147483647C" would overflow the signed
  // pos.x()+n arithmetic in doCursorRight() et al. before the result gets
  // clamped to the screen bounds (signed overflow is undefined behaviour).
  // Capping the parsed value itself, long before it reaches any arithmetic,
  // closes that off regardless of screen size.
  constexpr int MaxCtlParam = 10000;

  // Hard cap on the raw, not-yet-parsed CSI sequence buffer
  // (KomportEmulation::mCtlSequence) itself, applied in sequence() below.
  // Without it, a host that sends "ESC[" followed by an endless run of
  // digits/semicolons and never a final letter would make mCtlSequence grow
  // without bound (adversarial or simply malfunctioning device) - unbounded
  // memory growth, and the terminal never processes another character while
  // it happens. No real VT100/VT102/xterm control sequence approaches this
  // length, so aborting the sequence past this point is always safe.
  constexpr int MaxCtlSequenceLength = 256;

  // Split a CSI parameter string (already stripped of any leading "?") on
  // ';' into its fields. Shared by ctlParam(), doCursorTo(), doGraphics()
  // and doSetMode(), which used to each duplicate this same loop.
  QList<QByteArray> splitCsiParams(const QByteArray &_seq)
  {
    QList<QByteArray> fields;
    int index = 0;
    int sep;
    do {
      sep = _seq.indexOf(';', index);
      if ( sep < 0 ) sep = _seq.length();
      fields.append( _seq.mid(index, sep-index) );
      index = sep+1;
    } while ( sep < _seq.length() );
    return fields;
  }

  // Strip a leading "?" (marks a DEC private-mode sequence, e.g. CSI
  // ?25h), reporting via _isPrivate whether one was present. Shared by
  // ctlParam() and doSetMode().
  QByteArray stripPrivatePrefix(const QByteArray &_seq, bool *_isPrivate)
  {
    if ( _seq.startsWith('?') ) {
      if ( _isPrivate ) *_isPrivate = true;
      return _seq.mid(1);
    }
    if ( _isPrivate ) *_isPrivate = false;
    return _seq;
  }

} // namespace

KomportEmulation::KomportEmulation(KomportSerial* _serial, KomportCellArray* _cellArray)
: mSerial(_serial)
, mCellArray(_cellArray)
, mSawESC(false)
, mInCtlSequence(false)
, mPendingCharsetChar(false)
, mApplicationCursorKeys(false)
, mLineEnding(LineEnding::CR)
{
  QObject::connect(serial(),SIGNAL(receivedChar(char)),this,SLOT(slotReceivedChar(char)));
}

/** the raw bytes for the current line ending */
QByteArray KomportEmulation::lineEndingBytes() const {
  switch ( mLineEnding ) {
    case LineEnding::LF:   return QByteArray("\n");
    case LineEnding::CRLF: return QByteArray("\r\n");
    case LineEnding::CR:
    default:               return QByteArray("\r");
  }
}

/** parse mCtlSequence (optionally "?"-prefixed) as a single decimal parameter */
int KomportEmulation::ctlParam(int _def) const {
  const QByteArray seq = stripPrivatePrefix(mCtlSequence, nullptr);
  const QList<QByteArray> fields = splitCsiParams(seq);
  if ( fields.isEmpty() || fields.first().isEmpty() ) return _def;
  bool ok = false;
  int n = fields.first().toInt(&ok);
  if ( !ok || n <= 0 ) return _def;
  return qMin(n, MaxCtlParam); // see MaxCtlParam above - avoids overflow further downstream
}

KomportEmulation::~KomportEmulation(){
}

// key press input */
void KomportEmulation::slotKeyPressed(QKeyEvent* _e)
{
  KomportSerial* s = serial();
  if ( s->isOpen() ) {
    // DECCKM: application cursor-key mode sends "ESC O x" instead of the
    // normal "ESC [ x" - real hardware (Cisco/Juniper CLIs, vi, htop, ...)
    // switches this on for full-screen apps and back off for line input.
    const char *cursorPrefix = mApplicationCursorKeys ? "O" : "[";
    switch( _e->key() ) {
      case Qt::Key_Insert: s->putChar(ASCII_ESC); s->putStr("[1~");  break;
      case Qt::Key_Delete: s->putChar(ASCII_ESC); s->putStr("[4~"); break;
      case Qt::Key_Home:  s->putChar(ASCII_ESC); s->putStr("[2~"); break;
      case Qt::Key_End:  s->putChar(ASCII_ESC); s->putStr("[5~"); break;
      case Qt::Key_PageUp: s->putChar(ASCII_ESC); s->putStr("[3~"); break;
      case Qt::Key_PageDown: s->putChar(ASCII_ESC); s->putStr("[6~"); break;
      case Qt::Key_Left: s->putChar(ASCII_ESC); s->putStr(cursorPrefix); s->putStr("D"); break;
      case Qt::Key_Up: s->putChar(ASCII_ESC); s->putStr(cursorPrefix); s->putStr("A"); break;
      case Qt::Key_Right: s->putChar(ASCII_ESC); s->putStr(cursorPrefix); s->putStr("C"); break;
      case Qt::Key_Down : s->putChar(ASCII_ESC); s->putStr(cursorPrefix); s->putStr("B"); break;
      case Qt::Key_Return:
      case Qt::Key_Enter:
        // What Return sends is configurable (see KomportApp's line-ending
        // toolbar dropdown) - some gear only understands a bare CR, Unix
        // hosts expect LF.
        s->putStr( lineEndingBytes().constData() );
        break;
      default:
      {
        // QKeyEvent::ascii() was removed in Qt6 - text() carries the same
        // information (the ASCII/Latin-1 char this key press produces, if
        // any) for the plain-ASCII dumb-terminal input this emulation
        // targets.
        const QString text = _e->text();
        if ( !text.isEmpty() ) {
          char ch = text.at(0).toLatin1();
          s->putChar(ch);
        }
        break;
        }
    }
  }
}

/** move cursor to x,y (CSI Pr;PcH / CSI Pr;Pcf), clamped to the grid.
 *  NOTE: the original only clamped to >= 0, never to the upper bound, so
 *  "ESC[9999;5H" (or any row/col past the edge) left the cursor sitting
 *  on an out-of-range cell; the next drawChar()/doClearEOL()/doDeleteChar()
 *  etc. would then index cell(x,y) out of bounds - cell() returns nullptr
 *  there, and every one of those callers dereferences it unconditionally.
 *  Clamping here, like the relative cursor-movement handlers already do,
 *  closes that off for every command that positions the cursor. */
void KomportEmulation::doCursorTo()
{
  const QList<QByteArray> fields = splitCsiParams(mCtlSequence);
  int row = ( fields.size() > 0 && !fields.at(0).isEmpty() ) ? fields.at(0).toInt()-1 : 0;
  int col = ( fields.size() > 1 && !fields.at(1).isEmpty() ) ? fields.at(1).toInt()-1 : 0;
  row = qBound( 0, row, cellArray()->arrayHeight()-1 );
  col = qBound( 0, col, cellArray()->arrayWidth()-1 );
  cellArray()->setCursor(QPoint(col,row));
}

/** cursor up <n> rows (CSI Pn A), clamped to the top row */
void KomportEmulation::doCursorUp()
{
  QPoint pos = cellArray()->cursor();
  int n = ctlParam(1);
  pos.setY( qMax(0, pos.y()-n) );
  cellArray()->setCursor(pos);
}

/** cursor down <n> rows (CSI Pn B), clamped to the bottom row.
 *  NOTE: the original clamped with "< arrayHeight()" (off by one), which let
 *  the cursor land one row past the last valid row - the next character
 *  drawn there would call cell(x,y) on an out-of-range row, which returns
 *  nullptr, and drawChar() dereferenced it unconditionally: a crash. Fixed
 *  here (and in doCursorRight() below, same bug on the x axis). */
void KomportEmulation::doCursorDown()
{
  QPoint pos = cellArray()->cursor();
  int n = ctlParam(1);
  pos.setY( qMin(cellArray()->arrayHeight()-1, pos.y()+n) );
  cellArray()->setCursor(pos);
}

/** cursor left <n> columns (CSI Pn D), clamped to the left margin */
void KomportEmulation::doCursorLeft()
{
  QPoint pos = cellArray()->cursor();
  int n = ctlParam(1);
  pos.setX( qMax(0, pos.x()-n) );
  cellArray()->setCursor(pos);
}

/** cursor right <n> columns (CSI Pn C), clamped to the right margin */
void KomportEmulation::doCursorRight()
{
  QPoint pos = cellArray()->cursor();
  int n = ctlParam(1);
  pos.setX( qMin(cellArray()->arrayWidth()-1, pos.x()+n) );
  cellArray()->setCursor(pos);
}

void KomportEmulation::doClearEOL()
{
   int attr = mCtlSequence.isEmpty() ? 0 : mCtlSequence.toInt();
   switch(attr) {
   case 0:  // cursor to EOL
    cellArray()->clearEOL();
    break;
   case 1: // BOL to cursor
    {
      QPoint save = cellArray()->cursor();
      for( int x=0; x<=save.x();x++ ) {
        cellArray()->cell(x,save.y())->clear();
      }
      cellArray()->setCursor(save);
     }
     break;
   case 2: // full line
    {
      QPoint save = cellArray()->cursor();
      cellArray()->setCursor(0,save.y());
      cellArray()->clearEOL();
      cellArray()->setCursor(save);
    }
    break;
   }
}

void KomportEmulation::doClearScreen()
{
   int attr = mCtlSequence.isEmpty() ? 0 : mCtlSequence.toInt();
   switch(attr) {
   case 0:  // cursor to EOD
    {
      cellArray()->clearEOL();
      QPoint pos = cellArray()->cursor();
      QPoint save = pos;
      for( pos.setX(0), pos.setY( pos.y()+1 );  pos.y() < cellArray()->arrayHeight(); pos.setY( pos.y()+1 )  ) {
        cellArray()->setCursor(pos);
        cellArray()->clearEOL();
      }
      cellArray()->setCursor(save);
    }
    break;
   case 1: // BOD to cursor
     {
      QPoint save = cellArray()->cursor();
      for( int y=0; y < cellArray()->arrayHeight(); y++ ) {
        for (int x=0; x < cellArray()->arrayWidth();x++ ) {
          cellArray()->cell(x,y)->clear();
          if ( x==save.x() && y==save.y() )
            break;
        }
      }
      cellArray()->setCursor(save);
     }
     break;
   case 2: // full display
    cellArray()->clear();
    break;
   }
}

/** save cursor */
void KomportEmulation::doSaveCursor(){
  mSaveCursor = cellArray()->cursor();
}

/** restore cursor */
void KomportEmulation::doRestoreCursor(){
  cellArray()->setCursor(mSaveCursor);
}

/** do graphics attributes */
void KomportEmulation::doGraphics(){
  const QList<QByteArray> fields = splitCsiParams( mCtlSequence.isEmpty() ? QByteArray("0") : mCtlSequence );
  for ( const QByteArray &attrStr : fields ) {
    if ( !attrStr.isEmpty() ) {
      int attr = attrStr.toInt();
      switch(attr) {

        //    Text attributes
        case 0:   //    All attributes off
          {
            cellArray()->setBackgroundColor(cellArray()->defaultBackgroundColor());
            cellArray()->setForegroundColor(cellArray()->defaultForegroundColor());
            cellArray()->setBlink(false);
            cellArray()->setBold(false);
            cellArray()->setReverse(false);
            cellArray()->setUnderline(false);
          }
          break;
        case 1:   //    Bold on
          cellArray()->setBold(true);
          break;
        case 4:   //    Underscore on
          cellArray()->setUnderline(true);
          break;
        case 5:   //    Blink on
          cellArray()->setBlink(true);
          break;
        case 7:   //    Reverse video on
          cellArray()->setReverse(true);
          break;
        case 8:   //    Concealed on
          break;
        case 21:  //    Normal intensity (documented alias of 22 below)
        case 22:  //    Normal intensity / bold off
          cellArray()->setBold(false);
          break;
        case 24:  //    Cancel underlined
          cellArray()->setUnderline(false);
          break;
        case 25:  //    Cancel blinking
          cellArray()->setBlink(false);
          break;
        case 27:  //    Cancel reverse
          cellArray()->setReverse(false);
          break;

        //    Foreground colors
        case 30:  //    Black
          cellArray()->setForegroundColor(QColor(0,0,0));       break;
        case 31:  //    Red
          cellArray()->setForegroundColor(QColor(255,0,0));     break;
        case 32:  //    Green
          cellArray()->setForegroundColor(QColor(0,255,0));     break;
        case 33:  //    Yellow
          cellArray()->setForegroundColor(QColor(240,240,10));  break;
        case 34:  //    Blue
          cellArray()->setForegroundColor(QColor(0,0,255));     break;
        case 35:  //    Magenta
          cellArray()->setForegroundColor(QColor(215,15,230));  break;
        case 36:  //    Cyan
          cellArray()->setForegroundColor(QColor(10,240,230));  break;
        case 37:  //    White
          cellArray()->setForegroundColor(QColor(255,255,255)); break;
        case 39:  //    Default foreground
          cellArray()->setForegroundColor(cellArray()->defaultForegroundColor()); break;

        //  Background colors
        case 40:  //    Black
          cellArray()->setBackgroundColor(QColor(0,0,0));       break;
        case 41:  //    Red
          cellArray()->setBackgroundColor(QColor(255,0,0));     break;
        case 42:  //    Green
          cellArray()->setBackgroundColor(QColor(0,255,0));     break;
        case 43:  //    Yellow
          cellArray()->setBackgroundColor(QColor(240,240,10));  break;
        case 44:  //    Blue
          cellArray()->setBackgroundColor(QColor(0,0,255));     break;
        case 45:  //    Magenta
          cellArray()->setBackgroundColor(QColor(215,15,230));  break;
        case 46:  //    Cyan
          cellArray()->setBackgroundColor(QColor(10,240,230));  break;
        case 47:  //    White
          cellArray()->setBackgroundColor(QColor(255,255,255)); break;
        case 49:  //    Default background
          cellArray()->setBackgroundColor(cellArray()->defaultBackgroundColor()); break;

        //    Bright ("aixterm") foreground colors 90-97 - not in the
        //    original vt102 doc block above (that's xterm-era ANSI), but
        //    real-world gear (Cisco/Juniper CLIs, colored `ls`, ...) uses
        //    them routinely, so a "complete" ANSI color implementation
        //    needs them too. Same base hues as 30-37, lightened.
        // QColor::lighter() multiplies the HSV value component, which is
        // 0 for pure black and so never brightens - use an explicit grey
        // for "bright black" instead.
        case 90:  cellArray()->setForegroundColor(QColor(85,85,85));                 break;
        case 91:  cellArray()->setForegroundColor(QColor(255,0,0).lighter(140));     break;
        case 92:  cellArray()->setForegroundColor(QColor(0,255,0).lighter(140));     break;
        case 93:  cellArray()->setForegroundColor(QColor(240,240,10).lighter(140));  break;
        case 94:  cellArray()->setForegroundColor(QColor(0,0,255).lighter(140));     break;
        case 95:  cellArray()->setForegroundColor(QColor(215,15,230).lighter(140));  break;
        case 96:  cellArray()->setForegroundColor(QColor(10,240,230).lighter(140));  break;
        case 97:  cellArray()->setForegroundColor(QColor(255,255,255));              break;

        //    Bright background colors 100-107
        case 100: cellArray()->setBackgroundColor(QColor(85,85,85));                 break;
        case 101: cellArray()->setBackgroundColor(QColor(255,0,0).lighter(140));     break;
        case 102: cellArray()->setBackgroundColor(QColor(0,255,0).lighter(140));     break;
        case 103: cellArray()->setBackgroundColor(QColor(240,240,10).lighter(140));  break;
        case 104: cellArray()->setBackgroundColor(QColor(0,0,255).lighter(140));     break;
        case 105: cellArray()->setBackgroundColor(QColor(215,15,230).lighter(140));  break;
        case 106: cellArray()->setBackgroundColor(QColor(10,240,230).lighter(140));  break;
        case 107: cellArray()->setBackgroundColor(QColor(255,255,255));              break;

        default:
          qDebug( "?attr? %d", attr);
          break;
      }
    }
  }
}

/** index: cursor down, scrolling at the bottom margin (ESC D) */
void KomportEmulation::doIndex()
{
  QPoint pos = cellArray()->cursor();
  pos.setY( pos.y()+1 );
  if ( pos.y() >= cellArray()->arrayHeight() ) {
    pos.setY( cellArray()->arrayHeight()-1 );
    cellArray()->scrollUp();
  }
  cellArray()->setCursor(pos);
}

/** reverse index: cursor up, stopping at the top row (ESC M).
 *  NOTE: a "full" reverse index scrolls the screen *down* (inserting a
 *  blank line at the top) once the cursor is already on the top row -
 *  that needs a symmetric scrollDown()/scroll-region implementation that
 *  KomportCellArray does not have (it only ever scrolls forward). Simply
 *  stopping at the top row, like the original doCursorUp() already did, is
 *  the safe subset implemented here; scrolling down is a known gap. */
void KomportEmulation::doReverseIndex()
{
  QPoint pos = cellArray()->cursor();
  if ( pos.y() > 0 ) {
    pos.setY( pos.y()-1 );
    cellArray()->setCursor(pos);
  }
}

/** next line: CR + index (ESC E) */
void KomportEmulation::doNextLine()
{
  QPoint pos = cellArray()->cursor();
  pos.setX(0);
  cellArray()->setCursor(pos);
  doIndex();
}

/** reset to initial state (ESC c) */
void KomportEmulation::doReset()
{
  cellArray()->setBackgroundColor(cellArray()->defaultBackgroundColor());
  cellArray()->setForegroundColor(cellArray()->defaultForegroundColor());
  cellArray()->setBlink(false);
  cellArray()->setBold(false);
  cellArray()->setReverse(false);
  cellArray()->setUnderline(false);
  cellArray()->setCursorVisible(true);
  mApplicationCursorKeys = false;
  cellArray()->clear();
  cellArray()->setCursor(QPoint(0,0));
}

/** insert <n> blank lines at the cursor row, pushing the rows below it (and
 *  the bottom margin's worth of the screen) down - CSI Pn L */
void KomportEmulation::doInsertLine()
{
  int n = ctlParam(1);
  int h = cellArray()->arrayHeight();
  int y = cellArray()->cursor().y();
  if ( n > h-y ) n = h-y;
  for ( int row = h-1; row >= y+n; row-- ) {
    cellArray()->copyRow(row, row-n);
  }
  for ( int row = y; row < y+n; row++ ) cellArray()->clearRow(row);
}

/** delete <n> lines at the cursor row, pulling the rows below it up and
 *  clearing the newly exposed rows at the bottom - CSI Pn M */
void KomportEmulation::doDeleteLine()
{
  int n = ctlParam(1);
  int h = cellArray()->arrayHeight();
  int y = cellArray()->cursor().y();
  if ( n > h-y ) n = h-y;
  for ( int row = y; row < h-n; row++ ) {
    cellArray()->copyRow(row, row+n);
  }
  for ( int row = h-n; row < h; row++ ) cellArray()->clearRow(row);
}

/** delete <n> characters at the cursor, shifting the rest of the row left
 *  and blanking the exposed columns at the end - CSI Pn P */
void KomportEmulation::doDeleteChar()
{
  int n = ctlParam(1);
  int w = cellArray()->arrayWidth();
  int y = cellArray()->cursor().y();
  int x0 = cellArray()->cursor().x();
  if ( n > w-x0 ) n = w-x0;
  for ( int x=x0; x < w-n; x++ ) cellArray()->cell(x,y)->copy( cellArray()->cell(x+n,y) );
  for ( int x=qMax(x0,w-n); x < w; x++ ) cellArray()->cell(x,y)->clear();
  cellArray()->updateRow(y);
}

/** set/reset mode (CSI Ps h / CSI Ps l), including private ("?"-prefixed)
 *  modes. Unsupported modes are safely ignored (consumed, no side effect)
 *  rather than leaking their sequence onto the screen or crashing. */
void KomportEmulation::doSetMode(bool _set)
{
  bool priv = false;
  const QByteArray seq = stripPrivatePrefix(mCtlSequence, &priv);
  const QList<QByteArray> fields = splitCsiParams(seq);

  for ( const QByteArray &modeStr : fields ) {
    if ( modeStr.isEmpty() ) continue;
    int mode = modeStr.toInt();
    if ( priv ) {
      switch ( mode ) {
        case 1:  mApplicationCursorKeys = _set; break;          // DECCKM
        case 25: cellArray()->setCursorVisible(_set); break;    // DECTCEM
        default: break; // ?2/?3/?4/?5/?6/?7/?8/... not implemented, ignored safely
      }
    }
    // non-private modes (2=keyboard lock, 4=insert mode, 12=echo,
    // 20=CR/LF mapping, ...) are not implemented; ignored safely.
  }
}

/** device status / cursor position report (CSI Ps n) */
void KomportEmulation::doDeviceStatusReport()
{
  int code = ctlParam(0);
  switch ( code ) {
    case 5: // status report request -> "ready, no malfunction"
      serial()->putStr( "\x1b[0n" );
      break;
    case 6: // cursor position report request
      {
        QPoint pos = cellArray()->cursor();
        QByteArray reply = "\x1b[" + QByteArray::number(pos.y()+1) + ";" + QByteArray::number(pos.x()+1) + "R";
        serial()->putStr( reply.constData() );
      }
      break;
    default:
      break;
  }
}

/** device attributes / "who are you" (CSI c, CSI 0c) - always answers as a
 *  VT102, matching this emulation's documented scope. */
void KomportEmulation::doDeviceAttributes()
{
  serial()->putStr( "\x1b[?6c" );
}

// received part of an escape sequence
void KomportEmulation::sequence(char _ch)
{
  bool completed=(_ch>='a'&&_ch<='z')||(_ch>='A'&&_ch<='Z');
  
  if ( completed ) {
   // debug( "ESC[%s%c",  mCtlSequence.data()==NULL?"": mCtlSequence.data(),_ch);
    switch( _ch ) {
      case 'H':   // cursor position
      case 'f':
        doCursorTo();
        break;
      case 'A':   // cursor up
        doCursorUp();
        break;
      case 'B':   // cursor down
        doCursorDown();
        break;
      case 'C':   // cursor forward
        doCursorRight();
        break;
      case 'D':   // cursor backward
        doCursorLeft();
        break;
      case 'J':   // erase display
        doClearScreen();
        break;
      case 'K':   // erase line
        doClearEOL();
        break;
      case 'm':   // graphics attributes
        doGraphics();
        break;
      case 's':   // save cursor position
        doSaveCursor();
        break;
      case 'u':   // restore cursor position
        doRestoreCursor();
        break;
      case 'L':   // insert line(s)
        doInsertLine();
        break;
      case 'M':   // delete line(s)
        doDeleteLine();
        break;
      case 'P':   // delete character(s)
        doDeleteChar();
        break;
      case 'h':   // set mode
        doSetMode(true);
        break;
      case 'l':   // reset mode
        doSetMode(false);
        break;
      case 'n':   // device status / cursor position report
        doDeviceStatusReport();
        break;
      case 'c':   // device attributes ("who are you")
        doDeviceAttributes();
        break;
      case 'g':   // clear tab stop(s) - tab stops are always at every 8th
      case 'r':   // set scrolling region - not implemented (would need a
                  // scroll-region-aware scrollUp()/scrollDown() in
                  // KomportCellArray); consumed harmlessly rather than
                  // printed as garbage.
        break;
      default:
        qDebug( "?ctl? '%c'", _ch );
        break;
    }
    mSawESC = false;
    mInCtlSequence = false;
    mCtlSequence.resize(0);
  } else if ( mCtlSequence.size() >= MaxCtlSequenceLength ) {
    // Malformed/adversarial sequence - a real terminal control sequence
    // never gets remotely this long. Abort it instead of growing
    // mCtlSequence without bound; see MaxCtlSequenceLength above.
    mSawESC = false;
    mInCtlSequence = false;
    mCtlSequence.clear();
  } else {
    mCtlSequence += _ch;
  }
}

/** handle a two-character escape sequence: ESC followed directly by _ch
 *  (no '['). These are documented at the top of this file (Index, Reverse
 *  Index, Next Line, Save/Restore Cursor, Reset, keypad mode, character
 *  set selection, ...). The original implementation didn't recognise any
 *  of these at all: it fell through to the printable-character branch
 *  below *without* resetting mSawESC, so e.g. every "ESC 7" (save cursor)
 *  a real host sends would print a literal '7' on screen and then get
 *  stuck thinking it was still mid-escape-sequence for everything after
 *  it. This function, and slotReceivedChar()'s state machine below, fix
 *  that. */
void KomportEmulation::shortEscape(char _ch)
{
  switch ( _ch ) {
    case 'D': doIndex(); break;
    case 'M': doReverseIndex(); break;
    case 'E': doNextLine(); break;
    case '7': doSaveCursor(); break;
    case '8': doRestoreCursor(); break;
    case 'c': doReset(); break;
    case 'H': break; // set horizontal tab stop - tab stops are fixed at every 8th column, ignored
    case '(': case ')': // select G0/G1 character set - consume the designator that follows
      mPendingCharsetChar = true;
      break;
    case '=': case '>': // application/numeric keypad mode - not implemented, ignored safely
    case 'N': case 'O': // single-shift G2/G3 - not implemented, ignored safely
    default:
      break;
  }
}

// received a char */
void KomportEmulation::slotReceivedChar(char _ch)
{
  // handle the terminal control sequence... a fresh ESC always takes
  // priority and (re)starts sequence recognition - real hosts do send ESC
  // to abort a sequence, and this must win even over a pending charset
  // designator below: checking it *after* that check let a genuine ESC
  // arriving right after "ESC (" / "ESC )" get silently swallowed as if it
  // were the (bogus) designator byte, instead of starting the new
  // sequence it actually was.
  if ( _ch == ASCII_ESC ) {
    mPendingCharsetChar = false;
    mSawESC = true;
    mInCtlSequence = false;
    mCtlSequence.clear();
    return;
  }

  // a character-set designator following ESC ( or ESC ) - swallow it, see
  // shortEscape() above.
  if ( mPendingCharsetChar ) {
    mPendingCharsetChar = false;
    return;
  }
  if ( mSawESC && !mInCtlSequence ) {
    if ( _ch == '[' ) {
      mInCtlSequence = true;
      mCtlSequence.clear();
    } else {
      shortEscape(_ch);
      mSawESC = false;
    }
    return;
  }
  if ( mInCtlSequence ) {
    sequence(_ch);
    return;
  }

  // handle standard dumb terminal codes and printable ASCII....
  switch(_ch) {
    case ASCII_BEL:
      {
        QApplication::beep();
      }
      break;
    case ASCII_BS:
      doCursorLeft();
      break;
    case ASCII_HT:
      {
        QPoint pos = cellArray()->cursor();
        int next = ((pos.x()/8)+1)*8;
        if ( next >= cellArray()->arrayWidth() ) next = cellArray()->arrayWidth()-1;
        pos.setX(next);
        cellArray()->setCursor(pos);
      }
      break;
    case ASCII_LF:
      {
        QPoint pos = cellArray()->cursor();
        pos.setY(pos.y()+1);
        if ( pos.y() >= cellArray()->arrayHeight() ) {
          pos.setY(cellArray()->arrayHeight()-1);
          cellArray()->scrollUp();
        }
        cellArray()->setCursor(pos);
      }
      break;
    case ASCII_CR:
      {
        QPoint pos = cellArray()->cursor();
        pos.setX(0);
        cellArray()->setCursor(pos);
      }
      break;
    default:
      cellArray()->drawChar(_ch,cellArray()->cursor());
      cellArray()->advanceCursor();
      break;
  }
}

/** simulated key press for pasting from clipboard, etc... */
void KomportEmulation::slotSimKeyPressed(QChar _c){
   KomportSerial* s = serial();
   if ( s->isOpen() ) {
      s->putChar(_c.toLatin1());
   }
}
