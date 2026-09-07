/***************************************************************************
                          komportscrollbuffer.cpp  -  Komport Serial Port Communicator
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
    // Capped at arrayHeight()-1, not arrayHeight(): KomportView::
    // slotAboutToScrollUp() writes each newly-scrolled-off row into this
    // array's *last* row, then immediately calls this scrollUp(), whose
    // inherited::scrollUp() (below) shifts every row - including the one
    // just written - up by one and appends a fresh *blank* row at the
    // end. That means the last row is always blank again right after
    // this call returns, forever, no matter how long the session runs -
    // there are only ever arrayHeight()-1 rows with real content, one
    // permanently blank row is baked into how this gets filled.
    //
    // In practice this particular cap constant can't be observed via
    // continuous scrolling alone: inherited::scrollUp() (below) shrinks
    // the array by one row and immediately grows it back by one, and
    // both of those setArraySize() calls dispatch virtually back into
    // setArraySize() below, which re-clamps mDepth against arrayHeight()
    // twice per call - so mDepth is already pinned at arrayHeight()-1
    // long before this line would ever matter, regardless of whether the
    // constant here says arrayHeight() or arrayHeight()-1. The
    // reachable, user-facing trigger is setArraySize() below being called
    // *directly* - i.e. a profile switch or Settings change that resizes
    // the scroll buffer - see the comment there. This cap is kept in
    // sync with that one anyway: it's the correct invariant on its own
    // terms, and disagreeing with setArraySize()'s cap here would just
    // be a second, needless way for the two to drift apart.
    if ( mDepth < arrayHeight()-1 ) ++mDepth;
    inherited::scrollUp();
}
   /** set the cell array size .
 */
void KomportScrollBuffer::setArraySize(QSize _sz){
    inherited::setArraySize(_sz);
    // Capped at arrayHeight()-1, not arrayHeight() - see scrollUp()
    // above for why that's the correct ceiling. This is the call site
    // that actually matters: a profile switch or Settings change
    // resizes the scroll buffer via a single, *un-paired* setArraySize()
    // call - e.g. switching to a profile whose configured ScrollBuffer
    // size happens to equal the current depth(). The old "> arrayHeight()"
    // clamp left mDepth unchanged in exactly that case (mDepth already
    // equalled the new arrayHeight(), so the ">" test was false),
    // landing depth() == arrayHeight() exactly - the one state
    // getCell()'s and cellAtHistoryRow()'s scroll-buffer index
    // arithmetic (both keyed off depth()) can't index into: it computed
    // an out-of-range negative index for the oldest row, silently
    // rendering it as missing/blank instead of the real content that was
    // actually still sitting right there.
    if ( mDepth > arrayHeight()-1 ) mDepth = qMax(0, arrayHeight()-1);
}
