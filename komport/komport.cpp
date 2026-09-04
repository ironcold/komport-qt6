/***************************************************************************
                          komport.cpp  -  Komport Serial Port Communicator
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

// include files for QT
#include <qdir.h>
#include <qprinter.h>
#include <qpainter.h>

// include files for KDE
#include <kiconloader.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kmenubar.h>
#include <kstatusbar.h>
#include <klocale.h>
#include <kconfig.h>
#include <kstdaction.h>

// application specific includes
#include "komport.h"
#include "komportview.h"
#include "komportdoc.h"
#include "komporttransfer.h"
#include "komportupload.h"
#include "komportdownload.h"
#include "settingsdialog.h"

#define ID_STATUS_MSG 1

KomportApp::KomportApp(QWidget* , const char* name):KMainWindow(0, name)
{
  config=kapp->config();

  ///////////////////////////////////////////////////////////////////
  // call inits to invoke all other construction parts
  initStatusBar();
  initActions();
  initDocument();
  initView();
	
  readOptions();

  ///////////////////////////////////////////////////////////////////
  // disable actions at startup
  //fileNew->setEnabled(false);
  fileOpenRecent->setEnabled(false);
  fileOpen->setEnabled(true);
  fileSave->setEnabled(true);
  //fileSaveAs->setEnabled(false);
  filePrint->setEnabled(true);
  editCut->setEnabled(false);
  editCopy->setEnabled(false);
  editPaste->setEnabled(false);
}

KomportApp::~KomportApp()
{

}

void KomportApp::initActions()
{
  fileNewWindow = new KAction(i18n("New &Window"), 0, 0, this, SLOT(slotFileNewWindow()), actionCollection(),"file_new_window");
  //fileNew = KStdAction::openNew(this, SLOT(slotFileNew()), actionCollection());
  fileOpen = KStdAction::open(this, SLOT(slotFileOpen()), actionCollection());
  fileOpen->setText(i18n("Upload"));
  fileOpenRecent = KStdAction::openRecent(this, SLOT(slotFileOpenRecent(const KURL&)), actionCollection());
  fileOpenRecent->setText(i18n("Upload recent file..."));
  fileSave = KStdAction::save(this, SLOT(slotFileSave()), actionCollection());
  fileSave->setText(i18n("Download"));
  //fileSaveAs = KStdAction::saveAs(this, SLOT(slotFileSaveAs()), actionCollection());
  //fileSaveAs->setText(i18n("Download As..."));
  fileClose = KStdAction::close(this, SLOT(slotFileClose()), actionCollection());
  filePrint = KStdAction::print(this, SLOT(slotFilePrint()), actionCollection());
  fileQuit = KStdAction::quit(this, SLOT(slotFileQuit()), actionCollection());
  editCut = KStdAction::cut(this, SLOT(slotEditCut()), actionCollection());
  editCopy = KStdAction::copy(this, SLOT(slotEditCopy()), actionCollection());
  editPaste = KStdAction::paste(this, SLOT(slotEditPaste()), actionCollection());
  viewToolBar = KStdAction::showToolbar(this, SLOT(slotViewToolBar()), actionCollection());
  viewStatusBar = KStdAction::showStatusbar(this, SLOT(slotViewStatusBar()), actionCollection());
  showPreferences = KStdAction::preferences(this, SLOT(slotShowPreferences()), actionCollection());
  
  fileNewWindow->setStatusText(i18n("Opens a new application window"));
  //fileNew->setStatusText(i18n("Creates a new document"));
  fileOpen->setStatusText(i18n("Upload a file"));
  fileOpenRecent->setStatusText(i18n("Opens a recently used file"));
  fileSave->setStatusText(i18n("Download a file"));
  //fileSaveAs->setStatusText(i18n("Download a file as..."));
  fileClose->setStatusText(i18n("Closes the actual document"));
  filePrint ->setStatusText(i18n("Prints out the whole screen or selected section"));
  fileQuit->setStatusText(i18n("Quits the application"));
  editCut->setStatusText(i18n("Cuts the selected section and puts it to the clipboard"));
  editCopy->setStatusText(i18n("Copies the selected section to the clipboard"));
  editPaste->setStatusText(i18n("Pastes the clipboard contents"));
  viewToolBar->setStatusText(i18n("Enables/disables the toolbar"));
  viewStatusBar->setStatusText(i18n("Enables/disables the statusbar"));
  showPreferences->setStatusText(i18n("Connection Settings"));

  // use the absolute path to your komportui.rc file for testing purpose in createGUI();
  createGUI();

}


void KomportApp::initStatusBar()
{
  ///////////////////////////////////////////////////////////////////
  // STATUSBAR
  // TODO: add your own items you need for displaying current application status.
  statusBar()->insertItem(i18n("Ready."), ID_STATUS_MSG);
}

void KomportApp::initDocument()
{
  doc = new KomportDoc(this);
  QObject::connect(doc,SIGNAL(documentModified()),this,SLOT(slotDocumentModified()));
  QObject::connect(doc,SIGNAL(viewModified(KomportView*)),this,SLOT(slotViewModified(KomportView*)));
  doc->newDocument();
}

void KomportApp::initView()
{ 
  ////////////////////////////////////////////////////////////////////
  // create the main widget here that is managed by KTMainWindow's view-region and
  // connect the widget to your document to display document contents.

  view = new KomportView(this);
  doc->addView(view);
  setCentralWidget(view);	
  setCaption(doc->URL().fileName(),false);

}

void KomportApp::openDocumentFile(const KURL& url)
{
  slotStatusMsg(i18n("Opening file..."));

  doc->openDocument( url);
  fileOpenRecent->addURL( url );
  slotStatusMsg(i18n("Ready."));
}


KomportDoc *KomportApp::getDocument() const
{
  return doc;
}

void KomportApp::saveOptions()
{	
  config->setGroup("General Options");
  config->writeEntry("Geometry", size());
  config->writeEntry("Show Toolbar", viewToolBar->isChecked());
  config->writeEntry("Show Statusbar",viewStatusBar->isChecked());
  config->writeEntry("ToolBarPos", (int) toolBar("mainToolBar")->barPos());
  fileOpenRecent->saveEntries(config,"Recent Files");

  saveProperties( config );
}


void KomportApp::readOptions()
{
	
  config->setGroup("General Options");

  // bar status settings
  bool bViewToolbar = config->readBoolEntry("Show Toolbar", true);
  viewToolBar->setChecked(bViewToolbar);
  slotViewToolBar();

  bool bViewStatusbar = config->readBoolEntry("Show Statusbar", true);
  viewStatusBar->setChecked(bViewStatusbar);
  slotViewStatusBar();


  // bar position settings
  KToolBar::BarPosition toolBarPos;
  toolBarPos=(KToolBar::BarPosition) config->readNumEntry("ToolBarPos", KToolBar::Top);
  toolBar("mainToolBar")->setBarPos(toolBarPos);
	
  // initialize the recent file list
  fileOpenRecent->loadEntries(config,"Recent Files");

  QSize size=config->readSizeEntry("Geometry");
  if(!size.isEmpty())
  {
    resize(size);
  }

  readProperties( config );
}

void KomportApp::saveProperties(KConfig *_cfg)
{
  if(doc->URL().fileName()!=i18n("Untitled") && !doc->isModified())
  {
    // saving to tempfile not necessary

  }
  else
  {
    KURL url=doc->URL();	
    _cfg->writeEntry("filename", url.url());
    _cfg->writeEntry("modified", doc->isModified());
    QString tempname = kapp->tempSaveName(url.url());
    QString tempurl= KURL::encode_string(tempname);
    KURL _url(tempurl);
    doc->saveDocument(_url);
  }
  if ( !strDevice.isEmpty() ) {
    _cfg->writeEntry("Device", strDevice );
    _cfg->writeEntry("BaudRate", strBaudRate);
    _cfg->writeEntry("FlowControl", strFlowControl );
    _cfg->writeEntry("RXQueue", strRxQueue );
    _cfg->writeEntry("FlushRate", strFlushRate );
    _cfg->writeEntry("StartBits", strStartBits );
    _cfg->writeEntry("DataBits", strDataBits );
    _cfg->writeEntry( "StopBits", strStopBits );
    _cfg->writeEntry( "Parity", strParity );
    _cfg->writeEntry("Emulation", strEmulation );
    _cfg->writeEntry("ScrollBuffer",strScrollBuffer );
    _cfg->sync();
  }
}


void KomportApp::readProperties(KConfig* _cfg)
{
  QString filename = _cfg->readEntry("filename", "");
  KURL url(filename);
  bool modified = _cfg->readBoolEntry("modified", false);
  if(modified)
  {
    bool canRecover;
    QString tempname = kapp->checkRecoverFile(filename, canRecover);
    KURL _url(tempname);
  	
    if(canRecover)
    {
      doc->openDocument(_url);
      doc->setModified();
      setCaption(_url.fileName(),true);
      QFile::remove(tempname);
    }
  }
  else
  {
    if(!filename.isEmpty())
    {
      doc->openDocument(url);
      setCaption(url.fileName(),false);
    }
  }
  strDevice = _cfg->readEntry("Device", "/dev/ttyS0");
  strBaudRate = _cfg->readEntry("BaudRate", "9600");
  strFlowControl = _cfg->readEntry("FlowControl", "XON/XOFF");
  strRxQueue = _cfg->readEntry("RXQueue","1024");
  strFlushRate = _cfg->readEntry("FlushRate","256");
  strStartBits = _cfg->readEntry("StartBits","1");
  strDataBits = _cfg->readEntry("DataBits","8");
  strStopBits = _cfg->readEntry("StopBits","1");
  strParity = _cfg->readEntry("Parity","NONE");
  strEmulation = _cfg->readEntry("Emulation", "VT102");
  strScrollBuffer = _cfg->readEntry("ScrollBuffer","1024");
  view->setScrollBuffer( strScrollBuffer.toInt()  );
  KomportSerial* serial = view->getSerial();
  serial->setDeviceName( strDevice );
  serial->setFraming( strStartBits, strDataBits, strStopBits, strParity );
  serial->setBaudRate( strBaudRate );
  serial->setRxQueue( strRxQueue.toInt() );
  serial->setFlushRate( strFlushRate.toInt() );
  serial->open();
}

bool KomportApp::queryClose()
{
  return doc->saveModified();
}

bool KomportApp::queryExit()
{
  saveOptions();
  return true;
}

/////////////////////////////////////////////////////////////////////
// SLOT IMPLEMENTATION
/////////////////////////////////////////////////////////////////////

void KomportApp::slotFileNewWindow()
{
  slotStatusMsg(i18n("Opening a new application window..."));
	
  KomportApp *new_window= new KomportApp();
  new_window->show();

  slotStatusMsg(i18n("Ready."));
}

void KomportApp::slotFileNew()
{
  slotStatusMsg(i18n("Creating new document..."));

  if(!doc->saveModified())
  {
     // here saving wasn't successful

  }
  else
  {	
    doc->newDocument();		
    setCaption(doc->URL().fileName(), false);
  }

  slotStatusMsg(i18n("Ready."));
}

void KomportApp::slotFileOpen()
{
  slotStatusMsg(i18n("Uploading file..."));
  KURL url=KFileDialog::getOpenURL(QString::null,  i18n("*"), this, i18n("Upload File..."));
  if(!url.isEmpty())
    {
      KomportTransfer transfer( view->getSerial(), view );
      transfer.setURL(url);
      transfer.upload();
      fileOpenRecent->addURL( url );
    }
  slotStatusMsg(i18n("Ready."));
}

void KomportApp::slotFileOpenRecent(const KURL& url)
{
  slotStatusMsg(i18n("Uploading file..."));
  if(!url.isEmpty())
    {
      KomportTransfer transfer( view->getSerial(), view );
      transfer.setURL(url);
      transfer.upload();
      fileOpenRecent->addURL( url );
    }
  slotStatusMsg(i18n("Ready."));
}

void KomportApp::slotFileSave()
{
    slotFileSaveAs();
}

void KomportApp::slotFileSaveAs()
{
  slotStatusMsg(i18n("Downloading a file..."));
  KURL url=KFileDialog::getSaveURL(QDir::currentDirPath(),i18n("*|All files"), this, i18n("Save as..."));
  if(!url.isEmpty())
    {
      KomportTransfer transfer( view->getSerial(), view );
      transfer.setURL(url);
      transfer.download();
      fileOpenRecent->addURL( url );
    }
  slotStatusMsg(i18n("Ready."));
}

void KomportApp::slotFileClose()
{
  slotStatusMsg(i18n("Closing file..."));
	
  view->getSerial()->close();
  close();

  slotStatusMsg(i18n("Ready."));
}

void KomportApp::slotFilePrint()
{
  slotStatusMsg(i18n("Printing..."));

  QPrinter printer;
  if (printer.setup(this))
  {
    view->print(&printer);
  }

  slotStatusMsg(i18n("Ready."));
}

void KomportApp::slotFileQuit()
{
  slotStatusMsg(i18n("Exiting..."));
  saveOptions();
  // close the first window, the list makes the next one the first again.
  // This ensures that queryClose() is called on each window to ask for closing
  KMainWindow* w;
  if(memberList)
  {
    for(w=memberList->first(); w!=0; w=memberList->first())
    {
      // only close the window if the closeEvent is accepted. If the user presses Cancel on the saveModified() dialog,
      // the window and the application stay open.
      if(!w->close())
	break;
    }
  }	
}

void KomportApp::slotEditCut()
{
  slotStatusMsg(i18n("Cutting selection..."));

  slotStatusMsg(i18n("Ready."));
}

void KomportApp::slotEditCopy()
{
  slotStatusMsg(i18n("Copying selection to clipboard..."));
  if ( view->hasSelection() ) {
      QClipboard* cb = QApplication::clipboard();
      QString str= cb->text( QClipboard::Selection );
      cb->setText(  str, QClipboard::Clipboard );
  }
  slotStatusMsg(i18n("Ready."));
}

void KomportApp::slotEditPaste()
{
  slotStatusMsg(i18n("Inserting clipboard contents..."));
  QClipboard* cb = QApplication::clipboard();
  QString str = cb->text( QClipboard::Clipboard );
  for ( int i=0; i < str.length(); i++ ) {
      //kapp->processEvents();
      view->slotSimKeyPressed( str[ i ] );
  }
  slotStatusMsg(i18n("Ready."));
}

void KomportApp::slotViewToolBar()
{
  slotStatusMsg(i18n("Toggling toolbar..."));
  ///////////////////////////////////////////////////////////////////
  // turn Toolbar on or off
  if(!viewToolBar->isChecked())
  {
    toolBar("mainToolBar")->hide();
  }
  else
  {
    toolBar("mainToolBar")->show();
  }		

  slotStatusMsg(i18n("Ready."));
}

void KomportApp::slotViewStatusBar()
{
  slotStatusMsg(i18n("Toggle the statusbar..."));
  ///////////////////////////////////////////////////////////////////
  //turn Statusbar on or off
  if(!viewStatusBar->isChecked())
  {
    statusBar()->hide();
  }
  else
  {
    statusBar()->show();
  }

  slotStatusMsg(i18n("Ready."));
}


void KomportApp::slotShowPreferences()
{
  slotStatusMsg(i18n("Open settings form..."));
  ///////////////////////////////////////////////////////////////////
  // open the settings dialog...
  SettingsDialog settingsDialog;

  readProperties( config );
  settingsDialog.DeviceComboBox->setCurrentText(  strDevice );
  settingsDialog.BaudRateComboBox->setCurrentText(  strBaudRate );
  settingsDialog.FlowControlComboBox->setCurrentText(  strFlowControl );
  settingsDialog.RxQueueSpinBox->setValue( strRxQueue.toInt() );
  settingsDialog.FlushRateSpinBox->setValue( strFlushRate.toInt() );
  settingsDialog.StartBitsComboBox->setCurrentText( strStartBits );
  settingsDialog.DataBitsComboBox->setCurrentText( strDataBits );
  settingsDialog.StopBitsComboBox->setCurrentText( strStopBits );
  settingsDialog.ParityComboBox->setCurrentText( strParity );
  settingsDialog.EmulationComboBox->setCurrentText(  strEmulation );  
  settingsDialog.ScrollBufferSpinBox->setValue( strScrollBuffer.toInt() );
  if ( settingsDialog.exec() == QDialog::Accepted ) {
      strDevice =  settingsDialog.DeviceComboBox->currentText();
      strBaudRate =  settingsDialog.BaudRateComboBox->currentText() ;
      strFlowControl = settingsDialog.FlowControlComboBox->currentText();
      strRxQueue = settingsDialog.RxQueueSpinBox->text();
      strFlushRate = settingsDialog.FlushRateSpinBox->text();
      strStartBits =  settingsDialog.StartBitsComboBox->currentText() ;
      strDataBits =  settingsDialog.DataBitsComboBox->currentText() ;
      strStopBits =  settingsDialog.StopBitsComboBox->currentText() ;
      strParity =  settingsDialog.ParityComboBox->currentText() ;
      strEmulation = settingsDialog.EmulationComboBox->currentText();
      strScrollBuffer = settingsDialog.ScrollBufferSpinBox->text();
      saveProperties( config );
      readProperties( config );
  } 
  
  slotStatusMsg(i18n("Ready."));
}

void KomportApp::slotStatusMsg(const QString &text)
{
  ///////////////////////////////////////////////////////////////////
  // change status message permanently
  statusBar()->clear();
  statusBar()->changeItem(text, ID_STATUS_MSG);
}

/** Document has changed.  */
void KomportApp::slotDocumentModified(){
  //fileSave->setEnabled(doc->isModified());
  //fileSaveAs->setEnabled(doc->isModified());
}
/** No descriptions */
void KomportApp::slotViewModified(KomportView* _v){
    editCopy->setEnabled( _v->hasSelection() );
    editPaste->setEnabled( !QApplication::clipboard()->text( QClipboard::Clipboard ).isEmpty() );
}
/** get configuration object */
KConfig* KomportApp::getConfig(){
    return config;
}
