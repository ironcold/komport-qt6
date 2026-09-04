/***************************************************************************
                          komport.cpp  -  Komport Serial Port Communicator
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
#include <QPrinter>
#include <QPrintDialog>
#include <QPainter>
#include <QSettings>
#include <QMessageBox>
#include <QFileDialog>
#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QStatusBar>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QIcon>
#include <QFileInfo>
#include <QComboBox>
#include <QSpinBox>

// application specific includes
#include "komport.h"
#include "komportview.h"
#include "komportdoc.h"
#include "komporttransfer.h"
#include "settingsdialog.h"

static const int MAX_RECENT_FILES = 10;

KomportApp::KomportApp(QWidget* parent):QMainWindow(parent)
{
  setWindowIcon( QIcon(QStringLiteral(":/icons/lo32-app-komport.png")) );

  config = new QSettings(this);

  ///////////////////////////////////////////////////////////////////
  // call inits to invoke all other construction parts
  initStatusBar();
  initActions();
  initMenus();
  initToolBar();
  initDocument();
  initView();

  readOptions();
}

KomportApp::~KomportApp()
{

}

void KomportApp::initActions()
{
  fileNewWindow = new QAction( tr("New &Window"), this );
  connect( fileNewWindow, &QAction::triggered, this, &KomportApp::slotFileNewWindow );
  fileNewWindow->setStatusTip( tr("Opens a new application window") );

  fileOpen = new QAction( QIcon::fromTheme(QStringLiteral("document-open")), tr("&Upload..."), this );
  fileOpen->setShortcut( QKeySequence::Open );
  connect( fileOpen, &QAction::triggered, this, &KomportApp::slotFileOpen );
  fileOpen->setStatusTip( tr("Upload a file") );

  fileSave = new QAction( QIcon::fromTheme(QStringLiteral("document-save")), tr("&Download..."), this );
  fileSave->setShortcut( QKeySequence::Save );
  connect( fileSave, &QAction::triggered, this, &KomportApp::slotFileSave );
  fileSave->setStatusTip( tr("Download a file") );

  fileClose = new QAction( tr("&Close"), this );
  fileClose->setShortcut( QKeySequence::Close );
  connect( fileClose, &QAction::triggered, this, &KomportApp::slotFileClose );
  fileClose->setStatusTip( tr("Closes the actual document") );

  filePrint = new QAction( QIcon::fromTheme(QStringLiteral("document-print")), tr("&Print..."), this );
  filePrint->setShortcut( QKeySequence::Print );
  connect( filePrint, &QAction::triggered, this, &KomportApp::slotFilePrint );
  filePrint->setStatusTip( tr("Prints out the whole screen or selected section") );

  fileQuit = new QAction( QIcon::fromTheme(QStringLiteral("application-exit")), tr("&Quit"), this );
  fileQuit->setShortcut( QKeySequence::Quit );
  connect( fileQuit, &QAction::triggered, this, &KomportApp::slotFileQuit );
  fileQuit->setStatusTip( tr("Quits the application") );

  editCut = new QAction( QIcon::fromTheme(QStringLiteral("edit-cut")), tr("Cu&t"), this );
  editCut->setShortcut( QKeySequence::Cut );
  connect( editCut, &QAction::triggered, this, &KomportApp::slotEditCut );
  editCut->setStatusTip( tr("Cuts the selected section and puts it to the clipboard") );
  editCut->setEnabled( false );

  editCopy = new QAction( QIcon::fromTheme(QStringLiteral("edit-copy")), tr("&Copy"), this );
  editCopy->setShortcut( QKeySequence::Copy );
  connect( editCopy, &QAction::triggered, this, &KomportApp::slotEditCopy );
  editCopy->setStatusTip( tr("Copies the selected section to the clipboard") );
  editCopy->setEnabled( false );

  editPaste = new QAction( QIcon::fromTheme(QStringLiteral("edit-paste")), tr("&Paste"), this );
  editPaste->setShortcut( QKeySequence::Paste );
  connect( editPaste, &QAction::triggered, this, &KomportApp::slotEditPaste );
  editPaste->setStatusTip( tr("Pastes the clipboard contents") );
  editPaste->setEnabled( false );

  viewToolBar = new QAction( tr("Show &Toolbar"), this );
  viewToolBar->setCheckable( true );
  viewToolBar->setChecked( true );
  connect( viewToolBar, &QAction::triggered, this, &KomportApp::slotViewToolBar );
  viewToolBar->setStatusTip( tr("Enables/disables the toolbar") );

  viewStatusBar = new QAction( tr("Show &Statusbar"), this );
  viewStatusBar->setCheckable( true );
  viewStatusBar->setChecked( true );
  connect( viewStatusBar, &QAction::triggered, this, &KomportApp::slotViewStatusBar );
  viewStatusBar->setStatusTip( tr("Enables/disables the statusbar") );

  showPreferences = new QAction( QIcon::fromTheme(QStringLiteral("preferences-system")), tr("&Connection Settings..."), this );
  showPreferences->setShortcut( QKeySequence::Preferences );
  connect( showPreferences, &QAction::triggered, this, &KomportApp::slotShowPreferences );
  showPreferences->setStatusTip( tr("Connection Settings") );
}

void KomportApp::initMenus()
{
  QMenu *fileMenu = menuBar()->addMenu( tr("&File") );
  fileMenu->addAction( fileNewWindow );
  fileMenu->addSeparator();
  fileMenu->addAction( fileOpen );
  fileOpenRecentMenu = fileMenu->addMenu( tr("Upload &recent file") );
  fileMenu->addAction( fileSave );
  fileMenu->addSeparator();
  fileMenu->addAction( fileClose );
  fileMenu->addAction( filePrint );
  fileMenu->addSeparator();
  fileMenu->addAction( fileQuit );

  QMenu *editMenu = menuBar()->addMenu( tr("&Edit") );
  editMenu->addAction( editCut );
  editMenu->addAction( editCopy );
  editMenu->addAction( editPaste );

  QMenu *viewMenu = menuBar()->addMenu( tr("&View") );
  viewMenu->addAction( viewToolBar );
  viewMenu->addAction( viewStatusBar );

  QMenu *settingsMenu = menuBar()->addMenu( tr("&Settings") );
  settingsMenu->addAction( showPreferences );

  rebuildRecentFilesMenu();
}

void KomportApp::initToolBar()
{
  mainToolBar = addToolBar( tr("Main Toolbar") );
  mainToolBar->setObjectName( QStringLiteral("mainToolBar") );
  mainToolBar->addAction( fileOpen );
  mainToolBar->addAction( fileSave );
  mainToolBar->addAction( filePrint );
  mainToolBar->addSeparator();
  mainToolBar->addAction( editCut );
  mainToolBar->addAction( editCopy );
  mainToolBar->addAction( editPaste );
}

void KomportApp::initStatusBar()
{
  ///////////////////////////////////////////////////////////////////
  // STATUSBAR
  statusBar()->showMessage( tr("Ready.") );
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
  // create the main widget here that is managed by the main window's view-region and
  // connect the widget to your document to display document contents.

  view = new KomportView(this);
  doc->addView(view);
  setCentralWidget(view);
  setWindowTitle( doc->fileName() );
}

void KomportApp::openDocumentFile(const QUrl& url)
{
  slotStatusMsg(tr("Opening file..."));

  doc->openDocument( url );
  if ( !url.isEmpty() ) addRecentFile( url );
  slotStatusMsg(tr("Ready."));
}


KomportDoc *KomportApp::getDocument() const
{
  return doc;
}

void KomportApp::addRecentFile(const QUrl& url)
{
  mRecentFiles.removeAll(url);
  mRecentFiles.prepend(url);
  while ( mRecentFiles.size() > MAX_RECENT_FILES ) mRecentFiles.removeLast();
  rebuildRecentFilesMenu();
}

void KomportApp::rebuildRecentFilesMenu()
{
  fileOpenRecentMenu->clear();
  fileOpenRecentMenu->setEnabled( !mRecentFiles.isEmpty() );
  for ( const QUrl &url : std::as_const(mRecentFiles) ) {
    QAction *action = fileOpenRecentMenu->addAction( url.toDisplayString() );
    connect( action, &QAction::triggered, this, [this, url]() { slotFileOpenRecent(url); } );
  }
}

void KomportApp::saveOptions()
{
  config->beginGroup( QStringLiteral("General Options") );
  config->setValue( QStringLiteral("Geometry"), size() );
  config->setValue( QStringLiteral("Show Toolbar"), viewToolBar->isChecked() );
  config->setValue( QStringLiteral("Show Statusbar"), viewStatusBar->isChecked() );
  QStringList recent;
  for ( const QUrl &url : std::as_const(mRecentFiles) ) recent << url.toString();
  config->setValue( QStringLiteral("Recent Files"), recent );
  config->endGroup();

  config->beginGroup( QStringLiteral("Connection") );
  config->setValue( QStringLiteral("Device"), strDevice );
  config->setValue( QStringLiteral("BaudRate"), strBaudRate );
  config->setValue( QStringLiteral("FlowControl"), strFlowControl );
  config->setValue( QStringLiteral("RXQueue"), strRxQueue );
  config->setValue( QStringLiteral("FlushRate"), strFlushRate );
  config->setValue( QStringLiteral("StartBits"), strStartBits );
  config->setValue( QStringLiteral("DataBits"), strDataBits );
  config->setValue( QStringLiteral("StopBits"), strStopBits );
  config->setValue( QStringLiteral("Parity"), strParity );
  config->setValue( QStringLiteral("Emulation"), strEmulation );
  config->setValue( QStringLiteral("ScrollBuffer"), strScrollBuffer );
  config->endGroup();
  config->sync();
}


void KomportApp::readOptions()
{
  config->beginGroup( QStringLiteral("General Options") );

  bool bViewToolbar = config->value( QStringLiteral("Show Toolbar"), true ).toBool();
  viewToolBar->setChecked(bViewToolbar);
  slotViewToolBar();

  bool bViewStatusbar = config->value( QStringLiteral("Show Statusbar"), true ).toBool();
  viewStatusBar->setChecked(bViewStatusbar);
  slotViewStatusBar();

  mRecentFiles.clear();
  const QStringList recent = config->value( QStringLiteral("Recent Files") ).toStringList();
  for ( const QString &s : recent ) mRecentFiles << QUrl(s);
  rebuildRecentFilesMenu();

  QSize sz = config->value( QStringLiteral("Geometry") ).toSize();
  config->endGroup();
  if ( sz.isValid() && !sz.isEmpty() )
  {
    resize(sz);
  }

  config->beginGroup( QStringLiteral("Connection") );
  strDevice = config->value( QStringLiteral("Device"), QStringLiteral("/dev/ttyS0") ).toString();
  strBaudRate = config->value( QStringLiteral("BaudRate"), QStringLiteral("9600") ).toString();
  strFlowControl = config->value( QStringLiteral("FlowControl"), QStringLiteral("NONE") ).toString();
  strRxQueue = config->value( QStringLiteral("RXQueue"), QStringLiteral("1024") ).toString();
  strFlushRate = config->value( QStringLiteral("FlushRate"), QStringLiteral("256") ).toString();
  strStartBits = config->value( QStringLiteral("StartBits"), QStringLiteral("1") ).toString();
  strDataBits = config->value( QStringLiteral("DataBits"), QStringLiteral("8") ).toString();
  strStopBits = config->value( QStringLiteral("StopBits"), QStringLiteral("1") ).toString();
  strParity = config->value( QStringLiteral("Parity"), QStringLiteral("NONE") ).toString();
  strEmulation = config->value( QStringLiteral("Emulation"), QStringLiteral("VT102") ).toString();
  strScrollBuffer = config->value( QStringLiteral("ScrollBuffer"), QStringLiteral("1024") ).toString();
  config->endGroup();

  view->setScrollBuffer( strScrollBuffer.toInt() );
  KomportSerial* serial = view->getSerial();
  serial->setDeviceName( strDevice );
  serial->setFraming( strStartBits, strDataBits, strStopBits, strParity );
  serial->setFlowControl( strFlowControl );
  serial->setBaudRate( strBaudRate );
  serial->setRxQueue( strRxQueue.toInt() );
  serial->setFlushRate( strFlushRate.toInt() );
  serial->open();
}

void KomportApp::closeEvent(QCloseEvent *event)
{
  if ( doc->saveModified() ) {
    saveOptions();
    event->accept();
  } else {
    event->ignore();
  }
}

/////////////////////////////////////////////////////////////////////
// SLOT IMPLEMENTATION
/////////////////////////////////////////////////////////////////////

void KomportApp::slotFileNewWindow()
{
  slotStatusMsg(tr("Opening a new application window..."));

  KomportApp *new_window= new KomportApp();
  new_window->show();

  slotStatusMsg(tr("Ready."));
}

void KomportApp::slotFileNew()
{
  slotStatusMsg(tr("Creating new document..."));

  if(!doc->saveModified())
  {
     // here saving wasn't successful

  }
  else
  {
    doc->newDocument();
    setWindowTitle( doc->fileName() );
  }

  slotStatusMsg(tr("Ready."));
}

void KomportApp::slotFileOpen()
{
  slotStatusMsg(tr("Uploading file..."));
  QString fileName = QFileDialog::getOpenFileName( this, tr("Upload File...") );
  if ( !fileName.isEmpty() )
    {
      KomportTransfer transfer( view->getSerial(), view );
      transfer.setFileName(fileName);
      transfer.upload();
      addRecentFile( QUrl::fromLocalFile(fileName) );
    }
  slotStatusMsg(tr("Ready."));
}

void KomportApp::slotFileOpenRecent(const QUrl& url)
{
  slotStatusMsg(tr("Uploading file..."));
  if(!url.isEmpty())
    {
      KomportTransfer transfer( view->getSerial(), view );
      transfer.setFileName( url.toLocalFile() );
      transfer.upload();
      addRecentFile( url );
    }
  slotStatusMsg(tr("Ready."));
}

void KomportApp::slotFileSave()
{
    slotFileSaveAs();
}

void KomportApp::slotFileSaveAs()
{
  slotStatusMsg(tr("Downloading a file..."));
  QString fileName = QFileDialog::getSaveFileName( this, tr("Save as..."), QDir::currentPath() );
  if ( !fileName.isEmpty() )
    {
      KomportTransfer transfer( view->getSerial(), view );
      transfer.setFileName(fileName);
      transfer.download();
      addRecentFile( QUrl::fromLocalFile(fileName) );
    }
  slotStatusMsg(tr("Ready."));
}

void KomportApp::slotFileClose()
{
  slotStatusMsg(tr("Closing file..."));

  view->getSerial()->close();
  close();

  slotStatusMsg(tr("Ready."));
}

void KomportApp::slotFilePrint()
{
  slotStatusMsg(tr("Printing..."));

  QPrinter printer;
  QPrintDialog dlg(&printer, this);
  if (dlg.exec() == QDialog::Accepted)
  {
    view->print(&printer);
  }

  slotStatusMsg(tr("Ready."));
}

void KomportApp::slotFileQuit()
{
  slotStatusMsg(tr("Exiting..."));
  // close each top-level window; the closeEvent()/saveModified() flow on
  // each one decides whether the close (and thus the overall quit) can go
  // ahead. Mirrors the original KMainWindow::memberList walk.
  const QWidgetList windows = QApplication::topLevelWidgets();
  for ( QWidget *w : windows )
  {
    if ( auto *win = qobject_cast<KomportApp*>(w) )
    {
      if ( !win->close() )
        break;
    }
  }
}

void KomportApp::slotEditCut()
{
  slotStatusMsg(tr("Cutting selection..."));

  slotStatusMsg(tr("Ready."));
}

void KomportApp::slotEditCopy()
{
  slotStatusMsg(tr("Copying selection to clipboard..."));
  if ( view->hasSelection() ) {
      QClipboard* cb = QApplication::clipboard();
      QString str= cb->text( QClipboard::Selection );
      cb->setText(  str, QClipboard::Clipboard );
  }
  slotStatusMsg(tr("Ready."));
}

void KomportApp::slotEditPaste()
{
  slotStatusMsg(tr("Inserting clipboard contents..."));
  QClipboard* cb = QApplication::clipboard();
  QString str = cb->text( QClipboard::Clipboard );
  for ( int i=0; i < str.length(); i++ ) {
      view->slotSimKeyPressed( str[ i ] );
  }
  slotStatusMsg(tr("Ready."));
}

void KomportApp::slotViewToolBar()
{
  slotStatusMsg(tr("Toggling toolbar..."));
  mainToolBar->setVisible( viewToolBar->isChecked() );
  slotStatusMsg(tr("Ready."));
}

void KomportApp::slotViewStatusBar()
{
  slotStatusMsg(tr("Toggle the statusbar..."));
  statusBar()->setVisible( viewStatusBar->isChecked() );
  slotStatusMsg(tr("Ready."));
}


void KomportApp::slotShowPreferences()
{
  slotStatusMsg(tr("Open settings form..."));
  ///////////////////////////////////////////////////////////////////
  // open the settings dialog...
  SettingsDialog settingsDialog(this);

  settingsDialog.DeviceComboBox->setCurrentText( strDevice );
  settingsDialog.BaudRateComboBox->setCurrentText( strBaudRate );
  settingsDialog.FlowControlComboBox->setCurrentText( strFlowControl );
  settingsDialog.RxQueueSpinBox->setValue( strRxQueue.toInt() );
  settingsDialog.FlushRateSpinBox->setValue( strFlushRate.toInt() );
  settingsDialog.StartBitsComboBox->setCurrentText( strStartBits );
  settingsDialog.DataBitsComboBox->setCurrentText( strDataBits );
  settingsDialog.StopBitsComboBox->setCurrentText( strStopBits );
  settingsDialog.ParityComboBox->setCurrentText( strParity );
  settingsDialog.EmulationComboBox->setCurrentText( strEmulation );
  settingsDialog.ScrollBufferSpinBox->setValue( strScrollBuffer.toInt() );
  if ( settingsDialog.exec() == QDialog::Accepted ) {
      strDevice =  settingsDialog.DeviceComboBox->currentText();
      strBaudRate =  settingsDialog.BaudRateComboBox->currentText() ;
      strFlowControl = settingsDialog.FlowControlComboBox->currentText();
      strRxQueue = QString::number( settingsDialog.RxQueueSpinBox->value() );
      strFlushRate = QString::number( settingsDialog.FlushRateSpinBox->value() );
      strStartBits =  settingsDialog.StartBitsComboBox->currentText() ;
      strDataBits =  settingsDialog.DataBitsComboBox->currentText() ;
      strStopBits =  settingsDialog.StopBitsComboBox->currentText() ;
      strParity =  settingsDialog.ParityComboBox->currentText() ;
      strEmulation = settingsDialog.EmulationComboBox->currentText();
      strScrollBuffer = QString::number( settingsDialog.ScrollBufferSpinBox->value() );

      view->setScrollBuffer( strScrollBuffer.toInt() );
      KomportSerial* serial = view->getSerial();
      serial->setDeviceName( strDevice );
      serial->setFraming( strStartBits, strDataBits, strStopBits, strParity );
      serial->setFlowControl( strFlowControl );
      serial->setBaudRate( strBaudRate );
      serial->setRxQueue( strRxQueue.toInt() );
      serial->setFlushRate( strFlushRate.toInt() );
      if ( !serial->isOpen() ) serial->open();
  }

  slotStatusMsg(tr("Ready."));
}

void KomportApp::slotStatusMsg(const QString &text)
{
  ///////////////////////////////////////////////////////////////////
  // change status message permanently
  statusBar()->showMessage(text);
}

/** Document has changed.  */
void KomportApp::slotDocumentModified(){
}
/** No descriptions */
void KomportApp::slotViewModified(KomportView* _v){
    editCopy->setEnabled( _v->hasSelection() );
    editPaste->setEnabled( !QApplication::clipboard()->text( QClipboard::Clipboard ).isEmpty() );
}
/** get configuration object */
QSettings* KomportApp::getConfig(){
    return config;
}
