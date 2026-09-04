/***************************************************************************
                          komportscrollbuffer.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : Thu Sep 25 2003
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

#include "komportscrollbuffer.h"

#define inherited KomportCellArray

KomportScrollBuffer::KomportScrollBuffer()
: mDepth(0)
{
}
KomportScrollBuffer::~KomportScrollBuffer(){
}

/** scroll up */
void KomportScrollBuffer::scrollUp(){
    if ( mDepth < arrayHeight() ) ++mDepth;
    inherited::scrollUp();
}
   /** set the cell array size .
 */
void KomportScrollBuffer::setArraySize(QSize _sz){
    inherited::setArraySize(_sz);
    if ( mDepth > arrayHeight() ) mDepth = arrayHeight();
}
