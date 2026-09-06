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
#include <QMessageBox>
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
    if ( !mFile.open(QIODevice::ReadOnly) ) {
        QMessageBox::warning( mParent, tr("Upload"),
            tr("Could not open \"%1\" for reading:\n%2").arg(mFile.fileName(), mFile.errorString()) );
        return false;
    }
    qint64 size = mFile.size();
    qint64 sent=0;
    QProgressDialog progress( tr("Upload Progress"), tr("Cancel"), 0, static_cast<int>(size), mParent );
    progress.setWindowTitle( tr("Upload Progress") );
    progress.setWindowModality( Qt::WindowModal );
    progress.show();
    progress.raise();
    char ch=0;
    bool writeFailed = false;
    while( mFile.getChar(&ch) && !progress.wasCanceled() ) {
       if ( !mSerial->putChar(ch) ) {
           writeFailed = true;
           break;
       }
       sent++;
       progress.setValue( static_cast<int>(sent) );
       QCoreApplication::processEvents( QEventLoop::AllEvents );
       if ( ch == '\n' ) {
           QThread::msleep( 250 );
       }
    }
    mFile.close();
    if ( writeFailed ) {
        QMessageBox::warning( mParent, tr("Upload"),
            tr("Lost the connection to the serial port after sending %1 of %2 bytes.").arg(sent).arg(size) );
        return false;
    }
    return true;
}
/** run the download file transfer */
bool KomportTransfer::download(){
    if ( !mFile.open(QIODevice::WriteOnly) ) {
        QMessageBox::warning( mParent, tr("Download"),
            tr("Could not open \"%1\" for writing:\n%2").arg(mFile.fileName(), mFile.errorString()) );
        return false;
    }
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
        // WaitForMoreEvents rather than a bare processEvents(): incoming
        // bytes, the Cancel button and Ctrl-D (slotReceivedChar() closing
        // mFile) all arrive as queued Qt events, so there's nothing useful
        // to do between them. The previous unconditional AllEvents call
        // reran this loop as fast as possible whenever no data was
        // arriving, pegging a CPU core for the entire duration of a
        // download that's just waiting on the remote side.
        QCoreApplication::processEvents( QEventLoop::WaitForMoreEvents );
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
