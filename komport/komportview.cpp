/***************************************************************************
                          komportview.cpp  -  Komport Serial Port Communicator
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

// include files for Qt
#include <QPrinter>
#include <QPainter>
#include <QRect>
#include <QFont>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDebug>

// application specific includes
#include "komportview.h"
#include "komportdoc.h"
#include "komport.h"

#define min(x,y) (x<y?x:y)
#define max(x,y) (x<y?y:x)

KomportView::KomportView(QWidget *parent)
: QWidget(parent)
, mCursorState(false)
, mBlinkState(false)
, mInSelection(false)
, mHasSelection(false)
{
  // A genuine child of this view (not a manually-positioned sibling of
  // its parent, like the original Qt3 code had it - that broke once the
  // view moved into a QSplitter for the hex monitor, showing up as a
  // stray floating bar). See komportminimap.h for the rest of the story.
  mScrollBar = new KomportMinimapScrollBar( this, this );
  mScrollBar->setObjectName( QStringLiteral("scroll_bar") );
  mScrollBar->setRange(0, 0);
  connect( mScrollBar, &KomportMinimapScrollBar::valueChanged, this, &KomportView::slotScroll );

  // We always redraw the full cell area from mPixmap in paintEvent(), so
  // avoid Qt erasing the background first (replaces Qt3's
  // setBackgroundMode(PaletteBase)).
  setAttribute(Qt::WA_OpaquePaintEvent, true);

  QObject::connect(cellArray(),SIGNAL(cursorChanged(QPoint,QPoint)),this,SLOT(slotCursorChanged(QPoint,QPoint)));
  QObject::connect(cellArray(),SIGNAL(cellChanged(QPoint)),this,SLOT(slotCellChanged(QPoint)));
  QObject::connect(cellArray(),SIGNAL(rowChanged(int)),this,SLOT(slotRowChanged(int)));
  QObject::connect(cellArray(),SIGNAL(aboutToScrollUp()),this,SLOT(slotAboutToScrollUp()));
  QObject::connect(cellArray(),SIGNAL(scrolledUp()),this,SLOT(slotScrolledUp()));
  QObject::connect(cellArray(),&KomportCellArray::cursorVisibilityChanged,this,[this](bool){
      updateCell( cellArray()->cursor() );
  });
  QObject::connect(QApplication::clipboard(),SIGNAL(selectionChanged()),this,SLOT(slotSelectionChanged()));

  mCursorTimer = startTimer( 500 );
  mBlinkTimer = startTimer( 1000 );
  mAutoScrollTimer = startTimer( 250 );

  setCellSize();
  setEnabled(true);
  setFocusPolicy(Qt::StrongFocus);

  mEmulation = new KomportEmulation( getSerial(), cellArray() );
  QObject::connect(this,SIGNAL(keyPressed(QKeyEvent*)),mEmulation,SLOT(slotKeyPressed(QKeyEvent*)));
  QObject::connect(this,SIGNAL(simKeyPressed(QChar)),mEmulation,SLOT(slotSimKeyPressed(QChar)));

  QObject::connect(getSerial(),SIGNAL(receivedChar(char)),this,SLOT(slotReceivedChar(char)));
}

KomportView::~KomportView()
{
  // mScrollBar is a real child widget now (parent == this), so Qt's
  // parent-child ownership already destroys it - deleting it again here
  // would double-free.
}

KomportDoc *KomportView::getDocument() const
{
  // window() rather than parentWidget(): this view no longer sits directly
  // under KomportApp - it's inside the central QSplitter (for the hex
  // monitor pane) - so its immediate parent is the splitter, not the main
  // window. window() walks all the way up to the top-level widget, which
  // is still KomportApp regardless of how many container widgets sit in
  // between.
  //
  // KomportView is architecturally only meant to be used with a KomportApp
  // as its top-level window (it's not a general-purpose reusable widget) -
  // getSerial() calls getDocument()->getSerial() unconditionally, and the
  // constructor calls getSerial() immediately, so there is no safe/partial
  // way to carry on if that assumption doesn't hold; returning nullptr
  // here would just move the crash one call further out, into an
  // unrelated null-pointer dereference with no indication of the real
  // cause. qobject_cast rather than a blind C-style cast (which used to
  // reinterpret whatever window() returned as a KomportApp
  // unconditionally - undefined behaviour, i.e. potentially silent memory
  // corruption, if that assumption is ever wrong) still buys something
  // real: it lets us fail loudly and exactly here, at the actual
  // precondition violation, instead of via UB or a mystery crash
  // elsewhere.
  KomportApp *theApp = qobject_cast<KomportApp *>( window() );
  if ( !theApp ) {
    qFatal( "KomportView::getDocument(): not embedded under a KomportApp "
            "top-level window - this view requires one." );
    return nullptr; // unreachable: qFatal() aborts
  }

  return theApp->getDocument();
}

void KomportView::print(QPrinter *pPrinter)
{
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
  paint.drawText( bounds, Qt::TextExpandTabs | Qt::AlignLeft | Qt::AlignTop, str );
  // eject the page...
  paint.end();
}

/** return cell array object */
KomportCellArray* KomportView::cellArray(){
  return &mCellArray;
}

/** draw a cell */
void KomportView::paintCell( QPainter* _paint, int _x, int _y, QRect _bounds ) {
  KomportCell* cell = getCell(_x,_y);
  if ( cell != nullptr ) {
    QString str( cell->character() );
    bool cursorCellOn = (cellArray()->cursor() == QPoint( _x, _y ) && mCursorState && cellArray()->cursorVisible());
    QColor fillColor;
    QColor textColor;
    QColor cellBackground = cell->backgroundColor();
    QColor cellForeground = cell->foregroundColor();

    // determine forground and background color...
    if ( cell->select() ) {
        fillColor =  QApplication::palette().color(QPalette::Highlight);
        textColor = QApplication::palette().color(QPalette::HighlightedText);
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
  // mPixmap only gets its actual size in resizeEvent() (see there) - before
  // this widget has ever been resized (e.g. profile-loading during
  // KomportApp's constructor, well before show()), it's still a
  // default-constructed null QPixmap. Painting into a null QPixmap is a
  // silent no-op as far as pixels go, but QPainter still complains loudly
  // ("Paint device returned engine == 0") - and there's nothing meaningful
  // to draw into yet anyway. Once resizeEvent() does give mPixmap a real
  // size, it finishes with cellArray()->update(), which re-triggers this
  // for every cell - so nothing is lost by skipping here, only redundant
  // work and warning spam.
  if ( mPixmap.isNull() ) return;
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
    if ( mScrollBar->value() != mScrollBar->maximum() ) {
        resetScroll();
        slotScroll( mScrollBar->value() );
    }
    emit keyPressed(_e);
}

/** key release event */
void KomportView::keyReleaseEvent(QKeyEvent* _e){
  Q_UNUSED(_e);
}

/** paint event */
void KomportView::paintEvent(QPaintEvent* _e){
  QRect rect = _e->rect();
  QPainter p(this);
  // Fill first - WA_OpaquePaintEvent means Qt trusts this to paint
  // everything itself, and height() is rarely an exact multiple of
  // cellHeight() now that height tracks the window, so there's usually a
  // thin margin below the last row that mPixmap doesn't cover.
  p.fillRect(rect, cellArray()->defaultBackgroundColor());
  // Clip to mPixmap's own bounds (the character-grid area) - the minimap
  // to the right is a proper child widget and paints itself; this must
  // not draw over/under it with stale pixmap content.
  QRect gridRect = rect.intersected( QRect(QPoint(0,0), mPixmap.size()) );
  if ( !gridRect.isEmpty() ) {
    p.drawPixmap(gridRect.topLeft(), mPixmap, gridRect);
  }
}

/** timer event */
void KomportView::timerEvent(QTimerEvent* _e) {
  if ( _e->timerId() == mBlinkTimer ) {
    mBlinkState = !mBlinkState;
    // arrayWidth() (column count), not cellWidth() (a single glyph's pixel
    // width, e.g. 5-10) - the latter made this loop only ever re-check
    // blink state for the first handful of columns instead of the whole
    // screen width.
    int w = cellArray()->arrayWidth();
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
        else if ( mMousePos.y() > height() && mScrollBar->value() < mScrollBar->maximum() ) { // scroll up (out of scroll buffer)...
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
  if ( mScrollBar->value() < mScrollBar->maximum() ) {
        int scrolled = mScrollBar->maximum() -  mScrollBar->value();
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

/** total rows across scrollback + the live screen */
int KomportView::totalHistoryRows() {
  return mScrollBuffer.depth() + cellArray()->arrayHeight();
}

/** cell at absolute row _row (0 = oldest scrollback row) across the
 *  combined scrollback+live history. Same indexing scheme as
 *  slotAboutToScrollUp()/getCell() use internally, just generalized to an
 *  absolute row instead of one relative to the current scroll position. */
KomportCell* KomportView::cellAtHistoryRow(int _col, int _row) {
  int depth = mScrollBuffer.depth();
  int total = depth + cellArray()->arrayHeight();
  if ( _row < 0 || _row >= total ) return nullptr;
  if ( _row < depth ) {
    return mScrollBuffer.cell( _col, (mScrollBuffer.arrayHeight() - depth) + _row );
  }
  return cellArray()->cell( _col, _row - depth );
}

/** calculates cell size based on font. */
void KomportView::setCellSize(){
  QFontMetrics fm = fontMetrics();
  cellArray()->setCellSize(QSize(fm.horizontalAdvance(QChar('H')),fm.height()));

  // Width is locked to the configured column count (+ the minimap strip) -
  // a classic fixed-width terminal, kept that way on purpose. Height is
  // NOT locked: only a modest minimum (so the grid can't be shrunk to
  // nothing) and no maximum, so the window can be resized taller/shorter
  // freely - resizeEvent() adjusts the actual row count (resizeGridRows())
  // to match whatever height the window ends up with.
  int fixedWidth = cellArray()->width() + mScrollBar->width();
  int minRows = qMin(cellArray()->arrayHeight(), 3);
  setMinimumSize( fixedWidth, minRows * cellArray()->cellHeight() );
  setMaximumSize( fixedWidth, QWIDGETSIZE_MAX );
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
    Q_UNUSED(_ch);
    if ( mScrollBar->value() != mScrollBar->maximum() ) {
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
  Q_UNUSED(_e);

  // Height (not width - width stays fixed at the configured column count,
  // a classic fixed-width terminal, by design) tracks the available
  // window space: recompute how many rows fit and grow/shrink the live
  // grid to match.
  int cellHeight = cellArray()->cellHeight();
  if ( cellHeight > 0 ) {
    int newRows = qMax(1, height() / cellHeight);
    resizeGridRows(newRows);
  }

  // mScrollBar is a proper child of this widget now, so its position only
  // ever needs to be expressed relative to `this` - correct regardless of
  // how this view itself is embedded (e.g. inside the central QSplitter).
  int mmWidth = mScrollBar->width();
  mScrollBar->resize( mmWidth, height() );
  mScrollBar->move( width() - mmWidth, 0 );
  mScrollBar->show();

  // QPixmap has no in-place resize() in Qt6 - build a new one and copy the
  // previous contents into its top-left corner, matching what Qt3's
  // QPixmap::resize() did. Sized to the character-grid's own pixel size
  // (arrayHeight() * cellHeight()), which is now current after
  // resizeGridRows() above - not the raw widget height, which is rarely
  // an exact multiple of cellHeight() (paintEvent() fills that margin).
  QSize gridSize( qMax(0, width() - mmWidth), cellArray()->height() );
  QPixmap resized( gridSize );
  resized.fill(cellArray()->defaultBackgroundColor());
  QPainter p(&resized);
  p.drawPixmap(0, 0, mPixmap);
  p.end();
  mPixmap = resized;
  cellArray()->update();
}

/** grow/shrink the live grid to _newRows, keeping the column count fixed */
void KomportView::resizeGridRows(int _newRows){
  int oldRows = cellArray()->arrayHeight();
  if ( _newRows == oldRows || _newRows < 1 ) return;
  int w = cellArray()->arrayWidth();
  QPoint cur = cellArray()->cursor();

  if ( _newRows < oldRows ) {
    // Shrinking: push the rows that no longer fit off the top into the
    // scrollback buffer first, one at a time - exactly what a normal
    // line-feed-driven scroll does in slotAboutToScrollUp(), just without
    // also re-growing the live grid back afterward.
    int removed = oldRows - _newRows;
    for ( int n = 0; n < removed; ++n ) {
      if ( mScrollBuffer.arrayHeight() > 0 ) {
        for ( int x = 0; x < w; ++x ) {
          mScrollBuffer.cell( x, mScrollBuffer.arrayHeight()-1 )->copy( cellArray()->cell(x, n) );
        }
        mScrollBuffer.scrollUp();
      }
    }
    cur.setY( qBound(0, cur.y() - removed, _newRows - 1) );
  } else {
    cur.setY( qBound(0, cur.y(), _newRows - 1) );
  }

  cellArray()->setArraySize( QSize(w, _newRows) );
  cellArray()->setCursor(cur);
  resetScroll(); // scrollback depth may have changed
}
/** notification that the cell array has scrolled up so we need to scroll visually */
void KomportView::slotScrolledUp(){
    // see the comment in updateCell() - same null-mPixmap guard, needed
    // here since a line feed can arrive (and scroll the cell array) before
    // this widget's first resizeEvent() has ever given mPixmap a size.
    if ( mPixmap.isNull() ) return;
    int rowHeight = cellArray()->cellHeight();
    // scroll the offscreen pixels up one row: copy the pixmap's lower
    // portion into a temporary buffer first, since painting a QPixmap onto
    // itself with overlapping source/target regions is not supported.
    QPixmap copy = mPixmap.copy(0, rowHeight, mPixmap.width(), mPixmap.height()-rowHeight);
    QPainter p(&mPixmap);
    p.drawPixmap(0, 0, copy);
    p.end();
    // transfer the offscreen pixels to the screen....
    update();
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
        selectStart( _e->position().toPoint() );
        mInSelection = true;
    }
}
/** No descriptions */
void KomportView::mouseReleaseEvent( QMouseEvent* _e ){
    if( _e->button() == Qt::LeftButton && mInSelection ) {
        selectEnd( _e->position().toPoint() );
        select( mSelectStart, mSelectEnd, true );
        mSelectStart = QPoint( 0, 0 );
        mSelectEnd = mMousePos = mSelectStart;
        mInSelection = false;
    }
}
/** No descriptions */
void KomportView::mouseMoveEvent( QMouseEvent* _e ){
     if ( mInSelection ) {
         selectEnd( (mMousePos = _e->position().toPoint()) );
         select( mSelectStart, mSelectEnd );
     }
}
/** mouse wheel scrolls the scrollback, same as dragging the minimap */
void KomportView::wheelEvent( QWheelEvent* _e ){
    // angleDelta() isn't always a full +-120 "notch" per event - many mice
    // and touchpads (especially with smooth-scrolling drivers, e.g.
    // libinput on Linux) split a single notch across several events with
    // small deltas. Truncating each one individually via a plain /120
    // rounds most of them down to zero, so nothing moves unless the user
    // scrolls hard enough to produce one big burst in a single event.
    // Accumulate instead, and only step once a full notch's worth has
    // built up - mAccumWheelDelta carries the remainder to the next event.
    mAccumWheelDelta += _e->angleDelta().y();
    int steps = mAccumWheelDelta / 120; // 120 = one notch
    if ( steps != 0 ) {
        mAccumWheelDelta -= steps * 120;
        mScrollBar->setValue( mScrollBar->value() + steps * 3 ); // emits valueChanged() itself, which drives slotScroll()
    }
    _e->accept();
}
/** make a selection by cell coordinates */
void KomportView::select(QPoint start, QPoint end, bool clip){
    deselect();
    if ( start != end ) {
      int startX = min(start.x(),end.x());
      int startY = min(start.y(),end.y());
      int endX = max(start.x(),end.x());
      int endY = max(start.y(),end.y());
      KomportCell* cell=nullptr;
      QString str;
      for( int y=startY; y <= endY; y++ ) {
          for( int x=startX; x <= endX; x++ ) {
              cell = cellArray()->cell(x,y);
              if ( cell != nullptr ) {
                  cellArray()->cell(x,y)->setSelect( true );
                  cellArray()->updateCell(x,y);
                  if ( clip )
                      str += cell->character();
              }
          }
          if ( clip && cell != nullptr ) {
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
    if ( mScrollBar->value() != mScrollBar->maximum() ) {
        resetScroll();
        slotScroll( mScrollBar->value() );
    }
    emit simKeyPressed(_c);
}
/** scroll bar moved */
void KomportView::slotScroll(int _value){
    Q_UNUSED(_value);
    // see the comment in updateCell() - same null-mPixmap guard, needed
    // here too since this can be reached via resetScroll()'s
    // setValue()/valueChanged() before the first resizeEvent() ever runs
    // (e.g. applyConnectionSettings() -> setScrollBuffer() -> resetScroll()
    // during profile loading in KomportApp's constructor).
    if ( mPixmap.isNull() ) return;
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
    update();
}
/** reset scrollbar */
void KomportView::resetScroll(){
        mScrollBar->setMaximum(mScrollBuffer.depth());
        mScrollBar->setValue(mScrollBuffer.depth());
}
/** set the number of lines in the scroll buffer */
void KomportView::setScrollBuffer(int _sz){
  mScrollBuffer.setArraySize( QSize( cellArray()->arrayWidth(), _sz ) );
  resetScroll();
}
