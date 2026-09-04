/***************************************************************************
                          komportscrollbuffer.h  -  Komport Serial Port Communicator
                             -------------------
    begin                : Thu Sep 25 2003
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

#ifndef KOMPORTSCROLLBUFFER_H
#define KOMPORTSCROLLBUFFER_H

#include "komportcellarray.h"

/**Impliments a scroll buffer
  *@author Mike Sharkey
  */

class KomportScrollBuffer : public KomportCellArray  {
public:
	KomportScrollBuffer();
	~KomportScrollBuffer() override;
  /** scroll up one line */
  void scrollUp() override;
  /** scroll depth in lines */
  virtual int depth() { return mDepth; }
  /** set the cell array size .
 */
  void setArraySize(QSize _sz) override;
protected:
    /** scroll depth in lines */
    int mDepth;
};

#endif
