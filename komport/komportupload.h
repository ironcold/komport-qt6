/***************************************************************************
                          komportupload.h  -  Komport Serial Port Communicator
                             -------------------
    begin                : Tue Oct 7 2003
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

#ifndef KOMPORTUPLOAD_H
#define KOMPORTUPLOAD_H

#include <komporttransfer.h>

/**baseclass for upload file
  *@author Mike Sharkey
  */

class KomportUpload : public KomportTransfer  {
public: 
	KomportUpload();
	~KomportUpload();
};

#endif
