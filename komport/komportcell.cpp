/***************************************************************************
                          komportcell.cpp  -  Komport Serial Port Communicator
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

#include "komportcell.h"

#define _DEFAULT_CHAR_ ' '
#define _DEFAULT_FOREGROUND_   QApplication::palette().active().foreground()
#define _DEFAULT_BACKGROUND_   QApplication::palette().active().background()

KomportCell::KomportCell()
: mSelect(false)
, mBlink(false)
, mBold(false)
, mCharacter(_DEFAULT_CHAR_)
, mReverse(false)
, mUnderline(false)
, mForegroundColor(_DEFAULT_FOREGROUND_)
, mBackgroundColor(_DEFAULT_BACKGROUND_ )
{
}

KomportCell::~KomportCell(){
}

/** reset properties to default values */
void KomportCell::clear(){
  setSelect(false);
  setBlink(false);
  setBold(false);
  setCharacter(_DEFAULT_CHAR_);
  setReverse(false);
  setUnderline(false);
  setForegroundColor(_DEFAULT_FOREGROUND_);
  setBackgroundColor(_DEFAULT_BACKGROUND_);
}

/** copy operator */
KomportCell & KomportCell::operator=(const KomportCell & _other){
  if ( this != &_other ) {
    mSelect          = _other.mSelect;
    mBlink            = _other.mBlink;
    mBold             = _other.mBold;
    mCharacter        = _other.mCharacter;
    mReverse          = _other.mReverse;
    mUnderline        = _other.mUnderline;
    mForegroundColor  = _other.mForegroundColor;
    mBackgroundColor  = _other.mBackgroundColor;
  }
  return *this;
}

/** copy a cell */
void KomportCell::copy(KomportCell* _other){
  setSelect(         _other->select() );
  setBlink(           _other->blink()      );
  setBold(            _other->bold()       );
  setCharacter(       _other->character()  );
  setReverse(         _other->reverse()    );
  setUnderline(       _other->underline()  );
  setForegroundColor( _other->foregroundColor() );
  setBackgroundColor( _other->backgroundColor() );
}

