/***************************************************************************
                          komportfilescrollbuffer.cpp  -  Komport Serial Port Communicator
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

#include "komportfilescrollbuffer.h"

KomportFileScrollBuffer::KomportFileScrollBuffer(QString _name)
{
    setName(_name);
}
KomportFileScrollBuffer::~KomportFileScrollBuffer(){
}
/** get a pointer to the cell from location (x,y) */
KomportCell* KomportFileScrollBuffer::cell(int _x,int _y){
  return NULL;
}

/** get a pointer to the cell from location (x,y) */
KomportCell* KomportFileScrollBuffer::cell(QPoint _p){
  return cell(_p.x(),_p.y());
}

/** set file name */
void KomportFileScrollBuffer::setName(QString _name){
    mFile.setName(_name);
}
/** scroll up */
void KomportFileScrollBuffer::scrollUp(){
    if ( mDepth < arrayHeight() ) ++mDepth;
}
/** set the cell array size */
void KomportFileScrollBuffer::setArraySize(QSize _sz){
    mArraySize = _sz;
    if ( mDepth > arrayHeight() ) mDepth = arrayHeight();
}
