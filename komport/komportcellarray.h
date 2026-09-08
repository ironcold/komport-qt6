/***************************************************************************
                          komportcellarray.h  -  Komport Serial Port Communicator
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

#ifndef KOMPORTCELLARRAY_H
#define KOMPORTCELLARRAY_H

#include <QObject>
#include <QList>
#include <QString>
#include <QPoint>
#include <QSize>
#include <QColor>

#include "komportcell.h"

/**Encapsulates the array of character cells.
  *@author Mike Sharkey
  */

class KomportCellArray : public QObject  {
Q_OBJECT
public:
  KomportCellArray();
	~KomportCellArray() override;

  /** retrieve blink property */
  inline bool blink()                 {return mBlink;}
  /** retrieve bold property */
  inline bool bold()                  {return mBold;}
  /** retrieve reverse property */
  inline bool reverse()               {return mReverse;}
  /** retrieve underline property */
  inline bool underline()             {return mUnderline;}
  /** get background color */
  inline QColor backgroundColor()     {return mBackgroundColor;}
  /** get foreground color */
  inline QColor foregroundColor()     {return mForegroundColor;}
  /** is the *current* (next-character-drawn) foreground color still "the
   *  default" - see KomportCell::foregroundIsDefault() and
   *  setDefaultForegroundColor() below for why this is tracked explicitly
   *  rather than inferred from color equality (Milestone 5, Codex review). */
  inline bool foregroundIsDefault()   {return mForegroundIsDefault;}
  /** see foregroundIsDefault() above */
  inline bool backgroundIsDefault()   {return mBackgroundIsDefault;}

  /** set blink property */
  inline void setBlink(bool _b)       {mBlink=_b;}
  /** set bold property */
  inline void setBold(bool _b)        {mBold=_b;}
  /** set reverse property */
  inline void setReverse(bool _b)     {mReverse = _b;}
  /** set underline property */
  inline void setUnderline(bool _b)   {mUnderline = _b;}
  /** set forground color - an *explicit* SGR color (30-37/90-97/256-color).
   *  Marks foregroundIsDefault() false - use resetForegroundToDefault()
   *  below for SGR 0/39 instead, which correctly marks it true. */
  inline void setForegroundColor(QColor _c) {mForegroundColor = _c; mForegroundIsDefault = false;}
  /** set background color - see setForegroundColor() above, and
   *  resetBackgroundToDefault() for SGR 0/49 */
  inline void setBackgroundColor(QColor _c) {mBackgroundColor = _c; mBackgroundIsDefault = false;}
  /** reset the *current* (next-character-drawn) foreground/background
   *  color back to the default (SGR 0/39, SGR 0/49) - unlike
   *  setForegroundColor()/setBackgroundColor(), correctly marks the
   *  result as foregroundIsDefault()/backgroundIsDefault() rather than
   *  "explicitly colored to happen to match the default". */
  void resetForegroundToDefault();
  void resetBackgroundToDefault();

  /** set the cell array size .
 */
  virtual void setArraySize(QSize _sz);
  /** set cell size */
  void setCellSize(QSize _sz);
  /** return calculated dimensions in pixels based on cell size and array size */
  QSize size();
  /** get cell size */
  QSize cellSize();
  /** get array size */
  QSize arraySize();
  /** return calculated heiht based on cell height * array height */
  int height();
  /** return calculated width based on cell width * array width */
  int width();
  /** cell width */
  int cellWidth();
  /** array height */
  int arrayHeight();
  /** array width  */
  int arrayWidth();
  /** cell heiht */
  int cellHeight();
  /** draw a character into a cell */
  void drawChar(QChar _c,int _x, int _y);
  /** draw a character into a cell */
  void drawChar(QChar _c,QPoint _p);
  /** get a pointer to the cell from location (x,y) */
  virtual KomportCell* cell(int _x,int _y);
  /** get a pointer to the cell from location (x,y) */
  virtual KomportCell* cell(QPoint _p);
  /** cursor position */
  void setCursor(int _x, int _y) {setCursor(QPoint(_x,_y));}
  /** cursor position */
  void setCursor(QPoint _p);
  /** cursor position */
  QPoint cursor();
  /** is the cursor supposed to be drawn at all (DECTCEM, ESC[?25h/l)? */
  inline bool cursorVisible() { return mCursorVisible; }
  /** show/hide the cursor (DECTCEM) */
  void setCursorVisible(bool _v);
  /** Insert a character at the current cursor position and advance cursor. Scroll if advance is passed the last column. */
  void putChar(QChar _ch);
  /** move the cursor once cell forward, scolling or wrapping as nessesary */
  void advanceCursor();
  /** retrieve a character from a cell */
  QChar getChar(QPoint _p);
  /** scroll up one line */
  virtual void scrollUp();
  /** scroll rows [_top,_bottom] (inclusive, 0-based) up by one line: row
   *  _top's content is discarded, row _bottom ends up blank, rows outside
   *  the range are untouched. Unlike scrollUp(), this does NOT feed the
   *  scrollback history - used for DECSTBM scroll-region scrolling, where
   *  a program has deliberately split the screen (e.g. a status line) and
   *  only the inner region should move. */
  void scrollUpRegion(int _top, int _bottom);
  /** scroll rows [_top,_bottom] (inclusive, 0-based) down by one line: row
   *  _bottom's content is discarded, row _top ends up blank. Used for
   *  reverse-index (ESC M) scrolling within (or across) a scroll region. */
  void scrollDownRegion(int _top, int _bottom);
  /** clear a row */
  void clearRow(int _row);
  /** update cell */
  void updateCell(int _x,int _y);
  /** update all cells */
  void update();
  /** update a row */
  void updateRow(int _row);
  /** update a cell */
  void updateCell(QPoint _c);
  /** clear all cells */
  void clear();
  /** default background color - what a cleared/reset cell gets, and what
   *  SGR 49 ("default background") resolves to. Historically a fixed read
   *  of QApplication::palette(); configurable per-profile since
   *  Milestone 5 via setDefaultBackgroundColor() below. */
  QColor defaultBackgroundColor();
  /** default foreground color - see defaultBackgroundColor() above */
  QColor defaultForegroundColor();
  /** change the default background color (Milestone 5: profile-configured
   *  color schemes). Live-applies: every cell currently showing the *old*
   *  default background is recolored to the new one immediately (a host
   *  that explicitly set some other background via SGR is left alone -
   *  this can't tell "explicitly colored to the same shade as the old
   *  default" apart from "never explicitly colored", but that's an
   *  acceptable, common approximation - see komportcellarray.cpp). */
  void setDefaultBackgroundColor(QColor _c);
  /** change the default foreground color - see setDefaultBackgroundColor() above */
  void setDefaultForegroundColor(QColor _c);
  /** set cell attributes */
  void setCellAttributes(QPoint _p);
  /** clear to end of line */
  void clearEOL();
  /** clear cell */
  void clear(QPoint _p);
  /** clear cell */
  void clear(int _x, int _y);
  /** copy the contents of row _src onto row _dst (including notifying
   *  rowChanged(_dst)) - used e.g. by KomportEmulation's insert/delete
   *  line handling to shift rows without duplicating the per-cell loop. */
  void copyRow(int _dst, int _src);
private: // Private attributes
  /** cell list (owning: array deletes the cells it holds) */
  QList<KomportCell*> mCells;
  /** size of cell in pixels*/
  QSize mCellSize;
  /** cusror position */
  QPoint mCursor;
  /** blink property */
  bool mBlink;
  /** bold property */
  bool mBold;
  /** reverse video property */
  bool mReverse;
  /** underline property */
  bool mUnderline;
  /** forground color */
  QColor mForegroundColor;
  /** background color */
  QColor mBackgroundColor;
  /** see foregroundIsDefault()/backgroundIsDefault() above */
  bool mForegroundIsDefault;
  bool mBackgroundIsDefault;
  /** if true, signals are emitted when a cell is changed, otherwise not */
  bool mNotify;
  /** default (SGR 39/49, cell-clear) colors - see defaultForegroundColor()/
   *  defaultBackgroundColor() above. Initialized from QApplication::
   *  palette() in the constructor, same as before Milestone 5; overridden
   *  by setDefaultForegroundColor()/setDefaultBackgroundColor() once a
   *  profile with its own color scheme is loaded. */
  QColor mDefaultForegroundColor;
  QColor mDefaultBackgroundColor;
  /** DECTCEM cursor-visible flag */
  bool mCursorVisible = true;
protected:
  /** size of cell array */
  QSize mArraySize;
protected slots: // Protected slots
  /** initialize default settings */
  void initSettings();
signals: // Signals
  /** emited when the cursor changes position */
  void cursorChanged(QPoint _old, QPoint _new);
  /** a cell's content has changed */
  void cellChanged(QPoint _p);
  /** signal a row has changed */
  void rowChanged(int _row);
signals: // Signals
  /** notify anyone who cares that the array has scrolled up by one row */
  void scrolledUp();
signals: // Signals
  /** prepare for scrolling */
  void aboutToScrollUp();
signals: // Signals
  /** the DECTCEM cursor-visible flag changed */
  void cursorVisibilityChanged(bool _visible);
};

#endif
