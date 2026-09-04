/***************************************************************************
                          komportfilescrollbuffer.h  -  Komport Serial Port Communicator
                             -------------------
    begin                : Wed Oct 8 2003
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

#ifndef KOMPORTFILESCROLLBUFFER_H
#define KOMPORTFILESCROLLBUFFER_H

#include <komportscrollbuffer.h>

#include <qfile.h>

/**file base scroll buffer implimentation
  *@author Mike Sharkey
  */

class KomportFileScrollBuffer : public KomportScrollBuffer  {
public: 
	KomportFileScrollBuffer(QString _name);
	~KomportFileScrollBuffer();
  /** get a pointer to the cell from location (x,y) */
  virtual KomportCell* cell(int _x,int _y);
  /** get a pointer to the cell from location (x,y) */
  virtual KomportCell* cell(QPoint _p);
  /** set file name */
  virtual void setName(QString _name);
  /** scroll up one line */
  virtual void scrollUp();
  /** scroll depth in lines */
  virtual int depth() { return mDepth; }
  /** set the cell array size .
 */
  virtual void setArraySize(QSize _sz);
private: // Private attributes
  /** file object */
  QFile mFile;
};

#endif
