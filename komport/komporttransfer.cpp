/***************************************************************************
                          komporttransfer.cpp  -  Komport Serial Port Communicator
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

#include "komporttransfer.h"

#include <QApplication>
#include <QProgressDialog>
#include <QThread>

KomportTransfer::KomportTransfer(KomportSerial *_serial,QWidget *_parent)
: mSerial(_serial)
, mParent(_parent)
{
}
KomportTransfer::~KomportTransfer(){
    mFile.close();
}
/** set the local file used for the transfer */
bool KomportTransfer::setFileName(const QString &_fileName){
  mFile.setFileName( _fileName );
  return true;
}
/** run the upload file transfer */
bool KomportTransfer::upload(){
    mFile.open(QIODevice::ReadOnly);
    qint64 size = mFile.size();
    qint64 sent=0;
    QProgressDialog progress( tr("Upload Progress"), tr("Cancel"), 0, static_cast<int>(size), mParent );
    progress.setWindowTitle( tr("Upload Progress") );
    progress.setWindowModality( Qt::WindowModal );
    progress.show();
    progress.raise();
    char ch=0;
    while( mFile.getChar(&ch) && !progress.wasCanceled() ) {
       mSerial->putChar(ch);
       sent++;
       progress.setValue( static_cast<int>(sent) );
       QCoreApplication::processEvents( QEventLoop::AllEvents );
       if ( ch == '\n' ) {
           QThread::msleep( 250 );
       }
    }
    mFile.close();
    return true;
}
/** run the download file transfer */
bool KomportTransfer::download(){
    mFile.open(QIODevice::WriteOnly);
    QObject::connect(mSerial,SIGNAL(receivedChar(char)),this,SLOT(slotReceivedChar(char)));
    qint64 received=0;
    QProgressDialog progress( tr("Download Progress"), tr("Cancel"), 0, 0, mParent );
    progress.setWindowTitle( tr("Download Progress") );
    progress.setWindowModality( Qt::WindowModal );
    progress.show();
    progress.raise();
    while( mFile.isOpen() && !progress.wasCanceled() ) {
        received = mFile.size();
        progress.setMaximum( static_cast<int>(received+1) );
        progress.setValue( static_cast<int>(received) );
        QCoreApplication::processEvents( QEventLoop::AllEvents );
    }
    mFile.close();
    QObject::disconnect(mSerial,SIGNAL(receivedChar(char)),this,SLOT(slotReceivedChar(char)));
    return true;
}
/** No descriptions */
void KomportTransfer::slotReceivedChar(char _ch){
    if ( mFile.isOpen() )  {
        if ( _ch == ('D'-0x40) )  { // end of text (Ctrl-D)
            mFile.close();
        } else {
            mFile.putChar( _ch );
        }
    }
}
