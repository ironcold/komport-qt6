/***************************************************************************
                          komportview.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : Mon Feb 17 00:05:54 EST 2003
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

// include files for Qt
#include <qprinter.h>
#include <qpainter.h>
#include <qrect.h>
#include <qfont.h>
#include <qfontmetrics.h>

// application specific includes
#include "komportview.h"
#include "komportdoc.h"
#include "komport.h"

#define min(x,y) (x<y?x:y)
#define max(x,y) (x<y?y:x)

KomportView::KomportView(QWidget *parent, const char *name)
: QWidget(parent, name)
, mCursorState(false)
, mBlinkState(false)
, mInSelection(false)
, mHasSelection(false)
{
  mScrollBar = new QScrollBar( Qt::Vertical, parent,"scroll_bar");
  mScrollBar->setMinValue(0);
  mScrollBar->setMaxValue(0);
  QObject::connect(mScrollBar,SIGNAL(valueChanged(int)),this,SLOT(slotScroll(int)));
  
  setBackgroundMode(PaletteBase);

  QObject::connect(cellArray(),SIGNAL(cursorChanged(QPoint,QPoint)),this,SLOT(slotCursorChanged(QPoint,QPoint)));
  QObject::connect(cellArray(),SIGNAL(cellChanged(QPoint)),this,SLOT(slotCellChanged(QPoint)));
  QObject::connect(cellArray(),SIGNAL(rowChanged(int)),this,SLOT(slotRowChanged(int)));
  QObject::connect(cellArray(),SIGNAL(aboutToScrollUp()),this,SLOT(slotAboutToScrollUp()));
  QObject::connect(cellArray(),SIGNAL(scrolledUp()),this,SLOT(slotScrolledUp()));
  QObject::connect(QApplication::clipboard(),SIGNAL(selectionChanged()),this,SLOT(slotSelectionChanged()));

  mCursorTimer = startTimer( 500 );
  mBlinkTimer = startTimer( 1000 );
  mAutoScrollTimer = startTimer( 250 );
  
  setCellSize();
  setEnabled(true);
  setFocusPolicy(StrongFocus);

  mEmulation = new KomportEmulation( getSerial(), cellArray() );
  QObject::connect(this,SIGNAL(keyPressed(QKeyEvent*)),mEmulation,SLOT(slotKeyPressed(QKeyEvent*)));
  QObject::connect(this,SIGNAL(simKeyPressed(QChar)),mEmulation,SLOT(slotSimKeyPressed(QChar)));

  QObject::connect(getSerial(),SIGNAL(receivedChar(char)),this,SLOT(slotReceivedChar(char)));
}

KomportView::~KomportView()
{
 delete mScrollBar;
  //getSerial()->close();
}

KomportDoc *KomportView::getDocument() const
{
  KomportApp *theApp=(KomportApp *) parentWidget();

  return theApp->getDocument();
}

void KomportView::print(QPrinter *pPrinter)
{
  for ( int copies=0; copies < pPrinter->numCopies(); copies++ ) {
      QPainter paint;
      paint.begin(pPrinter);
      QRect bounds = paint.viewport();
      QString str="";
      if ( hasSelection() ) {
          // draw the contents of the mouse clipboard...
          str = QApplication::clipboard()->text( QClipboard::Clipboard );
      } else {
          // draw the contents of the screen...
          for( int y=0; y < cellArray()->arrayHeight(); y++ ) {
              for( int x=0; x < cellArray()->arrayWidth(); x++ ) {
                  KomportCell* cell = cellArray()->cell(x,y);
                  str += cell->character();
              }
              str += QChar( '\n' );
          }
      }
      paint.drawText( bounds, AlignAuto|PlainText, str );
      // eject the page...
      paint.end();
  }
}

/** return cell array object */
KomportCellArray* KomportView::cellArray(){
  return &mCellArray;
}

/** draw a cell */
void KomportView::paintCell( QPainter* _paint, int _x, int _y, QRect _bounds ) {
  KomportCell* cell = getCell(_x,_y);
  if ( cell != NULL ) {
    QString str( cell->character() );
    bool cursorCellOn = (cellArray()->cursor() == QPoint( _x, _y ) && mCursorState);
    QColor fillColor;
    QColor textColor;
    QColor cellBackground = cell->backgroundColor();
    QColor cellForeground = cell->foregroundColor();

    // determine forground and background color...
    if ( cell->select() ) {
        fillColor =  QApplication::palette().active().highlight();
        textColor = QApplication::palette().active().highlightedText();
    } else  if ( cursorCellOn && hasFocus() ) {
      fillColor = cell->reverse() ? cellBackground: cellForeground;
      textColor = cell->reverse() ? cellForeground:cellBackground;
    } else {
      fillColor = cell->reverse() ? cellForeground : cellBackground;
      textColor = cell->reverse() ? cellBackground : cellForeground;
    }

    _paint->fillRect( _bounds, QBrush( fillColor ) );
    _paint->setPen( textColor );
    QFont font = _paint->font();
    font.setBold( cell->bold() );
    font.setUnderline(cell->underline());
    _paint->setFont(font);
    if ( ( !cell->blink() || (cell->blink() &&  !mBlinkState) ) )
      _paint->drawText ( _bounds, Qt::AlignCenter, str );
    if ( cursorCellOn && !hasFocus() ) { // out-of-focus cursor box
      _paint->drawRect( _bounds );
    }
  }
}

/** draw a character in a cell */
void KomportView::drawChar(QChar _c,int _x, int _y){
  cellArray()->drawChar(_c,_x,_y);
  updateCell(_x,_y);
}

/** re-draw a cell */
void KomportView::updateCell(int _x,int _y){
  int cellWidth = cellArray()->cellWidth();
  int cellHeight = cellArray()->cellHeight();
  QRect cellRect( _x*cellWidth, _y*cellHeight, cellWidth, cellHeight );
  QPainter paint( &mPixmap );
  paintCell( &paint, _x, _y , cellRect  );
  paint.end();
  update( QRect( _x*cellWidth, _y*cellHeight, cellWidth, cellHeight ) );
}

/** update a cell */
void KomportView::updateCell(QPoint _p){
  updateCell( _p.x(), _p.y() );
}

/** key press event */
void KomportView::keyPressEvent(QKeyEvent* _e){
    if ( mScrollBar->value() != mScrollBar->maxValue() ) {
        resetScroll();
        slotScroll( mScrollBar->value() );
    }
    emit keyPressed(_e);
}

/** key release event */
void KomportView::keyReleaseEvent(QKeyEvent* _e){
}

/** paint event */
void KomportView::paintEvent(QPaintEvent* _e){
  QRect rect = _e->rect();
  bitBlt(this,rect.topLeft(),&mPixmap,rect,Qt::CopyROP);
}

/** timer event */
void KomportView::timerEvent(QTimerEvent* _e) {
  if ( _e->timerId() == mBlinkTimer ) {
    mBlinkState = !mBlinkState;
    int w = cellArray()->cellWidth();
    int h = cellArray()->arrayHeight();
    for ( int x=0; x < w; x++ ) {
      for ( int y=0; y < h; y++ ) {
        if ( getCell(x,y)->blink() ) {
          updateCell(x,y);
        }
      }
    }
  } else if ( _e->timerId() == mCursorTimer ) {
    mCursorState = !mCursorState;
    updateCell(cellArray()->cursor().x(),cellArray()->cursor().y());
  } else if (_e->timerId() == mAutoScrollTimer ) {
    if ( mInSelection ) {
        if ( mMousePos.y() < 0 && mScrollBar->value() > 0 ) { // scroll down (into scroll buffer )...
            int amount = 1+(abs( mMousePos.y() )/10); // calculate acceleration.
            mScrollBar->setValue( mScrollBar->value()-amount );
            slotScroll( mScrollBar->value() );
        }
        else if ( mMousePos.y() > height() && mScrollBar->value() < mScrollBar->maxValue() ) { // scroll up (out of scroll buffer)...
            int amount = 1+((mMousePos.y()-height() )/10); // calculate acceleration.
            mScrollBar->setValue( mScrollBar->value()+amount );
            slotScroll( mScrollBar->value() );
        }
    }
  }
}

/** get a cell */
KomportCell* KomportView::getCell(int _x, int _y){
  KomportCell* cell = cellArray()->cell(_x,_y);
  if ( mScrollBar->value() < mScrollBar->maxValue() ) {
        int scrolled = mScrollBar->maxValue() -  mScrollBar->value();
        if ( scrolled < cellArray()->arrayHeight() ) { // not a complete screen of scroll back...
            if ( _y < scrolled ) { // is it the scroll buffer portion?
                cell = mScrollBuffer.cell(_x, ((mScrollBuffer.arrayHeight()-1)-scrolled)+_y);
            } else { // get the cell from screen buffer with scroll offset...
                cell = cellArray()->cell(_x,_y-scrolled);
            }
        } else { // we're completely into the scroll buffer...
            cell = mScrollBuffer.cell(_x, ((mScrollBuffer.arrayHeight()-1)-scrolled)+_y);
        }
  }
  return cell;
}

/** calculates cell size based on font. */
void KomportView::setCellSize(){
  QFontMetrics fm = fontMetrics();
  cellArray()->setCellSize(QSize(fm.width(QChar('H')),fm.height()));
  QSize sz(cellArray()->width(),cellArray()->height());
  setMinimumSize(sz);
  setMaximumSize(sz);
}

/** notify cursor position has changde */
void KomportView::slotCursorChanged(QPoint _old, QPoint _new)
{
  updateCell( _old );
  mCursorState=true;
  updateCell( _new );
}

/** a cell content has changed */
void KomportView::slotCellChanged(QPoint _p){
  updateCell(_p);
}

/** update a row */
void KomportView::slotRowChanged(int _row){
  for( int x=0; x < cellArray()->arrayWidth(); x++ )
    updateCell(x,_row);
}


/** received a char */
void KomportView::slotReceivedChar(char _ch){
    if ( mScrollBar->value() != mScrollBar->maxValue() ) {
        resetScroll();
        slotScroll( mScrollBar->value() );
    }
}

/** get serial port */
KomportSerial* KomportView::getSerial(){
  return getDocument()->getSerial();
}
/** resize the offscreen pixmap and refresh */
void KomportView::resizeEvent(QResizeEvent* _e){
  mScrollBar->resize( mScrollBar->width(), height() );
  mScrollBar->move( width(), y() );
  mScrollBar->show();
  mPixmap.resize( _e->size() );
  cellArray()->update();
}
/** notification that the cell array has scrolled up so we need to scroll visually */
void KomportView::slotScrolledUp(){
    int rowHeight = cellArray()->cellHeight();
    // scroll the offscreen pixels up one row.....
    bitBlt(&mPixmap,0,0,&mPixmap,0,rowHeight,mPixmap.width(),mPixmap.height()-rowHeight,Qt::CopyROP);
    // transfer the offscreen pixels to the screen....
    //update();
    repaint(false);
}
/** before the actual scroll takes place */
void KomportView::slotAboutToScrollUp(){
    // turn off cursor...
    mCursorState = false;
    updateCell(cellArray()->cursor().x(),cellArray()->cursor().y());
    // scroll buffer andf scroll bar...
    if ( mScrollBuffer.arrayHeight() > 0 ) {
        for( int x=0; x<cellArray()->arrayWidth();x++) {
            mScrollBuffer.cell( x, mScrollBuffer.arrayHeight()-1 )->copy( cellArray()->cell(x,0) );
        }
        mScrollBuffer.scrollUp();
        resetScroll();
    }
}
/** No descriptions */
void KomportView::mousePressEvent( QMouseEvent* _e ){
    if ( _e->button() == Qt::LeftButton ) {
        mSelectStart = QPoint( 0, 0 );
        mSelectEnd = mMousePos = mSelectStart;
        deselect();
        selectStart( _e->pos() );
        mInSelection = true;
    }
}
/** No descriptions */
void KomportView::mouseReleaseEvent( QMouseEvent* _e ){
    if( _e->button() == Qt::LeftButton && mInSelection ) {
        selectEnd( _e->pos() );
        select( mSelectStart, mSelectEnd, true );
        mSelectStart = QPoint( 0, 0 );
        mSelectEnd = mMousePos = mSelectStart;
        mInSelection = false;
    }
}
/** No descriptions */
void KomportView::mouseMoveEvent( QMouseEvent* _e ){
     if ( mInSelection ) {
         selectEnd( (mMousePos = _e->pos()) );
         select( mSelectStart, mSelectEnd );
     }
}
/** No descriptions */
void KomportView::moveEvent( QMoveEvent* _e ){
    mScrollBar->resize( mScrollBar->width(), height() );
    mScrollBar->move( width(), y() );
    mScrollBar->show();
}
/** make a selection by cell coordinates */
void KomportView::select(QPoint start, QPoint end, bool clip){
    deselect();
    if ( start != end ) {
      int startX = min(start.x(),end.x());
      int startY = min(start.y(),end.y());
      int endX = max(start.x(),end.x());
      int endY = max(start.y(),end.y());
      KomportCell* cell=NULL;
      QString str;
      for( int y=startY; y <= endY; y++ ) {
          for( int x=startX; x <= endX; x++ ) {
              cell = cellArray()->cell(x,y);
              if ( cell != NULL ) {
                  cellArray()->cell(x,y)->setSelect( true );;
                  cellArray()->updateCell(x,y);
                  if ( clip )
                      str += cell->character();
              }
          }
          if ( clip && cell != NULL ) {
              str += QChar('\n');
          }
      }
      if ( clip ) {
          QApplication::clipboard()->setText(str,QClipboard::Selection);
          mHasSelection = true;
          emit viewModified(this);
      }
    }
}
/** clear selection */
void KomportView::deselect(){
    for( int y=0; y < cellArray()->arrayHeight(); y++ ) {
        for( int x=0; x < cellArray()->arrayWidth(); x++ ) {
            KomportCell* cell = cellArray()->cell(x,y);
            if ( cell->select() ) {
                cell->setSelect(false);
                cellArray()->updateCell(x,y);
            }
        }
    }
    mHasSelection = false;
    emit viewModified( this );
}
/** begin a selection by pixel coordinate */
void KomportView::selectStart( QPoint _pt ){
    mSelectStart = QPoint( _pt.x()  / cellArray()->cellWidth(), _pt.y() / cellArray()->cellHeight() );
    mSelectEnd = mSelectStart;
}
/** end a selection by pixel coordinate */
void KomportView::selectEnd( QPoint _pt ){
    mSelectEnd = QPoint( _pt.x()  / cellArray()->cellWidth(), _pt.y() / cellArray()->cellHeight() );
}
/** No descriptions */
void KomportView::slotSelectionChanged() {
    if ( !mInSelection ){
        deselect();
    }
    emit viewModified(this);
}
/** has a text selection */
bool KomportView::hasSelection(){
    return mHasSelection;
}
/** simulated key press for inserting from clipboard, etc... */
void KomportView::slotSimKeyPressed(QChar _c){
    if ( mScrollBar->value() != mScrollBar->maxValue() ) {
        resetScroll();
        slotScroll( mScrollBar->value() );
    }
    emit simKeyPressed(_c);
}
/** scroll bar moved */
void KomportView::slotScroll(int _value){
    int cellWidth = cellArray()->cellWidth();
    int cellHeight = cellArray()->cellHeight();
    QPainter paint( &mPixmap );
    for( int y=0; y < cellArray()->arrayHeight(); y++ ) {
        for ( int x=0; x < cellArray()->arrayWidth(); x++ ) {
            QRect cellRect( x*cellWidth, y*cellHeight, cellWidth, cellHeight );
            paintCell( &paint, x, y , cellRect  );
        }
    }
    paint.end();
    repaint(false);
}
/** reset scrollbar */
void KomportView::resetScroll(){
        mScrollBar->setMaxValue(mScrollBuffer.depth());
        mScrollBar->setValue(mScrollBuffer.depth());
}
/** set the number of lines in the scroll buffer */
void KomportView::setScrollBuffer(int _sz){
  mScrollBuffer.setArraySize( QSize( cellArray()->arrayWidth(), _sz ) );
  resetScroll();
}
