/***************************************************************************
                          komporttransfer.cpp  -  Komport Serial Port Communicator
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

#include "komporttransfer.h"

#include <qapplication.h>
#include <qeventloop.h>
#include <unistd.h>

KomportTransfer::KomportTransfer(KomportSerial *_serial,QWidget *_parent)
: mSerial(_serial)
, mParent(_parent)
{
}
KomportTransfer::~KomportTransfer(){
    mFile.close();
   KIO::NetAccess::removeTempFile( mFileName );  
}
/** provide a file dialog for selecting a local file */
bool KomportTransfer::setURL(KURL _url){
  KIO::NetAccess::download( _url, mFileName );
  mFile.setName( mFileName );
  return true;
}
/** run the upload file transfer */
bool KomportTransfer::upload(){
    mFile.open(IO_ReadOnly);
    long int size = mFile.size();
    long int sent=0;
    int ch=0;
    QEventLoop* eventLoop = QApplication::eventLoop();
    KProgressDialog progress( mParent, "upload_progress", i18n("Upload Progress"), i18n("Upload Progress"), true );
    progress.progressBar()->setTotalSteps( size );
    progress.show();
    progress.raise();
    while( (ch = mFile.getch()) != -1 && !progress.wasCancelled() ) {
       mSerial->putChar(ch);
       sent++;
       progress.progressBar()->setProgress( sent );
       eventLoop->processEvents( QEventLoop::AllEvents );
       if ( ch == '\n' ) {
           usleep( 1000*250 );
       }
    }
    mFile.close();
    return true;
}
/** run the download file transfer */
bool KomportTransfer::download(){
    mFile.open(IO_WriteOnly);
    QObject::connect(mSerial,SIGNAL(receivedChar(char)),this,SLOT(slotReceivedChar(unsigned char)));
    long int received=0;
    QEventLoop* eventLoop = QApplication::eventLoop();
    KProgressDialog progress(mParent, "download_progress", i18n("Download Progress"), i18n("Download Progress"), true );
    progress.show();
    progress.raise();
    while( mFile.isOpen() && !progress.wasCancelled() ) {
        received = mFile.size();   
        progress.progressBar()->setTotalSteps( received+1 );
        progress.progressBar()->setProgress( received );
        eventLoop->processEvents( QEventLoop::AllEvents );
    }
    mFile.close();
    return true;
}
/** No descriptions */
void KomportTransfer::slotReceivedChar(unsigned char _ch){
    if ( mFile.isOpen() )  {
        if ( _ch == ('D'-0x40) )  { // end of text
            mFile.close();
        } else {
            mFile.putch( _ch );
        }
    }
}
