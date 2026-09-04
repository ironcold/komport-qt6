/***************************************************************************
                          komportcellarray.h  -  Komport Serial Port Communicator
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

#ifndef KOMPORTCELLARRAY_H
#define KOMPORTCELLARRAY_H

#include <qobject.h>
#include <qptrlist.h>
#include <qstring.h>
#include <qpoint.h>

#include "komportcell.h"

/**Encapsulates the array of character cells.
  *@author Mike Sharkey
  */

class KomportCellArray : public QObject  {
Q_OBJECT
public:
  KomportCellArray();
	~KomportCellArray();

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

  /** set blink property */
  inline void setBlink(bool _b)       {mBlink=_b;}
  /** set bold property */
  inline void setBold(bool _b)        {mBold=_b;}
  /** set reverse property */
  inline void setReverse(bool _b)     {mReverse = _b;}
  /** set underline property */
  inline void setUnderline(bool _b)   {mUnderline = _b;}
  /** set forground color */
  inline void setForegroundColor(QColor _c) {mForegroundColor = _c;}
  /** set background color */
  inline void setBackgroundColor(QColor _c) {mBackgroundColor = _c;}

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
  /** Insert a character at the current cursor position and advance cursor. Scroll if advance is passed the last column. */
  void putChar(QChar _ch);
  /** move the cursor once cell forward, scolling or wrapping as nessesary */
  void advanceCursor();
  /** retrieve a character from a cell */
  QChar getChar(QPoint _p);
  /** scroll up one line */
  virtual void scrollUp();
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
  /** default background color */
  QColor defaultBackgroundColor();
  /** default foreground color */
  QColor defaultForegroundColor();
  /** set cell attributes */
  void setCellAttributes(QPoint _p);
  /** clear to end of line */
  void clearEOL();
  /** clear cell */
  void clear(QPoint _p);
  /** clear cell */
  void clear(int _x, int _y);
private: // Private attributes
  /** cell list */
  QPtrList<KomportCell> mCells;
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
  /** if true, signals are emitted when a cell is changed, otherwise not */
  bool mNotify;
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
protected: // Protected methods
  /** copy a row  */
  void copyRow(int _dst, int _src);
signals: // Signals
  /** notify anyone who cares that the array has scrolled up by one row */
  void scrolledUp();
signals: // Signals
  /** prepare for scrolling */
  void aboutToScrollUp();
};

#endif
