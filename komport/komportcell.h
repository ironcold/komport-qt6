/***************************************************************************
                          komportcell.h  -  Komport Serial Port Communicator
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

#ifndef KOMPORTCELL_H
#define KOMPORTCELL_H

/**Encapsulates a character cell.
  *@author Mike Sharkey
  */

#include <QChar>
#include <QColor>

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
  /** is the foreground color currently "the default" (never explicitly
   *  colored via SGR since the last clear/reset, as opposed to explicitly
   *  colored via SGR 30-37/90-97/256-color)? Milestone 5 (Codex review
   *  finding): color-scheme live-recoloring can't infer this from color
   *  equality alone - a scheme's own default can legitimately be the same
   *  QColor a real SGR code produces (e.g. "Green on Black"'s default
   *  background is the same black SGR 40 produces), so comparing "is this
   *  cell's color equal to the old default" would wrongly recolor an
   *  explicitly-black-via-SGR-40 cell right along with genuinely
   *  unset ones. This flag tracks provenance explicitly instead. */
  inline bool foregroundIsDefault()   {return mForegroundIsDefault;}
  /** see foregroundIsDefault() above */
  inline bool backgroundIsDefault()   {return mBackgroundIsDefault;}

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
  /** set forground color. _isDefault marks whether this sets the cell back
   *  to "the default" (SGR 0/39, clear()) as opposed to an explicit SGR
   *  color (30-37/90-97/256-color, the default here) - see
   *  foregroundIsDefault() above. */
  inline void setForegroundColor(QColor _c, bool _isDefault = false) {mForegroundColor = _c; mForegroundIsDefault = _isDefault;}
  /** set background color - see setForegroundColor() above */
  inline void setBackgroundColor(QColor _c, bool _isDefault = false) {mBackgroundColor = _c; mBackgroundIsDefault = _isDefault;}

  /** reset properties to default values, using _fg/_bg as the "no color
   *  set" colors. No default arguments on purpose (Milestone 5, see
   *  komportcellarray.cpp): the colors a cleared cell gets are
   *  configurable per profile now, and KomportCell has no back-reference
   *  to the KomportCellArray that owns it and knows the current
   *  configured defaults - every caller must pass them explicitly
   *  (typically cellArray()->defaultForegroundColor()/
   *  defaultBackgroundColor()) rather than this class silently reaching
   *  for a hardcoded fallback that could go stale the moment the user
   *  picks a color scheme. */
  void clear(const QColor &_fg, const QColor &_bg);
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
  /** see foregroundIsDefault()/backgroundIsDefault() above */
  bool mForegroundIsDefault;
  bool mBackgroundIsDefault;
};

#endif
