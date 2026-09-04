/***************************************************************************
                          komportdoc.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : Mon Feb 17 00:05:54 EST 2003
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

// include files for Qt
#include <qdir.h>
#include <qwidget.h>

// include files for KDE
#include <klocale.h>
#include <kmessagebox.h>
#include <kio/job.h>
#include <kio/netaccess.h>

// application specific includes
#include "komportdoc.h"
#include "komport.h"
#include "komportview.h"

QList<KomportView> *KomportDoc::pViewList = 0L;

KomportDoc::KomportDoc(QWidget *parent, const char *name) : QObject(parent, name)
{
  if(!pViewList)
  {
    pViewList = new QList<KomportView>();
  }

  pViewList->setAutoDelete(true);
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
  pViewList->remove(view);
}
void KomportDoc::setURL(const KURL &url)
{
  doc_url=url;
}

const KURL& KomportDoc::URL() const
{
  return doc_url;
}

void KomportDoc::slotUpdateAllViews(KomportView *sender)
{
  KomportView *w;
  if(pViewList)
  {
    for(w=pViewList->first(); w!=0; w=pViewList->next())
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
    KomportApp *win=(KomportApp *) parent();
    int want_save = KMessageBox::warningYesNoCancel(win,
                                         i18n("The current file has been modified.\n"
                                              "Do you want to save it?"),
                                         i18n("Warning"));
    switch(want_save)
    {
      case KMessageBox::Yes:
           if (doc_url.fileName() == i18n("untitled.kom"))
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

      case KMessageBox::No:
           setModified(false);
           deleteContents();
           completed=true;
           break;

      case KMessageBox::Cancel:
           completed=false;
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
  doc_url.setFileName(i18n("untitled.kom"));

  setModified(true);
  
  return true;
}

bool KomportDoc::openDocument(const KURL& url, const char *format /*=0*/)
{
  QString tmpfile;
  KIO::NetAccess::download( url, tmpfile );
  /////////////////////////////////////////////////
  // TODO: Add your document opening code here
  /////////////////////////////////////////////////

  KIO::NetAccess::removeTempFile( tmpfile );

  modified=false;
  return true;
}

bool KomportDoc::saveDocument(const KURL& url, const char *format /*=0*/)
{
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
