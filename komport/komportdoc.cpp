/***************************************************************************
                          komportdoc.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : Mon Feb 17 2003
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

// include files for Qt
#include <QDir>
#include <QWidget>
#include <QMessageBox>
#include <QFileInfo>

// application specific includes
#include "komportdoc.h"
#include "komport.h"
#include "komportview.h"

QList<KomportView*> *KomportDoc::pViewList = nullptr;

KomportDoc::KomportDoc(QObject *parent) : QObject(parent)
, modified(false)
{
  if(!pViewList)
  {
    pViewList = new QList<KomportView*>();
  }
  // Note: unlike the original Qt3 QPtrList, this list does not own/delete
  // the views it tracks - it is only used to broadcast repaints across all
  // open windows (see slotUpdateAllViews()). Views are owned as normal
  // QWidget children and destroyed with their window.
}

KomportDoc::~KomportDoc()
{
}

void KomportDoc::addView(KomportView *view)
{
  pViewList->append(view);
  QObject::connect(view,SIGNAL(viewModified(KomportView*)),this,SLOT(slotViewModified(KomportView*)));
}

void KomportDoc::removeView(KomportView *view)
{
  pViewList->removeAll(view);
}
void KomportDoc::setURL(const QUrl &url)
{
  doc_url=url;
}

const QUrl& KomportDoc::URL() const
{
  return doc_url;
}

/** display name of the document */
QString KomportDoc::fileName() const
{
  QString name = QFileInfo( doc_url.toLocalFile() ).fileName();
  return name.isEmpty() ? QStringLiteral("Untitled") : name;
}

void KomportDoc::slotUpdateAllViews(KomportView *sender)
{
  if(pViewList)
  {
    for( KomportView *w : std::as_const(*pViewList) )
    {
      if(w!=sender)
        w->repaint();
    }
  }

}

bool KomportDoc::saveModified()
{
  bool completed=true;

  if(modified)
  {
    KomportApp *win = qobject_cast<KomportApp *>(parent());
    QMessageBox::StandardButton want_save = QMessageBox::warning(win,
                                         tr("Warning"),
                                         tr("The current file has been modified.\n"
                                              "Do you want to save it?"),
                                         QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    switch(want_save)
    {
      case QMessageBox::Yes:
           if (fileName() == QLatin1String("untitled.kom"))
           {
             win->slotFileSaveAs();
           }
           else
           {
             saveDocument(URL());
       	   };

       	   deleteContents();
           completed=true;
           break;

      case QMessageBox::No:
           setModified(false);
           deleteContents();
           completed=true;
           break;

      default:
           completed=false;
           break;
    }
  }

  return completed;
}

void KomportDoc::closeDocument()
{
  deleteContents();
}

bool KomportDoc::newDocument()
{
  /////////////////////////////////////////////////
  // TODO: Add your document initialization code here
  /////////////////////////////////////////////////
  modified=false;
  doc_url = QUrl::fromLocalFile( QStringLiteral("untitled.kom") );

  setModified(true);

  return true;
}

bool KomportDoc::openDocument(const QUrl& url)
{
  Q_UNUSED(url);
  /////////////////////////////////////////////////
  // TODO: Add your document opening code here
  // (this was already an empty stub in the original KDE3 version - no file
  // content was ever actually read here, so nothing was lost by dropping
  // the KIO::NetAccess remote-download plumbing that used to wrap it)
  /////////////////////////////////////////////////

  modified=false;
  return true;
}

bool KomportDoc::saveDocument(const QUrl& url)
{
  Q_UNUSED(url);
  /////////////////////////////////////////////////
  // TODO: Add your document saving code here
  /////////////////////////////////////////////////

  modified=false;
  return true;
}

void KomportDoc::deleteContents()
{
  /////////////////////////////////////////////////
  // TODO: Add implementation to delete the document contents
  /////////////////////////////////////////////////

}

void KomportDoc::setModified(bool _m)
{
  bool changed = (_m!=modified);
  modified=_m;
  if ( changed && modified ) {
    emit documentModified();
  }
};
/** get the serial port */
KomportSerial* KomportDoc::getSerial(){
  return &mSerial;
}
/** No descriptions */
void KomportDoc::slotViewModified(KomportView* _v){
    emit viewModified(_v);
}
