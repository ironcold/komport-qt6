/***************************************************************************
                          komportcell.h  -  Komport Serial Port Communicator
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

#ifndef KOMPORTCELL_H
#define KOMPORTCELL_H


/**Encapsulates a character cell.
  *@author Mike Sharkey
  */

#include <qapplication.h>
#include <qstring.h>
#include <qcolor.h>

class KomportCell {
public: 
	KomportCell();
	~KomportCell();

  /** retrieve select property */
  inline bool select()              {return mSelect;}
  /** retrieve blink property */
  inline bool blink()                 {return mBlink;}
  /** retrieve bold property */
  inline bool bold()                  {return mBold;}
  /** retrieve character property */
  inline QChar character()            {return mCharacter;}
  /** retrieve reverse property */
  inline bool reverse()               {return mReverse;}
  /** retrieve underline property */
  inline bool underline()             {return mUnderline;}
  /** get background color */
  inline QColor backgroundColor()     {return mBackgroundColor;}
  /** get foreground color */
  inline QColor foregroundColor()     {return mForegroundColor;}

  /** set select property*/
  inline void setSelect(bool _b)    {mSelect=_b;}
  /** set blink property */
  inline void setBlink(bool _b)       {mBlink=_b;}
  /** set bold property */
  inline void setBold(bool _b)        {mBold=_b;}
  /** set character property */
  inline void setCharacter(QChar _c)  {mCharacter = _c;}
  /** set reverse property */
  inline void setReverse(bool _b)     {mReverse = _b;}
  /** set underline property */
  inline void setUnderline(bool _b)   {mUnderline = _b;}
  /** set forground color */
  inline void setForegroundColor(QColor _c) {mForegroundColor = _c;}
  /** set background color */
  inline void setBackgroundColor(QColor _c) {mBackgroundColor = _c;}

  /** reset properties to default values */
  void clear();
  /** copy operator */
  KomportCell & operator=(const KomportCell & _other);
  /** copy a cell */
  void copy(KomportCell* _other);

private: // Private attributes
  /** select property */
  bool mSelect;
  /** blink property */
  bool mBlink;
  /** bold property */
  bool mBold;
  /** character property */
  QChar mCharacter;
  /** reverse video property */
  bool mReverse;
  /** underline property */
  bool mUnderline;
  /** forground color */
  QColor mForegroundColor;
  /** background color */
  QColor mBackgroundColor;
};

#endif
