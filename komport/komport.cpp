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
#include <QEvent>
#include <QHelpEvent>
#include <QToolTip>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QIcon>
#include <QFileInfo>
#include <QComboBox>
#include <QSpinBox>
#include <QSplitter>
#include <QLabel>
#include <QLineEdit>

// application specific includes
#include "komport.h"
#include "komportview.h"
#include "komportdoc.h"
#include "komporttransfer.h"
#include "settingsdialog.h"
#include "komporthexview.h"
#include "komportmacrobar.h"
#include "komportsessionlogger.h"

static const int MAX_RECENT_FILES = 10;

KomportApp::KomportApp(QWidget* parent):QMainWindow(parent)
{
  // Without this, closing a window via the window manager's close button
  // only hides it - the QMainWindow object (and everything it owns: the
  // serial port, doc, view, timers, signal/slot connections) stays alive
  // for the rest of the process. With multiple windows open, a "closed"
  // one could keep holding its serial port exclusively forever. File ->
  // Close/Quit already worked around this by calling close() on the port
  // explicitly (see slotFileClose()/closeEvent() below), but the window
  // manager path bypassed that. WA_DeleteOnClose makes a real close()
  // actually destroy the window once closeEvent() accepts it.
  setAttribute( Qt::WA_DeleteOnClose );

  setWindowIcon( QIcon(QStringLiteral(":/icons/lo32-app-komport.png")) );

  config = new QSettings(this);

  sessionLogger = new KomportSessionLogger(this);

  ///////////////////////////////////////////////////////////////////
  // call inits to invoke all other construction parts
  initStatusBar();
  initActions();
  initMenus();
  initToolBar();
  initDocument();
  initView();
  initMacroBar();

  // hex monitor and session logger both watch the raw byte stream,
  // independent of what the VT100/VT102 emulation makes of it
  connect( view->getSerial(), &KomportSerial::receivedChar, hexView, &KomportHexView::appendRx );
  connect( view->getSerial(), &KomportSerial::sentChar, hexView, &KomportHexView::appendTx );
  connect( view->getSerial(), &KomportSerial::receivedChar, sessionLogger, &KomportSessionLogger::logChar );
  connect( view->getSerial(), &KomportSerial::settingsFailed, this, &KomportApp::slotSerialSettingsFailed );

  readOptions();
  // Milestone 7 addendum: populate the custom-charset registry before any
  // profile gets loaded below - a profile whose Charset value names a
  // custom charset needs it to already be resolvable.
  KomportCharset::reloadCustomCharsets();
  // Codex review round-4 finding (Medium): this reload is just as capable
  // of invalidating an *already open* window's live Custom selection as
  // the one in slotShowPreferences() is (e.g. "New Window" while another
  // window has a since-deleted custom charset selected) - the registry is
  // the same process-wide shared object either way. initView() above
  // already ran, so `this` window's own view->mEmulation exists and is
  // safe to reconcile here too, alongside every other already-open window.
  reconcileCharsetSelectionAfterReloadForAllWindows();
  seedBuiltinProfiles();
  initProfiles();
}

KomportApp::~KomportApp()
{
  // pViewList is a static list shared across all KomportApp windows (see
  // KomportDoc::slotUpdateAllViews()). It was never pruned before, which
  // used to be masked by windows never actually being destroyed (see
  // WA_DeleteOnClose above) - now that they are, unregister this window's
  // view here, while doc/view are still valid, rather than leaving a
  // dangling pointer behind for the next slotUpdateAllViews() broadcast.
  doc->removeView( view );
}

void KomportApp::initActions()
{
  // Status-bar hint texts (setStatusTip) are shown at the bottom while
  // hovering a menu item - kept short and to the point on purpose: the
  // status bar sits below a window that's only as wide as the terminal's
  // fixed character grid, and QStatusBar clips rather than wraps overflow,
  // so a full sentence there just gets cut off (this is what "tooltips get
  // cut off in the menu" actually was).
  fileNewWindow = new QAction( tr("New &Window"), this );
  connect( fileNewWindow, &QAction::triggered, this, &KomportApp::slotFileNewWindow );
  fileNewWindow->setStatusTip( tr("Open a new window") );

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
  fileClose->setStatusTip( tr("Close the window") );

  filePrint = new QAction( QIcon::fromTheme(QStringLiteral("document-print")), tr("&Print..."), this );
  filePrint->setShortcut( QKeySequence::Print );
  connect( filePrint, &QAction::triggered, this, &KomportApp::slotFilePrint );
  filePrint->setStatusTip( tr("Print the screen") );

  fileQuit = new QAction( QIcon::fromTheme(QStringLiteral("application-exit")), tr("&Quit"), this );
  fileQuit->setShortcut( QKeySequence::Quit );
  connect( fileQuit, &QAction::triggered, this, &KomportApp::slotFileQuit );
  fileQuit->setStatusTip( tr("Quit") );

  editCut = new QAction( QIcon::fromTheme(QStringLiteral("edit-cut")), tr("Cu&t"), this );
  editCut->setShortcut( QKeySequence::Cut );
  connect( editCut, &QAction::triggered, this, &KomportApp::slotEditCut );
  editCut->setStatusTip( tr("Cut selection") );
  editCut->setEnabled( false );

  editCopy = new QAction( QIcon::fromTheme(QStringLiteral("edit-copy")), tr("&Copy"), this );
  editCopy->setShortcut( QKeySequence::Copy );
  connect( editCopy, &QAction::triggered, this, &KomportApp::slotEditCopy );
  editCopy->setStatusTip( tr("Copy selection") );
  editCopy->setEnabled( false );

  editPaste = new QAction( QIcon::fromTheme(QStringLiteral("edit-paste")), tr("&Paste"), this );
  editPaste->setShortcut( QKeySequence::Paste );
  connect( editPaste, &QAction::triggered, this, &KomportApp::slotEditPaste );
  editPaste->setStatusTip( tr("Paste") );
  editPaste->setEnabled( false );

  viewToolBar = new QAction( tr("Show &Toolbar"), this );
  viewToolBar->setCheckable( true );
  viewToolBar->setChecked( true );
  connect( viewToolBar, &QAction::triggered, this, &KomportApp::slotViewToolBar );
  viewToolBar->setStatusTip( tr("Show/hide the toolbar") );

  viewStatusBar = new QAction( tr("Show &Statusbar"), this );
  viewStatusBar->setCheckable( true );
  viewStatusBar->setChecked( true );
  connect( viewStatusBar, &QAction::triggered, this, &KomportApp::slotViewStatusBar );
  viewStatusBar->setStatusTip( tr("Show/hide the status bar") );

  // Renamed from "&Connection Settings..." (2026, user report: Appearance
  // tab, added in Milestone 5, wasn't discoverable under that label since
  // it no longer only covers the connection - the dialog now has Device/
  // Terminal/Appearance tabs) - see TODO.md section 0.9 addendum.
  showPreferences = new QAction( QIcon::fromTheme(QStringLiteral("preferences-system")), tr("&Settings..."), this );
  showPreferences->setShortcut( QKeySequence::Preferences );
  connect( showPreferences, &QAction::triggered, this, &KomportApp::slotShowPreferences );
  showPreferences->setStatusTip( tr("Connection, terminal and appearance settings") );

  viewHexMonitor = new QAction( QIcon::fromTheme(QStringLiteral("format-text-code")), tr("&Hex Monitor"), this );
  viewHexMonitor->setCheckable( true );
  // triggered (not toggled), matching viewToolBar/viewStatusBar below: it
  // only fires on actual user interaction, not on the setChecked() calls
  // readOptions() makes at startup (which call slotViewHexMonitor()
  // explicitly anyway) - toggled would fire from both, applying the same
  // visibility twice.
  connect( viewHexMonitor, &QAction::triggered, this, &KomportApp::slotViewHexMonitor );
  viewHexMonitor->setStatusTip( tr("Show raw RX/TX bytes as hex") );

  recordSession = new QAction( QIcon::fromTheme(QStringLiteral("media-record")), tr("&Record Session..."), this );
  recordSession->setCheckable( true );
  connect( recordSession, &QAction::toggled, this, &KomportApp::slotToggleRecording );
  recordSession->setStatusTip( tr("Log the session to a file") );

  profileSave = new QAction( QIcon::fromTheme(QStringLiteral("document-save")), tr("Save Profile"), this );
  connect( profileSave, &QAction::triggered, this, &KomportApp::slotSaveProfile );
  profileSave->setStatusTip( tr("Save as this profile") );

  profileDelete = new QAction( QIcon::fromTheme(QStringLiteral("edit-delete")), tr("Delete Profile"), this );
  connect( profileDelete, &QAction::triggered, this, &KomportApp::slotDeleteProfile );
  profileDelete->setStatusTip( tr("Delete this profile") );
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
  viewMenu->addSeparator();
  viewMenu->addAction( viewHexMonitor );

  QMenu *sessionMenu = menuBar()->addMenu( tr("&Session") );
  sessionMenu->addAction( recordSession );

  QMenu *settingsMenu = menuBar()->addMenu( tr("&Settings") );
  settingsMenu->addAction( showPreferences );

  rebuildRecentFilesMenu();
}

void KomportApp::initToolBar()
{
  mainToolBar = addToolBar( tr("Main Toolbar") );
  mainToolBar->setObjectName( QStringLiteral("mainToolBar") );
  // A larger, explicit icon size and icon-only buttons read as a modern
  // toolbar under current desktop themes (Breeze etc.) - the previous
  // default (whatever QStyle picks, typically 16px) looked dated next to
  // the rest of the UI.
  mainToolBar->setIconSize( QSize(24, 24) );
  mainToolBar->setToolButtonStyle( Qt::ToolButtonIconOnly );
  mainToolBar->addAction( fileOpen );
  mainToolBar->addAction( fileSave );
  mainToolBar->addAction( filePrint );
  mainToolBar->addSeparator();
  mainToolBar->addAction( editCut );
  mainToolBar->addAction( editCopy );
  mainToolBar->addAction( editPaste );
  mainToolBar->addSeparator();
  mainToolBar->addAction( viewHexMonitor );
  mainToolBar->addAction( recordSession );
  mainToolBar->addSeparator();

  mainToolBar->addWidget( new QLabel( tr(" Profile: "), mainToolBar ) );
  profileCombo = new QComboBox( mainToolBar );
  profileCombo->setEditable( true );
  profileCombo->setInsertPolicy( QComboBox::NoInsert ); // typing a name doesn't add it to the list - Save does
  profileCombo->setMinimumContentsLength( 14 );
  profileCombo->setToolTip( tr("Device profile: pick one to load it, or type a new\n"
                                "name and click Save to create it.") );
  // textActivated (not currentTextChanged/currentIndexChanged): only fires
  // on an actual user pick from the dropdown, never while typing a new
  // name or from the setCurrentIndex()/setCurrentText() calls this class
  // makes itself while populating/selecting programmatically.
  connect( profileCombo, &QComboBox::textActivated, this, &KomportApp::loadProfile );
  // typing a new name and pressing Enter saves it, same as clicking profileSave
  connect( profileCombo->lineEdit(), &QLineEdit::returnPressed, this, &KomportApp::slotSaveProfile );
  mainToolBar->addWidget( profileCombo );
  mainToolBar->addAction( profileSave );
  mainToolBar->addAction( profileDelete );
  mainToolBar->addSeparator();

  mainToolBar->addWidget( new QLabel( tr(" Enter sends: "), mainToolBar ) );
  lineEndingCombo = new QComboBox( mainToolBar );
  lineEndingCombo->addItems( { QStringLiteral("CR"), QStringLiteral("LF"), QStringLiteral("CR+LF") } );
  lineEndingCombo->setToolTip( tr("What the Return key (and quick-command buttons) send at end of line - "
                                   "some gear only understands a bare CR, Unix hosts usually expect LF.") );
  connect( lineEndingCombo, &QComboBox::currentTextChanged, this, &KomportApp::slotLineEndingChanged );
  mainToolBar->addWidget( lineEndingCombo );

  // Drive toolbar-icon hover hints through hoverHintLabel (see komport.h)
  // instead of statusBar()->showMessage(): showMessage()'s geometry
  // recomputes lazily (a posted, deferred QEvent::LayoutRequest), which
  // could in theory clip a message against stale geometry.
  // hoverHintLabel's width doesn't depend on its text (QSizePolicy::Ignored),
  // so plain setText() here never needs a relayout in the first place.
  //
  // The actually-reported truncation ("individual hovers fine, sweeping
  // icon-to-icon truncated, leaving the toolbar and coming back in fine
  // again") turned out not to be that at all, though - that exact pattern
  // is the signature of a well-known Qt/Plasma quirk where the *native*
  // QToolTip popup reuses the previous tooltip window's geometry when
  // transitioning directly between two adjacent widgets' tooltips with no
  // gap in between, clipping the new (possibly longer) text against the
  // old, narrower size; leaving and re-entering forces a fresh popup.
  // hoverHintLabel was never involved.
  //
  // Rather than just suppressing the native popup, give each button an
  // explicit tooltip (the same descriptive statusTip() text hoverHintLabel
  // shows, nicer than the terse default action text()) and take over
  // showing it ourselves in eventFilter() - explicitly hiding any existing
  // tooltip before showing the new one forces a clean, correctly-sized
  // popup every time instead of a stale, reused one.
  for ( QAction *action : mainToolBar->actions() ) {
    if ( action->statusTip().isEmpty() ) continue;
    connect( action, &QAction::hovered, this, [this, action]{
      hoverHintLabel->setText( action->statusTip() );
    } );
    if ( QWidget *button = mainToolBar->widgetForAction(action) ) {
      button->setToolTip( action->statusTip() );
      button->installEventFilter(this); // re-shows the tooltip fresh, see eventFilter()
    }
  }
  // and reset back to "Ready." once the mouse leaves the toolbar entirely
  // (see eventFilter()) - icon-to-icon moves are handled directly above,
  // this only catches leaving the last icon with nothing else to enter.
  mainToolBar->installEventFilter(this);
}

void KomportApp::initStatusBar()
{
  ///////////////////////////////////////////////////////////////////
  // STATUSBAR

  // Left-hand hover-hint label (see the doc comment on hoverHintLabel in
  // komport.h for why this isn't just statusBar()->showMessage()).
  // QSizePolicy::Ignored horizontally: the layout gives it its stretch
  // share of space up front and never asks it for a size hint again, so
  // setText() here is a plain repaint, never a relayout.
  hoverHintLabel = new QLabel( tr("Ready."), statusBar() );
  hoverHintLabel->setObjectName( QStringLiteral("hoverHintLabel") ); // lets tests find it via findChild()
  hoverHintLabel->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Preferred );
  statusBar()->addWidget( hoverHintLabel, 1 );

  // Permanent widget (right-aligned, stays put regardless of hoverHintLabel's
  // text on the left) showing the active connection at a glance. Kept short
  // on purpose - same reasoning as the shortened setStatusTip() texts above:
  // this window is only as wide as the terminal's fixed character grid.
  connectionStatusLabel = new QLabel( statusBar() );
  statusBar()->addPermanentWidget( connectionStatusLabel );
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

  // The hex monitor sits next to the terminal view in a splitter, hidden
  // by default and toggled by the "Hex Monitor" action/menu item.
  hexView = new KomportHexView(this);
  hexView->setVisible(false);

  centralSplitter = new QSplitter(Qt::Horizontal, this);
  centralSplitter->addWidget(view);
  centralSplitter->addWidget(hexView);
  centralSplitter->setStretchFactor(0, 0); // the terminal view has a fixed cell-grid size
  centralSplitter->setStretchFactor(1, 1); // the hex monitor gets any extra space
  setCentralWidget(centralSplitter);

  setWindowTitle( doc->fileName() );
}

void KomportApp::initMacroBar()
{
  macroBar = new KomportMacroBar(this);
  connect( macroBar, &KomportMacroBar::macroTriggered, this, &KomportApp::slotMacroTriggered );

  addToolBarBreak( Qt::BottomToolBarArea );
  QToolBar *macroToolBar = new QToolBar( tr("Quick Commands"), this );
  macroToolBar->setObjectName( QStringLiteral("macroToolBar") );
  macroToolBar->setMovable( false );
  macroToolBar->addWidget( macroBar );
  addToolBar( Qt::BottomToolBarArea, macroToolBar );
}

/////////////////////////////////////////////////////////////////////
// DEVICE PROFILES
//
// A profile bundles everything a "session" needs to reconnect to a given
// device the same way every time: the serial parameters, the line-ending
// choice, and the macro bar's quick commands. Stored under QSettings group
// "Profiles/<name>/...", with macroBar's own "Macros" group nested inside
// it (KomportMacroBar doesn't need to know about profiles at all - it just
// reads/writes whatever group is currently open on the QSettings object).
/////////////////////////////////////////////////////////////////////

void KomportApp::seedBuiltinProfiles()
{
  struct BuiltinMacro { QString label; QString command; };
  struct BuiltinProfile {
    QString name;
    QString baudRate, dataBits, stopBits, parity, flowControl;
    QList<BuiltinMacro> macros;
  };

  // Well-known serial-console defaults, one preset per common platform, so
  // there's something useful to start from instead of an empty combo.
  // Baud rate and exact CLI command syntax can still vary by exact model/
  // firmware version - these are a starting point, not gospel. Edit them
  // (right-click a macro button, or Settings) and hit Save Profile to make
  // them yours, or Delete Profile if you don't want a preset at all.
  const QList<BuiltinProfile> builtins = {
    {
      QStringLiteral("Cisco (9600 8N1)"),
      QStringLiteral("9600"), QStringLiteral("8"), QStringLiteral("1"), QStringLiteral("NONE"), QStringLiteral("NONE"),
      {
        { tr("Show Config"), QStringLiteral("show running-config") },
        { tr("Show Version"), QStringLiteral("show version") },
        { tr("Save (wr mem)"), QStringLiteral("write memory") },
        { tr("Exit"), QStringLiteral("exit") },
      }
    },
    {
      // HP 1920 & 1950 (confirmed 38400 by the user on real 1920 hardware -
      // NOT the more common 9600 default most other vendors/lines use).
      // The wider HPE Comware/H3C-derived switch line (older 5130/5510
      // etc.) shares this CLI dialect but may use a different baud rate -
      // check the specific model.
      // NOTE: no '/' in this name - QSettings treats '/' as a group-path
      // separator even inside a single beginGroup() argument, which would
      // silently split "1920/1950" into two nested groups instead of one
      // profile (profileNames()/loadProfile()/etc. all assume one flat
      // group level per profile name). Same restriction applies to any
      // profile name a user types in profileCombo - see slotSaveProfile().
      QStringLiteral("HP 1920 & 1950 (38400 8N1)"),
      QStringLiteral("38400"), QStringLiteral("8"), QStringLiteral("1"), QStringLiteral("NONE"), QStringLiteral("NONE"),
      {
        { tr("Show Config"), QStringLiteral("display current-configuration") },
        { tr("Show Version"), QStringLiteral("display version") },
        { tr("Save"), QStringLiteral("save") },
        { tr("Quit"), QStringLiteral("quit") },
      }
    },
    {
      // Newer Aruba-branded HPE switches (CX series - 6100/6300/6400/8xxx).
      // ArubaOS-CX deliberately uses Cisco-like command syntax, but many CX
      // models boot their console at a higher default baud rate than the
      // classic 9600 - double-check your exact model's installation guide,
      // some lines still default to 9600.
      QStringLiteral("Aruba CX (115200 8N1)"),
      QStringLiteral("115200"), QStringLiteral("8"), QStringLiteral("1"), QStringLiteral("NONE"), QStringLiteral("NONE"),
      {
        { tr("Show Config"), QStringLiteral("show running-config") },
        { tr("Show Version"), QStringLiteral("show version") },
        { tr("Save (wr mem)"), QStringLiteral("write memory") },
        { tr("Exit"), QStringLiteral("exit") },
      }
    },
  };

  // Tracked per preset *name* (not a single "already seeded" flag): lets a
  // later code update fix/rename/add a preset and have it actually reach
  // installs that already ran seeding once, without resurrecting presets
  // the user deliberately deleted. A name only gets skipped if it's either
  // a profile the user already has (own or previously-seeded-and-kept), or
  // one that was seeded before and is gone now (i.e. deleted on purpose).
  QStringList everSeeded = config->value( QStringLiteral("SeededProfileNames") ).toStringList();
  const QStringList existing = profileNames();
  bool changed = false;

  for ( const BuiltinProfile &bp : builtins ) {
    if ( existing.contains(bp.name) ) continue;   // already have a profile by this name
    if ( everSeeded.contains(bp.name) ) continue;  // was offered before and is gone now - respect that

    config->beginGroup( QStringLiteral("Profiles") );
    config->beginGroup( bp.name );
    config->setValue( QStringLiteral("Device"), QStringLiteral("/dev/ttyUSB0") ); // just a common starting point - pick the real port from the dropdown/Settings
    config->setValue( QStringLiteral("BaudRate"), bp.baudRate );
    config->setValue( QStringLiteral("FlowControl"), bp.flowControl );
    config->setValue( QStringLiteral("RXQueue"), QStringLiteral("1024") );
    config->setValue( QStringLiteral("FlushRate"), QStringLiteral("256") );
    config->setValue( QStringLiteral("StartBits"), QStringLiteral("1") );
    config->setValue( QStringLiteral("DataBits"), bp.dataBits );
    config->setValue( QStringLiteral("StopBits"), bp.stopBits );
    config->setValue( QStringLiteral("Parity"), bp.parity );
    config->setValue( QStringLiteral("Emulation"), QStringLiteral("VT102") );
    config->setValue( QStringLiteral("ScrollBuffer"), QStringLiteral("1024") );
    config->setValue( QStringLiteral("LineEnding"), QStringLiteral("CR") );
    config->setValue( QStringLiteral("Charset"), KomportCharset::settingsKey(KomportCharset::Standard) ); // Milestone 7
    config->beginGroup( QStringLiteral("Macros") );
    for ( int i = 0; i < bp.macros.size() && i < 8; ++i ) { // 8 == KomportMacroBar::SlotCount
      const QString key = QStringLiteral("Slot%1").arg(i);
      config->setValue( key + QStringLiteral("/Label"), bp.macros.at(i).label );
      config->setValue( key + QStringLiteral("/Command"), bp.macros.at(i).command );
    }
    config->endGroup(); // Macros
    config->endGroup(); // <profile name>
    config->endGroup(); // Profiles

    everSeeded << bp.name;
    changed = true;
  }

  if ( changed ) {
    config->setValue( QStringLiteral("SeededProfileNames"), everSeeded );
    config->sync();
  }
}

void KomportApp::initProfiles()
{
  QStringList names = profileNames();
  if ( names.isEmpty() ) {
    // First run under the profile feature (or a genuinely fresh install):
    // seed a "Default" profile from whatever flat, pre-profile settings
    // exist under the old "Connection"/"Macros"/"LineEnding" keys (or
    // their built-in defaults, for a fresh install), so nothing from an
    // older config is lost and there is always at least one profile to
    // fall back to.
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
    strLineEnding = config->value( QStringLiteral("LineEnding"), QStringLiteral("CR") ).toString();
    strCharset = config->value( QStringLiteral("Charset"), QStringLiteral("Standard") ).toString();
    config->endGroup();
    macroBar->loadSettings(config); // reads the old flat top-level "Macros" group, if any

    saveProfile( QStringLiteral("Default") );
    names << QStringLiteral("Default");
  }

  QString last = config->value( QStringLiteral("LastProfile") ).toString();
  if ( last.isEmpty() || !names.contains(last) ) last = names.first();

  refreshProfileCombo( last );
  loadProfile( last );
}

QStringList KomportApp::profileNames() const
{
  config->beginGroup( QStringLiteral("Profiles") );
  QStringList names = config->childGroups();
  config->endGroup();
  names.sort( Qt::CaseInsensitive );
  return names;
}

void KomportApp::refreshProfileCombo(const QString &_selectName)
{
  const QStringList names = profileNames();
  profileCombo->blockSignals(true);
  profileCombo->clear();
  profileCombo->addItems(names);
  if ( !_selectName.isEmpty() ) {
    int idx = profileCombo->findText(_selectName);
    if ( idx >= 0 ) profileCombo->setCurrentIndex(idx);
    else profileCombo->setCurrentText(_selectName);
  }
  profileCombo->blockSignals(false);
}

void KomportApp::saveProfile(const QString &_name)
{
  config->beginGroup( QStringLiteral("Profiles") );
  config->beginGroup( _name );
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
  config->setValue( QStringLiteral("LineEnding"), strLineEnding );
  config->setValue( QStringLiteral("Charset"), strCharset );
  macroBar->saveSettings(config); // ends up nested under Profiles/<name>/Macros
  hexView->saveSettings(config);  // ends up nested under Profiles/<name>/HexMonitor
  view->saveSettings(config);     // ends up nested under Profiles/<name>/Appearance (Milestone 5)
  config->endGroup();
  config->endGroup();

  config->setValue( QStringLiteral("LastProfile"), _name );
  config->sync();
  mCurrentProfile = _name;
}

void KomportApp::applyConnectionSettings()
{
  // strScrollBuffer comes straight from QSettings (see loadProfile()) and is
  // only range-checked when it goes through the settings dialog's spinbox
  // (0..4096, see settingsdialog.cpp). A hand-edited config file can still
  // contain a negative or absurdly large value; clamp to the same range
  // here so a bad profile can't crash KomportCellArray::setArraySize() or
  // balloon its memory use.
  view->setScrollBuffer( qBound( 0, strScrollBuffer.toInt(), 4096 ) );
  KomportSerial* serial = view->getSerial();
  // Cleanly disconnect first: a profile switch commonly means switching to
  // a completely different device, so always close/reapply/reopen rather
  // than relying on setDeviceName()'s "only reconnect if it actually
  // changed" shortcut (that one's still used by slotShowPreferences() for
  // in-place tweaks, where preserving the connection is nicer).
  serial->close();
  serial->setDeviceName( strDevice );
  serial->setFraming( strStartBits, strDataBits, strStopBits, strParity );
  serial->setFlowControl( strFlowControl );
  serial->setBaudRate( strBaudRate );
  serial->setRxQueue( strRxQueue.toInt() );
  serial->setFlushRate( strFlushRate.toInt() );
  serial->open();

  updateConnectionStatusLabel();
}

/** one-letter parity code for the "8N1"-style summary in the status bar */
static QChar parityLetter(const QString &_parity)
{
  if ( _parity.compare( QLatin1String("EVEN"), Qt::CaseInsensitive ) == 0 ) return QLatin1Char('E');
  if ( _parity.compare( QLatin1String("ODD"), Qt::CaseInsensitive ) == 0 ) return QLatin1Char('O');
  return QLatin1Char('N');
}

void KomportApp::updateConnectionStatusLabel()
{
  const QString framing = strDataBits + parityLetter(strParity) + strStopBits; // e.g. "8N1"
  connectionStatusLabel->setText(
      QStringLiteral("%1  ·  %2 %3  ·  Enter: %4")
          .arg( strDevice.isEmpty() ? tr("(no device)") : strDevice, strBaudRate, framing, strLineEnding ) );
  connectionStatusLabel->setToolTip(
      tr("Profile: %1\nDevice: %2\nBaud rate: %3\nFraming: %4 (data bits/parity/stop bits)\nFlow control: %5\nEnter sends: %6")
          .arg( mCurrentProfile.isEmpty() ? tr("(none)") : mCurrentProfile, strDevice, strBaudRate, framing, strFlowControl, strLineEnding ) );
}

void KomportApp::reconcileCharsetSelectionAfterReload()
{
  // Codex review finding: a custom charset the *live* session already has
  // selected can have its file deleted or renamed since - already handled
  // safely for actual RX/TX (KomportCharset::toDisplay()/toWire() degrade
  // to Standard's identity behavior for an unknown id), but that alone
  // left the *stored* selection itself dangling: view->mEmulation stayed
  // reporting charset()==Custom with a now-nonexistent customCharsetId(),
  // slotShowPreferences()'s combo-box pre-fill would then also fail to
  // find it (a plain "leave the combo at its own default" rather than an
  // honest reset), and saving the profile as-is would re-persist the same
  // broken reference - worse, a future file that happened to reuse that
  // exact id would silently reactivate without the user ever re-selecting
  // anything. Call this right after every KomportCharset::reloadCustomCharsets()
  // to close all of that at the source.
  if ( view->mEmulation->charset() != KomportCharset::Custom ) return;
  bool stillLoaded = false;
  for ( const auto &entry : KomportCharset::customCharsetEntries() ) {
    if ( entry.first == view->mEmulation->customCharsetId() ) { stillLoaded = true; break; }
  }
  if ( !stillLoaded ) {
    view->mEmulation->setCharset( KomportCharset::Standard );
    strCharset = KomportCharset::settingsKey( KomportCharset::Standard );
  }
}

void KomportApp::reconcileCharsetSelectionAfterReloadForAllWindows()
{
  // Same QApplication::topLevelWidgets() + qobject_cast<KomportApp*> walk
  // already used by slotFileQuit() to reach every open top-level window.
  const QWidgetList windows = QApplication::topLevelWidgets();
  for ( QWidget *w : windows ) {
    if ( auto *win = qobject_cast<KomportApp*>(w) ) {
      win->reconcileCharsetSelectionAfterReload();
    }
  }
}

void KomportApp::loadProfile(const QString &_name)
{
  if ( _name.isEmpty() ) return;
  config->beginGroup( QStringLiteral("Profiles") );
  config->beginGroup( _name );
  if ( config->childKeys().isEmpty() && config->childGroups().isEmpty() ) {
    // nothing actually stored under this name (e.g. stale combo entry) -
    // bail out rather than applying empty/default-constructed settings
    config->endGroup();
    config->endGroup();
    return;
  }
  strDevice = config->value( QStringLiteral("Device"), strDevice ).toString();
  strBaudRate = config->value( QStringLiteral("BaudRate"), strBaudRate ).toString();
  strFlowControl = config->value( QStringLiteral("FlowControl"), strFlowControl ).toString();
  strRxQueue = config->value( QStringLiteral("RXQueue"), strRxQueue ).toString();
  strFlushRate = config->value( QStringLiteral("FlushRate"), strFlushRate ).toString();
  strStartBits = config->value( QStringLiteral("StartBits"), strStartBits ).toString();
  strDataBits = config->value( QStringLiteral("DataBits"), strDataBits ).toString();
  strStopBits = config->value( QStringLiteral("StopBits"), strStopBits ).toString();
  strParity = config->value( QStringLiteral("Parity"), strParity ).toString();
  strEmulation = config->value( QStringLiteral("Emulation"), strEmulation ).toString();
  strScrollBuffer = config->value( QStringLiteral("ScrollBuffer"), strScrollBuffer ).toString();
  strLineEnding = config->value( QStringLiteral("LineEnding"), strLineEnding ).toString();
  // Codex review finding (same load-order-leakage class Milestone 5 fixed
  // for Appearance settings): falling back to the *current* strCharset
  // here meant a profile with no "Charset" key (an old profile predating
  // Milestone 7, or - until the fix above - a freshly seeded built-in
  // one) silently inherited whatever charset a *previously* loaded
  // profile happened to have, and saving it afterwards would even
  // persist that leaked value. Falls back to the fixed "Standard"
  // baseline instead, regardless of what was active before.
  strCharset = config->value( QStringLiteral("Charset"), KomportCharset::settingsKey(KomportCharset::Standard) ).toString();
  macroBar->loadSettings(config); // reads Profiles/<name>/Macros
  hexView->loadSettings(config);  // reads Profiles/<name>/HexMonitor
  view->loadSettings(config);     // reads Profiles/<name>/Appearance, applies immediately (Milestone 5)
  config->endGroup();
  config->endGroup();

  mCurrentProfile = _name;
  config->setValue( QStringLiteral("LastProfile"), _name );

  mSerialErrorPending = false; // see slotSerialSettingsFailed()
  applyConnectionSettings();
  lineEndingCombo->setCurrentText( strLineEnding ); // triggers slotLineEndingChanged() if it actually changed
  // Milestone 7: applied directly (not through a signal-driven toolbar
  // widget like lineEndingCombo above) so it's unconditional - no risk of
  // silently no-op'ing just because the new profile happens to already
  // match whatever the previous one had selected. resolveSettingsKey()
  // covers both the three built-ins and any loaded custom charset (see
  // KomportCharset::Selection) - a profile referencing a custom charset
  // whose file has since been deleted/renamed gracefully falls back to
  // Standard rather than crashing or guessing.
  {
    const KomportCharset::Selection sel = KomportCharset::resolveSettingsKey( strCharset );
    view->mEmulation->setCharset( sel.id, sel.customId );
    // Codex review round-3 finding: resolveSettingsKey() already falls
    // back the *live emulation* to Standard for an unrecognized/dangling
    // Charset value (a stale custom-charset id whose file is gone), but
    // strCharset itself - the string that gets *re-persisted* if this
    // profile is saved again - was left holding the original, still-
    // dangling raw value. Normalizing it here too closes that: a Custom
    // selection keeps its own customId as strCharset (matching how it's
    // stored/read everywhere else), anything else collapses to its fixed
    // settingsKey() - which is a no-op for an already-valid built-in
    // value, and only actually changes anything for the dangling case.
    strCharset = ( sel.id == KomportCharset::Custom ) ? sel.customId : KomportCharset::settingsKey( sel.id );
  }

  refreshProfileCombo( _name );
  // Don't stomp on a serial error applyConnectionSettings() may have just
  // reported (e.g. the profile's device doesn't exist) with our own
  // unconditional success message. Unlike slotShowPreferences() below, no
  // extra reconciliation against serial->isOpen() is needed here:
  // applyConnectionSettings() always closes the port before touching any
  // setting, so its single open() call at the end is the only thing that
  // can set mSerialErrorPending - there's no earlier, transient,
  // partially-updated-combination failure to second-guess.
  if ( !mSerialErrorPending ) {
    slotStatusMsg( tr("Loaded profile \"%1\"").arg(_name) );
  }
}

void KomportApp::slotSaveProfile()
{
  const QString name = profileCombo->currentText().trimmed();
  if ( name.isEmpty() ) {
    QMessageBox::warning( this, tr("Save Profile"), tr("Please enter a profile name first.") );
    return;
  }
  if ( name.contains(QLatin1Char('/')) ) {
    // QSettings treats '/' as a group-path separator even inside a single
    // beginGroup() argument, which would silently split the profile across
    // two nested groups instead of storing it as one - reject rather than
    // silently mangling it.
    QMessageBox::warning( this, tr("Save Profile"), tr("Profile names can't contain \"/\" - please remove it.") );
    return;
  }
  saveProfile(name);
  refreshProfileCombo(name);
  slotStatusMsg( tr("Saved profile \"%1\"").arg(name) );
}

void KomportApp::slotDeleteProfile()
{
  const QString name = profileCombo->currentText().trimmed();
  if ( name.isEmpty() || !profileNames().contains(name) ) {
    QMessageBox::information( this, tr("Delete Profile"), tr("\"%1\" is not a saved profile.").arg(name) );
    return;
  }
  if ( QMessageBox::question( this, tr("Delete Profile"), tr("Delete profile \"%1\"? This cannot be undone.").arg(name) )
       != QMessageBox::Yes ) {
    return;
  }

  config->beginGroup( QStringLiteral("Profiles") );
  config->remove( name );
  config->endGroup();
  config->sync();

  if ( mCurrentProfile == name ) mCurrentProfile.clear();
  refreshProfileCombo();
  slotStatusMsg( tr("Deleted profile \"%1\"").arg(name) );
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
  // Connection/framing/line-ending/macro settings are no longer saved here
  // - they live per-profile now (see saveProfile()/loadProfile()) and are
  // only ever written when the user explicitly saves a profile, not
  // silently on every window close.
  config->beginGroup( QStringLiteral("General Options") );
  config->setValue( QStringLiteral("Geometry"), size() );
  config->setValue( QStringLiteral("Show Toolbar"), viewToolBar->isChecked() );
  config->setValue( QStringLiteral("Show Statusbar"), viewStatusBar->isChecked() );
  config->setValue( QStringLiteral("Show Hex Monitor"), viewHexMonitor->isChecked() );
  QStringList recent;
  for ( const QUrl &url : std::as_const(mRecentFiles) ) recent << url.toString();
  config->setValue( QStringLiteral("Recent Files"), recent );
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

  bool bViewHexMonitor = config->value( QStringLiteral("Show Hex Monitor"), false ).toBool();
  viewHexMonitor->setChecked(bViewHexMonitor);
  slotViewHexMonitor(bViewHexMonitor);

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

  // Connection/framing/line-ending/macro settings are handled by
  // initProfiles() (called right after this), not here anymore.
}

void KomportApp::closeEvent(QCloseEvent *event)
{
  if ( doc->saveModified() ) {
    saveOptions();
    // Close the port here, once, for every close path (window-manager
    // close button, File > Close, File > Quit all end up here) -
    // slotFileClose() used to close it itself before calling close(), but
    // that meant serial teardown depended on which path was used.
    // KomportSerial::close() is safe to call again regardless.
    view->getSerial()->close();
    event->accept();
  } else {
    event->ignore();
  }
}

bool KomportApp::eventFilter(QObject *watched, QEvent *event)
{
  // Re-show the native tooltip ourselves instead of letting Qt's default
  // handling do it - this filter is only ever installed on mainToolBar
  // itself and on its icon buttons (see initToolBar()), so it's safe to do
  // this unconditionally for every QEvent::ToolTip it sees. Explicitly
  // hiding any currently-shown tooltip first forces a fresh, correctly-
  // sized popup: Qt can otherwise reuse the previous tooltip window's
  // cached geometry when sweeping directly between two adjacent widgets'
  // tooltips with no gap in between, clipping the new (possibly longer)
  // text against the old, narrower size - this was the actual cause of
  // the reported icon-to-icon truncation (confirmed via the user's own
  // screen recording, see TODO-ARCHIVE.md section 18).
  if ( event->type() == QEvent::ToolTip ) {
    if ( auto *w = qobject_cast<QWidget*>(watched); w && !w->toolTip().isEmpty() ) {
      auto *he = static_cast<QHelpEvent*>(event);
      QToolTip::hideText();
      QToolTip::showText( he->globalPos(), w->toolTip(), w );
    }
    return true; // handled either way - never fall through to Qt's own reused-popup path
  }
  // Reset the hover hint back to "Ready." once the mouse leaves the
  // toolbar entirely - moving from one icon straight to an adjacent one
  // never reaches this (each icon's own QAction::hovered() connection in
  // initToolBar() overwrites hoverHintLabel directly), so there's no
  // flicker in between; this only covers "left the last icon with nothing
  // else to enter".
  if ( watched == mainToolBar && event->type() == QEvent::Leave ) {
    hoverHintLabel->setText( tr("Ready.") );
  }
  return QMainWindow::eventFilter(watched, event);
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
      // Only remember it as a recent file if the upload actually
      // completed - a cancelled or failed transfer (transfer.upload()
      // already shows its own QMessageBox for a real error) previously
      // still landed in the recent-files list exactly as if it had
      // succeeded.
      if ( transfer.upload() ) {
        addRecentFile( QUrl::fromLocalFile(fileName) );
      }
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
      if ( transfer.upload() ) {
        addRecentFile( url );
      }
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
      // Only remember it as a recent file if the download actually
      // completed (Ctrl-D received) - previously a cancelled or failed
      // download landed in the recent-files list exactly as if it had
      // succeeded, since download() always returned true.
      if ( transfer.download() ) {
        addRecentFile( QUrl::fromLocalFile(fileName) );
      }
    }
  slotStatusMsg(tr("Ready."));
}

void KomportApp::slotFileClose()
{
  slotStatusMsg(tr("Closing file..."));

  // Do not touch `this` after a close() that succeeds: with
  // Qt::WA_DeleteOnClose set (see the constructor), an accepted close()
  // means this object is on its way out - Qt's own documentation for
  // WA_DeleteOnClose only promises deletion on an accepted close, not that
  // the widget stays usable afterwards within the same call. Only run the
  // trailing status update for the case where the user (or saveModified())
  // aborted the close, i.e. this window is still here.
  if ( !close() ) {
    slotStatusMsg(tr("Ready."));
  }
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
      // view->selectedText(), not QClipboard::Selection readback: the
      // latter (X11 "primary selection") isn't available on every
      // platform - Wayland without the primary-selection protocol,
      // Windows, macOS, and the "offscreen" QPA platform this project's
      // own tests run under all lack it, which silently copied nothing
      // there even though a selection was clearly visible on screen.
      // KomportView::select() still also writes to QClipboard::Selection
      // where it *is* supported (middle-click paste), independently of
      // this.
      QApplication::clipboard()->setText( view->selectedText(), QClipboard::Clipboard );
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
  // Reset here, at the very start - not just before the settings are
  // applied below - so an error reported while the modal dialog itself is
  // still open (a device disconnect event, say: settingsDialog.exec()
  // still runs a nested event loop) also survives to be seen, rather than
  // getting silently cleared by a stale flag from some earlier, unrelated
  // failure or overwritten by this method's own trailing "Ready.".
  mSerialErrorPending = false;
  // Milestone 7 addendum: re-scan for custom *.charset files every time
  // this dialog opens (cheap - typically a handful of small files), so a
  // file the user just dropped into customCharsetsDirectory() shows up
  // in the dropdown below without needing to restart the app.
  KomportCharset::reloadCustomCharsets();
  // Codex review round-3 finding: KomportCharset's custom-charset registry
  // is a single process-wide, function-local static (see its own comment)
  // shared by *every* open KomportApp window (slotFileNewWindow() can
  // create more than one) - reconciling only the window that happens to be
  // opening Settings left any *other* open window's live Custom selection
  // dangling against the now-reloaded (and possibly no-longer-matching)
  // registry until *it* also happened to open Settings. Reconciling every
  // open window right after the one shared reload keeps them all
  // consistent with each other, not just with whichever window triggered it.
  reconcileCharsetSelectionAfterReloadForAllWindows();
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
  // Codex review finding: read/write via the combo's own Qt::UserRole data
  // (findData()/currentData()) rather than row-index<->enum-value
  // coupling - see KomportCharset::displayEntries()'s comment. The
  // payload is the settingsKey()-style string (not the bare Id) since
  // Milestone 7's custom-charset addendum: every loaded custom charset
  // shares Id::Custom, so only the string (built-in name, or a custom
  // charset's own id) actually identifies a unique dropdown row.
  {
    const QString currentKey = ( view->mEmulation->charset() == KomportCharset::Custom )
        ? view->mEmulation->customCharsetId()
        : KomportCharset::settingsKey( view->mEmulation->charset() );
    const int idx = settingsDialog.CharsetComboBox->findData( currentKey );
    if ( idx >= 0 ) settingsDialog.CharsetComboBox->setCurrentIndex( idx );
  }
  settingsDialog.setSelectedFont( view->font() ); // Milestone 5
  settingsDialog.setColors( view->cellArray()->defaultForegroundColor(), view->cellArray()->defaultBackgroundColor() );
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
      // Milestone 7: font/colors below already apply live and independent
      // of the serial settings, same reasoning applies here - no port
      // re-open, no mSerialErrorPending interplay.
      {
        const QString key = settingsDialog.CharsetComboBox->currentData().toString();
        const KomportCharset::Selection sel = KomportCharset::resolveSettingsKey( key );
        strCharset = key;
        view->mEmulation->setCharset( sel.id, sel.customId );
      }

      view->setScrollBuffer( strScrollBuffer.toInt() );
      KomportSerial* serial = view->getSerial();
      serial->setDeviceName( strDevice );
      serial->setFraming( strStartBits, strDataBits, strStopBits, strParity );
      serial->setFlowControl( strFlowControl );
      // Each of the setters above (setDeviceName()'s internal reopen
      // included) re-applies the *entire current* field combination to an
      // already-open port immediately - so setFraming()/setFlowControl()
      // above can transiently fail on a still-partially-updated
      // combination (new data bits with the still-old flow control, say),
      // a false alarm that has nothing to do with the combination the
      // user actually asked for. setBaudRate() is the last call that
      // touches the port's settings before open() below, so by the time
      // it runs every field already has its final value - resetting the
      // flag immediately before it means only *this* call's result (the
      // true, fully-applied combination) is what's left standing
      // afterwards, discarding the earlier calls' transient noise instead
      // of trusting a weaker proxy like serial->isOpen() (a rejected
      // setting does not necessarily close an already-open port).
      mSerialErrorPending = false; // see slotSerialSettingsFailed()
      serial->setBaudRate( strBaudRate );
      serial->setRxQueue( strRxQueue.toInt() );
      serial->setFlushRate( strFlushRate.toInt() );
      // If the port wasn't open at all (nothing above actually applied
      // anything, since every setter's re-apply is itself gated on
      // isOpen()), this open() call is what performs the one and only
      // real application attempt, and its own settingsFailed() (via
      // slotPortError()) is what will have the final say on the flag.
      if ( !serial->isOpen() ) serial->open();

      // Milestone 5: font/colors apply live, independent of the serial
      // settings above (no port re-open, no mSerialErrorPending interplay -
      // these never touch the connection at all).
      view->setTerminalFont( settingsDialog.selectedFont() );
      view->setDefaultColors( settingsDialog.foregroundColor(), settingsDialog.backgroundColor() );

      // Persist the tweak into the active profile, so it isn't silently
      // lost the next time this profile is (re)loaded or the app restarts.
      if ( !mCurrentProfile.isEmpty() ) saveProfile( mCurrentProfile );

      updateConnectionStatusLabel();
  }

  // Don't stomp on a serial error just reported above (or the dialog was
  // cancelled and mSerialErrorPending is still whatever it was before -
  // false unless a previous, still-unacknowledged error is pending).
  if ( !mSerialErrorPending ) {
    slotStatusMsg(tr("Ready."));
  }
}

void KomportApp::slotStatusMsg(const QString &text)
{
  ///////////////////////////////////////////////////////////////////
  // change status message permanently
  //
  // Through hoverHintLabel, not statusBar()->showMessage(): a temporary
  // showMessage() hides every "normal" status-bar widget while it's active
  // (that's what makes it "temporary"), and this is called constantly -
  // every action prints its own "Ready." here on completion. If this used
  // showMessage(), hoverHintLabel (also a normal widget) would spend nearly
  // all its time hidden behind whatever this last printed. Qt's own
  // automatic menu-hover status tips still go through the real
  // showMessage() and briefly overlay hoverHintLabel while a menu is open -
  // that's fine, it already works and reverts on its own once the menu
  // closes.
  hoverHintLabel->setText(text);
}

/** Document has changed.  */
void KomportApp::slotDocumentModified(){
}
void KomportApp::slotSerialSettingsFailed(const QString &_reason){
  // Previously this only reached qWarning() (see KomportSerial::
  // slotPortError()/applyPortSettings()) - a device that's unplugged,
  // permission-denied, or given an unsupported baud/framing combination
  // failed silently as far as the UI was concerned. slotStatusMsg() rather
  // than a modal QMessageBox: settingsFailed() can fire repeatedly for a
  // flaky connection, and a stream of blocking dialogs would be far worse
  // than a status-bar message that's simply overwritten by the next one.
  //
  // mSerialErrorPending: this can fire synchronously from inside
  // loadProfile()/slotShowPreferences() while they're still applying the
  // new settings (KomportSerial::open() et al. can emit settingsFailed()
  // directly) - without this flag, their own unconditional trailing
  // "Loaded profile ..."/"Ready." message would immediately overwrite the
  // error we just showed here, and the user would never actually see it.
  mSerialErrorPending = true;
  slotStatusMsg( tr("Serial port error: %1").arg(_reason) );
  updateConnectionStatusLabel();
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

/** toggles the hex monitor split-screen panel */
void KomportApp::slotViewHexMonitor(bool checked)
{
  hexView->setVisible(checked);
}

/** send a macro bar command (plus the configured line ending) */
void KomportApp::slotMacroTriggered(const QString &command)
{
  KomportSerial *serial = view->getSerial();
  if ( !serial->isOpen() || command.isEmpty() ) return;
  // Milestone 7 (Codex review finding): route each character through the
  // same charset translation typed/pasted input already gets
  // (KomportEmulation::slotKeyPressed()/slotSimKeyPressed()) - macro text
  // used to bypass it entirely via toLocal8Bit(), a locale-dependent
  // encoding with no relation to the selected charset at all, so the same
  // text sent as a macro vs. typed by hand could reach the wire
  // differently.
  const KomportCharset::Id charset = view->mEmulation->charset();
  const QString customCharsetId = view->mEmulation->customCharsetId();
  QByteArray wireBytes;
  wireBytes.reserve( command.size() );
  for ( const QChar &c : command ) wireBytes.append( KomportCharset::toWire(charset, c, customCharsetId) );
  // Codex review finding (round 2, corrected in round 3): putStr()'s
  // null-terminated const char* overload would silently truncate at a
  // genuine embedded NUL (CP437's byte 0x00 is real Unicode NUL since the
  // 0x00-0x7F identity scoping - see KomportCharset). Round 2's fix
  // skipped NUL bytes instead - avoided the truncation, but silently
  // dropped the byte rather than preserving it, a different (smaller,
  // but still real) corruption. KomportSerial::putStr(const char*,
  // qsizetype) - a new, minimal, length-aware overload added for exactly
  // this call site - sends every byte verbatim, embedded NULs included.
  serial->putStr( wireBytes.constData(), wireBytes.size() );
  // Unrelated to the NUL fix above (never charset-translated, never
  // contains a NUL) - left on the null-terminated overload rather than
  // migrated, per instruction to touch only the actually-affected call
  // site.
  serial->putStr( view->mEmulation->lineEndingBytes().constData() );
}

/** toggles session logging - prompts for a file to start, if not already logging */
void KomportApp::slotToggleRecording(bool checked)
{
  if ( checked ) {
    QString fileName = QFileDialog::getSaveFileName( this, tr("Start Session Log..."), QDir::currentPath(),
                                                       tr("Text files (*.log *.txt);;All files (*)") );
    if ( fileName.isEmpty() || !sessionLogger->startLogging(fileName) ) {
      recordSession->blockSignals(true);
      recordSession->setChecked(false);
      recordSession->blockSignals(false);
      if ( !fileName.isEmpty() ) {
        QMessageBox::warning( this, tr("Session Log"), tr("Could not open \"%1\" for writing.").arg(fileName) );
      }
      return;
    }
    slotStatusMsg( tr("Recording session to %1").arg(sessionLogger->fileName()) );
  } else {
    sessionLogger->stopLogging();
    slotStatusMsg( tr("Ready.") );
  }
}

/** apply a line-ending choice ("CR"/"LF"/"CR+LF") from the toolbar dropdown */
void KomportApp::slotLineEndingChanged(const QString &text)
{
  strLineEnding = text;
  KomportEmulation::LineEnding le = KomportEmulation::LineEnding::CR;
  if ( text == QLatin1String("LF") ) le = KomportEmulation::LineEnding::LF;
  else if ( text == QLatin1String("CR+LF") ) le = KomportEmulation::LineEnding::CRLF;
  view->mEmulation->setLineEnding(le);
  updateConnectionStatusLabel();
}
