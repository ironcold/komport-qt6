/***************************************************************************
                          komporttransfer.h  -  Komport Serial Port Communicator
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

#ifndef KOMPORTTRANSFER_H
#define KOMPORTTRANSFER_H

#include <qobject.h>
#include <qwidget.h>
#include <qstring.h>
#include <qfile.h>

#include <kurl.h>
#include <kfiledialog.h>
#include <klocale.h>
#include <kio/netaccess.h>
#include <kprogress.h>

#include "komportserial.h"

/**baseclass for file transfer objects
  *@author Mike Sharkey
  */

class KomportTransfer : public QObject  {
public: 
	KomportTransfer(KomportSerial* _serial,QWidget* parent=0);
	~KomportTransfer();
  /** provide a file dialog for selecting a local file */
  virtual bool setURL(KURL _url);
  /** run the upload file transfer */
  virtual bool upload();
  /** run the download file transfer */
  virtual bool download();
protected: // Private attributes
  /** local file */
  QFile mFile;
  /** local file name */
  QString mFileName;
  /**  */
  KomportSerial* mSerial;
  /** */
  QWidget* mParent;
public slots: // Public slots
  /** No descriptions */
  void slotReceivedChar(unsigned char _ch);
};

#endif
