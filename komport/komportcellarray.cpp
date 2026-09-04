/***************************************************************************
                          komportcellarray.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : Mon Feb 17 2003
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

#include "komportcellarray.h"

#define inherited QWidget

#define _DEFAULT_FOREGROUND_   QApplication::palette().active().foreground()
#define _DEFAULT_BACKGROUND_   QApplication::palette().active().background()

KomportCellArray::KomportCellArray()
: mBlink(false)
, mBold(false)
, mReverse(false)
, mUnderline(false)
, mForegroundColor(_DEFAULT_FOREGROUND_)
, mBackgroundColor(_DEFAULT_BACKGROUND_)
,mNotify(true)
{
  mCells.setAutoDelete(true);
  initSettings();
}

KomportCellArray::~KomportCellArray(){
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
  mArraySize = _sz;
  int curcnt = mCells.count();
  int newcnt = _sz.width()*_sz.height();
  int index;
  if ( curcnt==0 ) {
      for( index=0; index < mArraySize.width()*mArraySize.height(); index++ ) {
          KomportCell *pCell = new KomportCell();
          mCells.append(pCell);
      }
  } else  if ( curcnt > newcnt ) { 
      int diff = curcnt - newcnt;
      for( index=0; index < diff; index++ )
          mCells.removeFirst();
  } else if ( curcnt < newcnt  ) {
      int diff = newcnt - curcnt;
      for( index=0; index < diff; index++ )  {
          KomportCell *pCell = new KomportCell();
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
  unsigned int index = (arrayWidth()*_y)+_x;
  if ( index < mCells.count() ) {
    return mCells.at( index );
  } else {
    return NULL;
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
  QChar old = getChar(_p);
  if (old != _c) {
    cell(_p)->setCharacter(_c);
    setCellAttributes(_p);
    if (mNotify) emit cellChanged(_p);
  }
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
    cell(x,_row)->clear();
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
  cell(_p)->clear();
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
  c->setForegroundColor(foregroundColor());
  c->setBackgroundColor(backgroundColor());
}

/** default foreground color */
QColor KomportCellArray::defaultForegroundColor(){
  return _DEFAULT_FOREGROUND_;
}

/** default background color */
QColor KomportCellArray::defaultBackgroundColor(){
  return _DEFAULT_BACKGROUND_;
}

/** clear to end of line */
void KomportCellArray::clearEOL(){
  for( int x=cursor().x(); x < arrayWidth(); x++ ) {
    clear(x,cursor().y());
  }
}
