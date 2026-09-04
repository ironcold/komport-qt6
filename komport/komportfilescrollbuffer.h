/***************************************************************************
                          komportfilescrollbuffer.h  -  Komport Serial Port Communicator
                             -------------------
    begin                : Wed Oct 8 2003
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

#ifndef KOMPORTFILESCROLLBUFFER_H
#define KOMPORTFILESCROLLBUFFER_H

#include "komportscrollbuffer.h"

#include <QFile>

/**file base scroll buffer implimentation
  *@author Mike Sharkey
  *
  * NOTE: as in the original KDE3 version, this implementation is a stub -
  * cell() always returns nullptr. File-backed scrollback was never finished
  * upstream; this is carried over unchanged by the Qt6 port, not a new
  * limitation.
  */

class KomportFileScrollBuffer : public KomportScrollBuffer  {
public:
	explicit KomportFileScrollBuffer(const QString &_name);
	~KomportFileScrollBuffer() override;
  /** get a pointer to the cell from location (x,y) */
  KomportCell* cell(int _x,int _y) override;
  /** get a pointer to the cell from location (x,y) */
  KomportCell* cell(QPoint _p) override;
  /** set file name */
  virtual void setName(const QString &_name);
  /** scroll up one line */
  void scrollUp() override;
  /** scroll depth in lines */
  int depth() override { return mDepth; }
  /** set the cell array size .
 */
  void setArraySize(QSize _sz) override;
private: // Private attributes
  /** file object */
  QFile mFile;
};

#endif
