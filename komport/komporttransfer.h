/***************************************************************************
                          komporttransfer.h  -  Komport Serial Port Communicator
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

#ifndef KOMPORTTRANSFER_H
#define KOMPORTTRANSFER_H

#include <QObject>
#include <QWidget>
#include <QString>
#include <QFile>

#include "komportserial.h"

/**baseclass for file transfer objects
  *
  * Ported from KURL/KFileDialog/KIO::NetAccess to a plain local file path -
  * file selection now goes through QFileDialog, which already hands back a
  * local path, so the KIO remote-download indirection the KDE3 version used
  * is no longer needed.
  *
  *@author Mike Sharkey
  */

class KomportTransfer : public QObject  {
Q_OBJECT
public:
	KomportTransfer(KomportSerial* _serial,QWidget* parent=nullptr);
	~KomportTransfer() override;
  /** set the local file used for the transfer */
  virtual bool setFileName(const QString &_fileName);
  /** run the upload file transfer */
  virtual bool upload();
  /** run the download file transfer */
  virtual bool download();
protected: // Private attributes
  /** local file */
  QFile mFile;
  /**  */
  KomportSerial* mSerial;
  /** */
  QWidget* mParent;
  /** set by slotReceivedChar() if mFile.putChar() fails mid-download (e.g.
   *  the disk fills up) - checked by download() once its polling loop
   *  exits, so a failed write is reported instead of silently continuing
   *  to discard bytes for the rest of the transfer. */
  bool mDownloadWriteError = false;
public slots: // Public slots
  /** No descriptions.
   *  NOTE: takes the same 'char' type as KomportSerial::receivedChar(char) -
   *  the original connected this to a mismatched 'unsigned char' overload,
   *  which Qt's signal/slot type matching would in fact have silently
   *  rejected at runtime (download() never actually received anything).
   *  Matching the type here is what makes the connection - and thus
   *  download() - actually work. */
  void slotReceivedChar(char _ch);
};

#endif
