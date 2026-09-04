/***************************************************************************
                          komportqueue.cpp  -  Komport Serial Port Communicator
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

#include "komportqueue.h"

KomportQueue::KomportQueue()
: mData(NULL)
, mHead(0)
, mSize(1024)
, mTail(0)
{
    setSize(mSize);
}

KomportQueue::~KomportQueue(){
}

/** get next character */
int KomportQueue::get(){
  int rc=(-1);
  if ( mTail != mHead ) {
    rc = mData[ mTail++ ];
    if ( mTail >= size() )
      mTail=0;
  }
  return rc;
}

/** put a character */
int KomportQueue::put(int _ch)
{
  mData[ mHead++ ] = _ch;
  if ( mHead >= size() )
    mHead = 0;
  if ( mHead == mTail )
    emit queueOverflow();
  return _ch;
}

/** queue size */
int KomportQueue::size(){
  return mSize;
}
/** resize the queue */
void KomportQueue::setSize(int _i){
    mData = (unsigned char*)realloc(mData,_i);
    mSize=_i;
}
