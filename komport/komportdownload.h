/***************************************************************************
                          komportdownload.h  -  Komport Serial Port Communicator
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

#ifndef KOMPORTDOWNLOAD_H
#define KOMPORTDOWNLOAD_H

#include "komporttransfer.h"

/**baseclass for file download
  *
  * NOTE: see the comment in komportupload.h - same situation (unused,
  * previously non-compiling stub; ported and made buildable for
  * completeness only).
  *
  *@author Mike Sharkey
  */

class KomportDownload : public KomportTransfer  {
public:
	KomportDownload(KomportSerial* _serial, QWidget* _parent = nullptr);
	~KomportDownload() override;
};

#endif
