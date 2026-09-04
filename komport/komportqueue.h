/***************************************************************************
                          komportqueue.h  -  Komport Serial Port Communicator
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

#ifndef KOMPORTQUEUE_H
#define KOMPORTQUEUE_H

#include <qobject.h>

/**Impliments a round queue.
  *@author Mike Sharkey
  */

class KomportQueue : public QObject  {
  Q_OBJECT
public: 
	KomportQueue();
	~KomportQueue();
  /** queue size */
  int size();
  /** get next character */
  int get();
  /** put a character */
  int put(int _ch);
  /** resize the queue */
  void setSize(int _i);
  /** is the queue empty? */
  bool empty() {return mHead==mTail;}
private: // Private attributes
  /** data */
  unsigned char*  mData;
  /** head pointer */
  int mHead;
  /** size of the queue */
  int mSize;
  /** tail pointer */
  int mTail;
signals: // Signals
  /** emited if trhe head and tail pointer overlap. */
  void queueOverflow();
};

#endif
