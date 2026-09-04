#include <klocale.h>
/****************************************************************************
** Form implementation generated from reading ui file './settingsdialog.ui'
**
** Created: Sun Oct 12 05:41:36 2003
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.1.1   edited Nov 21 17:40 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "settingsdialog.h"

#include <qvariant.h>
#include <kurlrequester.h>
#include <qbuttongroup.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qframe.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qspinbox.h>
#include <qtabwidget.h>
#include <qwidget.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>

/* 
 *  Constructs a SettingsDialog as a child of 'parent', with the 
 *  name 'name' and widget flags set to 'f'.
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  TRUE to construct a modal dialog.
 */
SettingsDialog::SettingsDialog( QWidget* parent, const char* name, bool modal, WFlags fl )
    : QDialog( parent, name, modal, fl )

{
    if ( !name )
	setName( "SettingsDialog" );
    setSizeGripEnabled( TRUE );
    SettingsDialogLayout = new QGridLayout( this, 1, 1, 11, 6, "SettingsDialogLayout"); 

    tabWidget = new QTabWidget( this, "tabWidget" );

    Widget2 = new QWidget( tabWidget, "Widget2" );

    BaudRateLabel = new QLabel( Widget2, "BaudRateLabel" );
    BaudRateLabel->setGeometry( QRect( 0, 60, 80, 20 ) );
    BaudRateLabel->setAlignment( int( QLabel::AlignVCenter | QLabel::AlignRight ) );

    textLabel1 = new QLabel( Widget2, "textLabel1" );
    textLabel1->setGeometry( QRect( 30, 20, 50, 20 ) );
    textLabel1->setAlignment( int( QLabel::AlignVCenter | QLabel::AlignRight ) );

    BaudRateComboBox = new QComboBox( FALSE, Widget2, "BaudRateComboBox" );
    BaudRateComboBox->setGeometry( QRect( 90, 60, 170, 30 ) );

    DeviceComboBox = new QComboBox( FALSE, Widget2, "DeviceComboBox" );
    DeviceComboBox->setGeometry( QRect( 90, 20, 170, 30 ) );
    DeviceComboBox->setEditable( TRUE );

    RXQueueGroupBox = new QGroupBox( Widget2, "RXQueueGroupBox" );
    RXQueueGroupBox->setGeometry( QRect( 270, 110, 270, 120 ) );

    RxQueueSpinBox = new QSpinBox( RXQueueGroupBox, "RxQueueSpinBox" );
    RxQueueSpinBox->setGeometry( QRect( 100, 30, 70, 30 ) );
    RxQueueSpinBox->setMaxValue( 32768 );
    RxQueueSpinBox->setMinValue( 1024 );
    RxQueueSpinBox->setLineStep( 1024 );

    FlushRateSpinBox = new QSpinBox( RXQueueGroupBox, "FlushRateSpinBox" );
    FlushRateSpinBox->setGeometry( QRect( 100, 70, 70, 30 ) );
    FlushRateSpinBox->setMaxValue( 4096 );
    FlushRateSpinBox->setLineStep( 16 );
    FlushRateSpinBox->setValue( 256 );

    textLabel2 = new QLabel( RXQueueGroupBox, "textLabel2" );
    textLabel2->setGeometry( QRect( 180, 30, 52, 20 ) );

    textLabel1_2 = new QLabel( RXQueueGroupBox, "textLabel1_2" );
    textLabel1_2->setGeometry( QRect( 40, 30, 52, 20 ) );
    textLabel1_2->setAlignment( int( QLabel::AlignVCenter | QLabel::AlignRight ) );

    textLabel1_3 = new QLabel( RXQueueGroupBox, "textLabel1_3" );
    textLabel1_3->setGeometry( QRect( 10, 70, 80, 20 ) );
    textLabel1_3->setAlignment( int( QLabel::AlignVCenter | QLabel::AlignRight ) );

    textLabel2_2 = new QLabel( RXQueueGroupBox, "textLabel2_2" );
    textLabel2_2->setGeometry( QRect( 180, 70, 78, 20 ) );

    FlowControlComboBox = new QComboBox( FALSE, Widget2, "FlowControlComboBox" );
    FlowControlComboBox->setGeometry( QRect( 380, 20, 130, 30 ) );

    groupBox4 = new QGroupBox( Widget2, "groupBox4" );
    groupBox4->setGeometry( QRect( 40, 100, 190, 150 ) );

    StartBitsComboBox = new QComboBox( FALSE, groupBox4, "StartBitsComboBox" );
    StartBitsComboBox->setGeometry( QRect( 100, 20, 70, 26 ) );

    DataBitsComboBox = new QComboBox( FALSE, groupBox4, "DataBitsComboBox" );
    DataBitsComboBox->setGeometry( QRect( 100, 50, 70, 26 ) );

    StopBitsComboBox = new QComboBox( FALSE, groupBox4, "StopBitsComboBox" );
    StopBitsComboBox->setGeometry( QRect( 100, 80, 70, 26 ) );

    ParityComboBox = new QComboBox( FALSE, groupBox4, "ParityComboBox" );
    ParityComboBox->setGeometry( QRect( 70, 110, 100, 26 ) );

    textLabel3 = new QLabel( groupBox4, "textLabel3" );
    textLabel3->setGeometry( QRect( 12, 80, 70, 20 ) );
    textLabel3->setAlignment( int( QLabel::AlignVCenter | QLabel::AlignRight ) );

    textLabel2_3 = new QLabel( groupBox4, "textLabel2_3" );
    textLabel2_3->setGeometry( QRect( 12, 50, 70, 20 ) );
    textLabel2_3->setAlignment( int( QLabel::AlignVCenter | QLabel::AlignRight ) );

    textLabel1_4 = new QLabel( groupBox4, "textLabel1_4" );
    textLabel1_4->setGeometry( QRect( 10, 20, 70, 20 ) );
    textLabel1_4->setAlignment( int( QLabel::AlignVCenter | QLabel::AlignRight ) );

    textLabel4_2 = new QLabel( groupBox4, "textLabel4_2" );
    textLabel4_2->setGeometry( QRect( 10, 110, 50, 20 ) );
    textLabel4_2->setAlignment( int( QLabel::AlignVCenter | QLabel::AlignRight ) );

    textLabel4 = new QLabel( Widget2, "textLabel4" );
    textLabel4->setGeometry( QRect( 280, 20, 90, 20 ) );
    textLabel4->setAlignment( int( QLabel::AlignVCenter | QLabel::AlignRight ) );
    tabWidget->insertTab( Widget2, "" );

    Widget3 = new QWidget( tabWidget, "Widget3" );

    buttonGroup1 = new QButtonGroup( Widget3, "buttonGroup1" );
    buttonGroup1->setGeometry( QRect( 240, 10, 320, 170 ) );

    FileBufferURLRequeste = new KURLRequester( buttonGroup1, "FileBufferURLRequeste" );
    FileBufferURLRequeste->setEnabled( FALSE );
    FileBufferURLRequeste->setGeometry( QRect( 40, 130, 240, 28 ) );

    FileBufferRadioButton = new QRadioButton( buttonGroup1, "FileBufferRadioButton" );
    FileBufferRadioButton->setEnabled( TRUE );
    FileBufferRadioButton->setGeometry( QRect( 20, 90, 87, 16 ) );

    line1 = new QFrame( buttonGroup1, "line1" );
    line1->setGeometry( QRect( 10, 70, 300, 16 ) );
    line1->setFrameShape( QFrame::HLine );
    line1->setFrameShadow( QFrame::Sunken );
    line1->setFrameShape( QFrame::HLine );

    MemoryBufferRadioButton = new QRadioButton( buttonGroup1, "MemoryBufferRadioButton" );
    MemoryBufferRadioButton->setGeometry( QRect( 20, 30, 120, 20 ) );
    MemoryBufferRadioButton->setChecked( TRUE );

    ScrollBufferSpinBox = new QSpinBox( buttonGroup1, "ScrollBufferSpinBox" );
    ScrollBufferSpinBox->setGeometry( QRect( 140, 30, 80, 30 ) );
    ScrollBufferSpinBox->setMaxValue( 4096 );
    ScrollBufferSpinBox->setMinValue( 0 );
    ScrollBufferSpinBox->setLineStep( 256 );
    ScrollBufferSpinBox->setValue( 1024 );

    textLines1 = new QLabel( buttonGroup1, "textLines1" );
    textLines1->setGeometry( QRect( 230, 30, 70, 20 ) );

    textLines2 = new QLabel( buttonGroup1, "textLines2" );
    textLines2->setEnabled( FALSE );
    textLines2->setGeometry( QRect( 230, 90, 70, 20 ) );

    FileBufferSizeSpinBox = new QSpinBox( buttonGroup1, "FileBufferSizeSpinBox" );
    FileBufferSizeSpinBox->setEnabled( FALSE );
    FileBufferSizeSpinBox->setGeometry( QRect( 140, 90, 80, 30 ) );
    FileBufferSizeSpinBox->setMaxValue( 102400 );
    FileBufferSizeSpinBox->setMinValue( 1024 );
    FileBufferSizeSpinBox->setLineStep( 1024 );
    FileBufferSizeSpinBox->setValue( 4096 );

    groupBox3 = new QGroupBox( Widget3, "groupBox3" );
    groupBox3->setGeometry( QRect( 20, 10, 210, 170 ) );

    EmulationComboBox = new QComboBox( FALSE, groupBox3, "EmulationComboBox" );
    EmulationComboBox->setGeometry( QRect( 30, 30, 140, 30 ) );

    VisualBellCheckBox = new QCheckBox( groupBox3, "VisualBellCheckBox" );
    VisualBellCheckBox->setEnabled( TRUE );
    VisualBellCheckBox->setGeometry( QRect( 30, 80, 100, 20 ) );

    LocalEchoCheckBox = new QCheckBox( groupBox3, "LocalEchoCheckBox" );
    LocalEchoCheckBox->setEnabled( TRUE );
    LocalEchoCheckBox->setGeometry( QRect( 30, 120, 100, 21 ) );
    tabWidget->insertTab( Widget3, "" );

    SettingsDialogLayout->addWidget( tabWidget, 0, 1 );

    Layout1 = new QHBoxLayout( 0, 0, 6, "Layout1"); 

    buttonHelp = new QPushButton( this, "buttonHelp" );
    buttonHelp->setAutoDefault( TRUE );
    Layout1->addWidget( buttonHelp );
    QSpacerItem* spacer = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    Layout1->addItem( spacer );

    buttonOk = new QPushButton( this, "buttonOk" );
    buttonOk->setAutoDefault( TRUE );
    buttonOk->setDefault( TRUE );
    Layout1->addWidget( buttonOk );

    buttonCancel = new QPushButton( this, "buttonCancel" );
    buttonCancel->setAutoDefault( TRUE );
    Layout1->addWidget( buttonCancel );

    SettingsDialogLayout->addMultiCellLayout( Layout1, 1, 1, 0, 1 );
    languageChange();
    resize( QSize(608, 345).expandedTo(minimumSizeHint()) );

    // signals and slots connections
    connect( buttonOk, SIGNAL( clicked() ), this, SLOT( accept() ) );
    connect( buttonCancel, SIGNAL( clicked() ), this, SLOT( reject() ) );
    connect( FileBufferRadioButton, SIGNAL( toggled(bool) ), FileBufferSizeSpinBox, SLOT( setEnabled(bool) ) );
    connect( FileBufferRadioButton, SIGNAL( toggled(bool) ), FileBufferURLRequeste, SLOT( setEnabled(bool) ) );
    connect( MemoryBufferRadioButton, SIGNAL( toggled(bool) ), ScrollBufferSpinBox, SLOT( setEnabled(bool) ) );
    connect( MemoryBufferRadioButton, SIGNAL( toggled(bool) ), textLines1, SLOT( setEnabled(bool) ) );
    connect( FileBufferRadioButton, SIGNAL( toggled(bool) ), textLines2, SLOT( setEnabled(bool) ) );
}

/*
 *  Destroys the object and frees any allocated resources
 */
SettingsDialog::~SettingsDialog()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void SettingsDialog::languageChange()
{
    setCaption( tr2i18n( "Settings" ) );
    BaudRateLabel->setText( tr2i18n( "Baud Rate:" ) );
    textLabel1->setText( tr2i18n( "Device:" ) );
    BaudRateComboBox->clear();
    BaudRateComboBox->insertItem( tr2i18n( "50" ) );
    BaudRateComboBox->insertItem( tr2i18n( "75" ) );
    BaudRateComboBox->insertItem( tr2i18n( "110" ) );
    BaudRateComboBox->insertItem( tr2i18n( "134" ) );
    BaudRateComboBox->insertItem( tr2i18n( "150" ) );
    BaudRateComboBox->insertItem( tr2i18n( "300" ) );
    BaudRateComboBox->insertItem( tr2i18n( "600" ) );
    BaudRateComboBox->insertItem( tr2i18n( "1200" ) );
    BaudRateComboBox->insertItem( tr2i18n( "1800" ) );
    BaudRateComboBox->insertItem( tr2i18n( "2400" ) );
    BaudRateComboBox->insertItem( tr2i18n( "4800" ) );
    BaudRateComboBox->insertItem( tr2i18n( "9600" ) );
    BaudRateComboBox->insertItem( tr2i18n( "19200" ) );
    BaudRateComboBox->insertItem( tr2i18n( "38400" ) );
    BaudRateComboBox->insertItem( tr2i18n( "57600" ) );
    BaudRateComboBox->insertItem( tr2i18n( "115200" ) );
    BaudRateComboBox->insertItem( tr2i18n( "230400" ) );
    BaudRateComboBox->setCurrentItem( 11 );
    QToolTip::add( BaudRateComboBox, tr2i18n( "Baud Rate (bps)" ) );
    DeviceComboBox->clear();
    DeviceComboBox->insertItem( tr2i18n( "/dev/ttyS0" ) );
    DeviceComboBox->insertItem( tr2i18n( "/dev/ttyS1" ) );
    DeviceComboBox->insertItem( tr2i18n( "/dev/ttyS2" ) );
    DeviceComboBox->insertItem( tr2i18n( "/dev/ttyS3" ) );
    QToolTip::add( DeviceComboBox, tr2i18n( "Device special file name" ) );
    RXQueueGroupBox->setTitle( tr2i18n( "RX Queue" ) );
    textLabel2->setText( tr2i18n( "bytes" ) );
    textLabel1_2->setText( tr2i18n( "Size:" ) );
    textLabel1_3->setText( tr2i18n( "Flush Interval:" ) );
    textLabel2_2->setText( tr2i18n( "milliseconds" ) );
    FlowControlComboBox->clear();
    FlowControlComboBox->insertItem( tr2i18n( "XON/XOFF" ) );
    FlowControlComboBox->insertItem( tr2i18n( "RTS/CTS" ) );
    FlowControlComboBox->insertItem( tr2i18n( "NONE" ) );
    FlowControlComboBox->setCurrentItem( 2 );
    groupBox4->setTitle( tr2i18n( "Framing" ) );
    StartBitsComboBox->clear();
    StartBitsComboBox->insertItem( tr2i18n( "1" ) );
    StartBitsComboBox->insertItem( tr2i18n( "2" ) );
    DataBitsComboBox->clear();
    DataBitsComboBox->insertItem( tr2i18n( "5" ) );
    DataBitsComboBox->insertItem( tr2i18n( "6" ) );
    DataBitsComboBox->insertItem( tr2i18n( "7" ) );
    DataBitsComboBox->insertItem( tr2i18n( "8" ) );
    DataBitsComboBox->setCurrentItem( 3 );
    StopBitsComboBox->clear();
    StopBitsComboBox->insertItem( tr2i18n( "1" ) );
    StopBitsComboBox->insertItem( tr2i18n( "1.5" ) );
    StopBitsComboBox->insertItem( tr2i18n( "2" ) );
    ParityComboBox->clear();
    ParityComboBox->insertItem( tr2i18n( "NONE" ) );
    ParityComboBox->insertItem( tr2i18n( "EVEN" ) );
    ParityComboBox->insertItem( tr2i18n( "ODD" ) );
    textLabel3->setText( tr2i18n( "Stop bits:" ) );
    textLabel2_3->setText( tr2i18n( "Data bits:" ) );
    textLabel1_4->setText( tr2i18n( "Start bits:" ) );
    textLabel4_2->setText( tr2i18n( "Parity:" ) );
    textLabel4->setText( tr2i18n( "Flow Control:" ) );
    tabWidget->changeTab( Widget2, tr2i18n( "Device" ) );
    buttonGroup1->setTitle( tr2i18n( "History Buffer" ) );
    FileBufferRadioButton->setText( tr2i18n( "File buffer" ) );
    MemoryBufferRadioButton->setText( tr2i18n( "Memory buffer" ) );
    textLines1->setText( tr2i18n( "lines max." ) );
    textLines2->setText( tr2i18n( "lines max." ) );
    groupBox3->setTitle( tr2i18n( "Emulation" ) );
    EmulationComboBox->clear();
    EmulationComboBox->insertItem( tr2i18n( "VT102" ) );
    VisualBellCheckBox->setText( tr2i18n( "Visual Bell" ) );
    LocalEchoCheckBox->setText( tr2i18n( "Local Echo" ) );
    tabWidget->changeTab( Widget3, tr2i18n( "Terminal" ) );
    buttonHelp->setText( tr2i18n( "&Help" ) );
    buttonHelp->setAccel( QKeySequence( tr2i18n( "F1" ) ) );
    buttonOk->setText( tr2i18n( "&OK" ) );
    buttonOk->setAccel( QKeySequence( QString::null ) );
    buttonCancel->setText( tr2i18n( "&Cancel" ) );
    buttonCancel->setAccel( QKeySequence( QString::null ) );
}

#include "settingsdialog.moc"
