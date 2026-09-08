/***************************************************************************
                          komportcellarray.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : Mon Feb 17 2003
    copyright            : (C) 2003 by Mike Sharkey
    email                : michael@sharkey.servebeer.com
    ported to Qt6         : 2026
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "komportcellarray.h"

#include <QApplication>
#include <QPalette>

#define _DEFAULT_FOREGROUND_   QApplication::palette().color(QPalette::Text)
#define _DEFAULT_BACKGROUND_   QApplication::palette().color(QPalette::Base)

KomportCellArray::KomportCellArray()
: mBlink(false)
, mBold(false)
, mReverse(false)
, mUnderline(false)
, mForegroundColor(_DEFAULT_FOREGROUND_)
, mBackgroundColor(_DEFAULT_BACKGROUND_)
, mForegroundIsDefault(true)
, mBackgroundIsDefault(true)
,mNotify(true)
, mDefaultForegroundColor(_DEFAULT_FOREGROUND_)
, mDefaultBackgroundColor(_DEFAULT_BACKGROUND_)
{
  // Qt6's QList has no auto-delete like Qt3's QPtrList did - ownership of the
  // cells is handled explicitly by this class (see setArraySize() and the
  // destructor below).
  initSettings();
}

KomportCellArray::~KomportCellArray(){
  qDeleteAll(mCells);
}

/** initialize default settings */
void KomportCellArray::initSettings(){
  setArraySize( QSize( 80, 25 ) );
  setCellSize( QSize( 5,10 ) );
  setCursor( QPoint( 0, 0 ) );
}

/** set the cell array size .
 */
void KomportCellArray::setArraySize(QSize _sz){
  // Defensive clamp: callers ultimately derive this from user-editable
  // QSettings/profile values (e.g. KomportApp::applyConnectionSettings()'s
  // "ScrollBuffer" entry), which the GUI's spinbox limits but a hand-edited
  // config file does not. A negative width/height would make newcnt below
  // negative while mCells may already hold plenty of cells from a previous
  // call - diff = curcnt - newcnt then overshoots curcnt and the shrink
  // branch calls takeFirst() on an already-empty list. Clamping here (not
  // just at the profile-loading call site) makes setArraySize() itself
  // safe against bad input regardless of caller.
  if ( _sz.width() < 0 )  _sz.setWidth(0);
  if ( _sz.height() < 0 ) _sz.setHeight(0);

  // Clamp each dimension individually first, before ever looking at their
  // product: a caller that only checks the eventual cell *count* (like the
  // MaxCells logic below) could still leave one absurdly large dimension
  // sitting in mArraySize as long as the other one is 0 or tiny - and
  // other public methods (size(), width()/height(), plain iteration up to
  // arrayWidth()/arrayHeight()) use mArraySize's components directly, not
  // just the cell count, so an unclamped dimension there can still
  // overflow their own arithmetic or do wildly excessive work even though
  // the cell list itself never grew unreasonably. MaxDimension is far
  // larger than any plausible terminal size (screen pixels / a handful of
  // pixels per cell) could ever legitimately produce.
  constexpr int MaxDimension = 100'000;
  if ( _sz.width()  > MaxDimension ) _sz.setWidth(MaxDimension);
  if ( _sz.height() > MaxDimension ) _sz.setHeight(MaxDimension);

  // Also guard the width*height multiplication itself against overflowing
  // int: even two individually-legal (each <= MaxDimension) values can
  // still overflow once multiplied together, which would defeat the
  // clamps above and reintroduce exactly the same "diff overshoots
  // curcnt, takeFirst() called on an already-empty list" crash this
  // function exists to prevent - just via an overflowed newcnt instead of
  // a negative or absurd one. MaxCells is far larger than any plausible
  // terminal (a pixel-sized screen's worth of columns times a scrollback
  // in the low thousands) could ever legitimately need.
  constexpr qint64 MaxCells = 10'000'000;
  const qint64 wanted = static_cast<qint64>(_sz.width()) * static_cast<qint64>(_sz.height());
  if ( wanted > MaxCells ) {
    // Width is normally the authoritative, already-bounded dimension (the
    // fixed terminal column count); height is the one that tends to carry
    // unchecked/hand-edited input (e.g. a scrollback depth). Clamp height
    // down so the product fits, rather than trying to preserve some
    // notion of aspect ratio that doesn't really apply here.
    _sz.setHeight( _sz.width() > 0 ? static_cast<int>(MaxCells / _sz.width()) : 0 );
    // Not reachable with the current MaxCells/MaxDimension values (by the
    // time this branch can trigger, width is already <= MaxDimension and
    // this branch only runs at all when width > MaxCells/MaxDimension,
    // which makes MaxCells/width < MaxDimension automatically) - but
    // re-applying the per-dimension cap here costs nothing and keeps that
    // guarantee from silently depending on the exact numeric relationship
    // between the two constants above, in case either one is ever changed
    // on its own later.
    if ( _sz.height() > MaxDimension ) _sz.setHeight(MaxDimension);
  }

  mArraySize = _sz;
  int curcnt = mCells.count();
  int newcnt = _sz.width()*_sz.height(); // safe now: bounded to <= MaxCells above
  int index;
  if ( curcnt==0 ) {
      for( index=0; index < newcnt; index++ ) {
          KomportCell *pCell = new KomportCell();
          // New cells start out colored from KomportCell's own ctor
          // default (the OS palette) - sync to *this array's* current
          // default colors instead, in case a profile color scheme was
          // already applied before this array was ever sized (Milestone 5:
          // mDefaultForegroundColor/mDefaultBackgroundColor can differ from
          // the palette by the time this runs).
          pCell->setForegroundColor(mDefaultForegroundColor, true); // true: at default, not explicit SGR (Milestone 5)
          pCell->setBackgroundColor(mDefaultBackgroundColor, true);
          mCells.append(pCell);
      }
  } else  if ( curcnt > newcnt ) {
      int diff = curcnt - newcnt;
      for( index=0; index < diff; index++ )
          delete mCells.takeFirst();
  } else if ( curcnt < newcnt  ) {
      int diff = newcnt - curcnt;
      for( index=0; index < diff; index++ )  {
          KomportCell *pCell = new KomportCell();
          pCell->setForegroundColor(mDefaultForegroundColor, true); // see comment above
          pCell->setBackgroundColor(mDefaultBackgroundColor, true);
          mCells.append(pCell);
      }
  }
}

/** set cell size */
void KomportCellArray::setCellSize(QSize _sz){
  mCellSize=_sz;
}

/** return calculated dimensions in pixels based on cell size and array size */
QSize KomportCellArray::size(){
  return QSize( arrayWidth()*cellWidth(), arrayHeight()*cellHeight() );
}

/** get array size */
QSize KomportCellArray::arraySize(){
  return mArraySize;
}

/** get cell size */
QSize KomportCellArray::cellSize(){
  return mCellSize;
}

/** return calculated width based on cell width * array width */
int KomportCellArray::width(){
  return size().width();
}

/** return calculated heiht based on cell height * array height */
int KomportCellArray::height(){
  return size().height();
}

/** array width  */
int KomportCellArray::arrayWidth(){
  return arraySize().width();
}

/** array height */
int KomportCellArray::arrayHeight(){
  return arraySize().height();
}

/** cell width */
int KomportCellArray::cellWidth(){
  return cellSize().width();
}

/** cell height */
int KomportCellArray::cellHeight(){
  return cellSize().height();
}

/** get a pointer to the cell from location (x,y) */
KomportCell* KomportCellArray::cell(int _x,int _y){
  // _x and _y must each be validated individually, not just the resulting
  // flat index: an out-of-range _x (negative, or >= arrayWidth()) still
  // produces an in-bounds *flat* index as long as _y compensates for it,
  // silently returning a cell from a neighbouring row instead of the
  // out-of-range signal (nullptr) callers actually expect. E.g. with an
  // 80-column grid, cell(-1, 1) computed index 80*1-1=79 - a "valid"
  // index, but that's row 0's last column, not anything belonging to row
  // 1. Concretely reachable via KomportView::selectStart()/selectEnd(),
  // which convert raw (unclamped) mouse pixel positions to cell
  // coordinates.
  if ( _x < 0 || _x >= arrayWidth() || _y < 0 || _y >= arrayHeight() ) {
    return nullptr;
  }
  int index = (arrayWidth()*_y)+_x;
  if ( index >= 0 && index < mCells.count() ) {
    return mCells.at( index );
  } else {
    return nullptr;
  }
}

/** get a pointer to the cell from location (x,y) */
KomportCell* KomportCellArray::cell(QPoint _p){
  return cell(_p.x(),_p.y());
}

/** draw a character into a cell */
void KomportCellArray::drawChar(QChar _c, int _x, int _y){
  drawChar(_c,QPoint(_x,_y));
}

/** draw a character into a cell */
void KomportCellArray::drawChar(QChar _c, QPoint _p){
  // Codex review finding (Milestone 5, gpt-5.6-sol round 2): this used to
  // skip setCellAttributes() (and therefore the current fg/bg color *and*
  // their isDefault provenance) whenever the character itself was
  // unchanged - a pre-existing optimization to avoid redundant
  // cellChanged() emissions for identical full-screen redraws (e.g.
  // htop/top refreshing every second). That also meant a host which
  // repositions the cursor, changes color, and rewrites the *same*
  // character (a common status-line/indicator pattern) got neither the
  // new color nor - worse, since this milestone added color-scheme
  // provenance tracking - the correct isDefault flag: the cell stayed
  // marked as still-default and a later scheme change would silently
  // recolor it, even though it had just been explicitly colored.
  // Attributes are now always applied on every drawChar() call, matching
  // how a real terminal treats every printed character: it always gets
  // whichever SGR attributes are currently active, whether or not the
  // glyph itself changed.
  cell(_p)->setCharacter(_c);
  setCellAttributes(_p);
  if (mNotify) emit cellChanged(_p);
}

/** cursor position */
QPoint KomportCellArray::cursor(){
  return mCursor;
}

/** cursor position */
void KomportCellArray::setCursor(QPoint _p){
  QPoint old = mCursor;
  if (old != _p) {
    mCursor = _p;
    if (mNotify) emit cursorChanged(old,_p);
  }
}

/** show/hide the cursor (DECTCEM) */
void KomportCellArray::setCursorVisible(bool _v){
  if ( mCursorVisible != _v ) {
    mCursorVisible = _v;
    emit cursorVisibilityChanged(_v);
  }
}

/** Insert a character at the current cursor position and advance cursor. Scroll if advance is passed the last column. */
void KomportCellArray::putChar(QChar _ch){
  drawChar(_ch,cursor());
  advanceCursor();
}

/** move the cursor once cell forward, scolling or wrapping as nessesary */
void KomportCellArray::advanceCursor(){
  QPoint pos = cursor();
  pos.setX(pos.x()+1);
  if ( pos.x() >= arrayWidth() ) {
    pos.setX(0);
    pos.setY(pos.y()+1);
    if ( pos.y() >= arrayHeight() ) {
      pos.setY(arrayHeight()-1);
      scrollUp();
    }
  }
  setCursor(pos);
}

/** retrieve a character from a cell */
QChar KomportCellArray::getChar(QPoint _p){
  return cell(_p)->character();
}

/** scroll screen up one line */
void KomportCellArray::scrollUp(){
    int h = arrayHeight();
    int w = arrayWidth();
    emit aboutToScrollUp();
    setArraySize( QSize( w, h-1 ) ); // delete the first row
    setArraySize( QSize( w, h ) ); // append a new row
    emit scrolledUp();
    clearRow(arrayHeight()-1);
}

/** scroll rows [_top,_bottom] up by one line, region-only (see header) */
void KomportCellArray::scrollUpRegion(int _top, int _bottom){
  // Defensive, like cell()/setArraySize() elsewhere in this class: with
  // arrayHeight()==0, qBound(0, x, arrayHeight()-1) == qBound(0, x, -1)
  // still clamps to 0 (a qBound with max < min just returns min), so
  // clearRow(0) below would run and call cell(x,0)->clear() on a null
  // cell() - a real crash, found by Codex review. Not reachable via the
  // live KomportView grid today (resizeGridRows() never lets it shrink
  // below 1 row), but these are public methods with no such guarantee.
  if ( arrayHeight() <= 0 ) return;
  _top = qBound(0, _top, arrayHeight()-1);
  _bottom = qBound(_top, _bottom, arrayHeight()-1);
  for ( int row = _top; row < _bottom; row++ ) {
    copyRow(row, row+1);
  }
  clearRow(_bottom);
}

/** scroll rows [_top,_bottom] down by one line, region-only (see header) */
void KomportCellArray::scrollDownRegion(int _top, int _bottom){
  if ( arrayHeight() <= 0 ) return; // see scrollUpRegion() above
  _top = qBound(0, _top, arrayHeight()-1);
  _bottom = qBound(_top, _bottom, arrayHeight()-1);
  for ( int row = _bottom; row > _top; row-- ) {
    copyRow(row, row-1);
  }
  clearRow(_top);
}

/** copy a row  */
void KomportCellArray::copyRow(int _dst, int _src){
  int w = arrayWidth();
  for( int x=0; x < w; x++ ) {
     KomportCell* srcCell = cell(x,_src);
     KomportCell* dstCell = cell(x,_dst);
     dstCell->copy( srcCell );
  }
  if ( mNotify ) emit rowChanged(_dst);
}

/** clear a row */
void KomportCellArray::clearRow(int _row){
  int w = arrayWidth();
  for( int x=0; x < w; x++ ) {
    cell(x,_row)->clear(mDefaultForegroundColor, mDefaultBackgroundColor);
  }
  if ( mNotify ) emit rowChanged(_row);
}

/** update a cell */
void KomportCellArray::updateCell(QPoint _c){
  if ( mNotify ) emit cellChanged(_c);
}

/** update cell */
void KomportCellArray::updateCell(int _x,int _y){
  updateCell(QPoint(_x,_y));
}

/** update a row */
void KomportCellArray::updateRow(int _row){
  if ( mNotify ) emit rowChanged(_row);
}

/** update all cells */
void KomportCellArray::update(){
  for( int y=0; y < arrayHeight(); y++ )
    updateRow(y);
}

/** clear all cells */
void KomportCellArray::clear(){
  for( int y=0; y < arrayHeight(); y++ )
    clearRow(y);
}

/** clear a cell */
void KomportCellArray::clear(QPoint _p)
{
  cell(_p)->clear(mDefaultForegroundColor, mDefaultBackgroundColor);
  updateCell(_p);
}


/** clear a cell */
void KomportCellArray::clear(int _x,int _y)
{
  clear(QPoint(_x,_y));
}

/** set cell attributes */
void KomportCellArray::setCellAttributes(QPoint _p){
  KomportCell* c = cell(_p);
  c->setBlink(blink());
  c->setBold(bold());
  c->setReverse(reverse());
  c->setUnderline(underline());
  // Propagate whether the *current* color is still "the default" too
  // (Milestone 5) - not just the color value itself - so a character
  // drawn right after an SGR 0/39/49 reset correctly ends up
  // foregroundIsDefault()/backgroundIsDefault() on the cell, the same as
  // one that was never explicitly colored at all.
  c->setForegroundColor(foregroundColor(), mForegroundIsDefault);
  c->setBackgroundColor(backgroundColor(), mBackgroundIsDefault);
}

/** default foreground color */
QColor KomportCellArray::defaultForegroundColor(){
  return mDefaultForegroundColor;
}

/** default background color */
QColor KomportCellArray::defaultBackgroundColor(){
  return mDefaultBackgroundColor;
}

/** change the default foreground color (Milestone 5), live-applying it to
 *  every cell currently at the default - determined via
 *  KomportCell::foregroundIsDefault() (color *provenance*), not by
 *  comparing colors: a Codex review finding caught that color-equality
 *  comparison is unsound here - a scheme's own default can legitimately be
 *  the exact same QColor a real SGR code produces (e.g. "Green on Black"'s
 *  default background is the same black SGR 40 produces), which would
 *  wrongly recolor an explicitly-black-via-SGR-40 cell right along with
 *  genuinely-still-default ones. Also updates the *current* SGR-writing
 *  color (mForegroundColor, what a newly typed character gets) if it was
 *  still at the old default, so typing right after a scheme change uses
 *  the new one too. */
void KomportCellArray::setDefaultForegroundColor(QColor _c){
  if ( _c == mDefaultForegroundColor ) return;
  mDefaultForegroundColor = _c;
  if ( mForegroundIsDefault ) mForegroundColor = _c;
  for ( KomportCell *pCell : std::as_const(mCells) ) {
    if ( pCell->foregroundIsDefault() ) pCell->setForegroundColor(_c, true);
  }
  update();
}

/** change the default background color - see setDefaultForegroundColor() above */
void KomportCellArray::setDefaultBackgroundColor(QColor _c){
  if ( _c == mDefaultBackgroundColor ) return;
  mDefaultBackgroundColor = _c;
  if ( mBackgroundIsDefault ) mBackgroundColor = _c;
  for ( KomportCell *pCell : std::as_const(mCells) ) {
    if ( pCell->backgroundIsDefault() ) pCell->setBackgroundColor(_c, true);
  }
  update();
}

/** reset the *current* (next-character-drawn) foreground color back to the
 *  default (SGR 0/39) - correctly marks it foregroundIsDefault(), unlike
 *  setForegroundColor(defaultForegroundColor()) (which would mark it as an
 *  *explicit* color that merely happens to match the default right now -
 *  exactly the ambiguity setDefaultForegroundColor() above needs to avoid). */
void KomportCellArray::resetForegroundToDefault(){
  mForegroundColor = mDefaultForegroundColor;
  mForegroundIsDefault = true;
}

/** see resetForegroundToDefault() above */
void KomportCellArray::resetBackgroundToDefault(){
  mBackgroundColor = mDefaultBackgroundColor;
  mBackgroundIsDefault = true;
}

/** clear to end of line */
void KomportCellArray::clearEOL(){
  for( int x=cursor().x(); x < arrayWidth(); x++ ) {
    clear(x,cursor().y());
  }
}
