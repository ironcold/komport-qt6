/***************************************************************************
                          komportupload.h  -  Komport Serial Port Communicator
                             -------------------
    begin                : Tue Oct 7 2003
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

#ifndef KOMPORTUPLOAD_H
#define KOMPORTUPLOAD_H

#include "komporttransfer.h"

/**baseclass for upload file
  *
  * NOTE: this class was never actually part of the KDE3-era build (missing
  * from komport/Makefile.am's SOURCES) and, as originally written, could not
  * even compile - its default constructor tried to default-construct
  * KomportTransfer, which has no default constructor. Kept here ported and
  * made buildable for completeness, but komport.cpp still talks to
  * KomportTransfer directly, exactly as it always has.
  *
  *@author Mike Sharkey
  */

class KomportUpload : public KomportTransfer  {
public:
	KomportUpload(KomportSerial* _serial, QWidget* _parent = nullptr);
	~KomportUpload() override;
};

#endif
