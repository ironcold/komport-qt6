/***************************************************************************
                          settingsdialog.cpp  -  Komport Serial Port Communicator
                             -------------------
    Serial port settings form
    original author      : Mike Sharkey <michael@sharkey.servebeer.com>
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

#include "settingsdialog.h"

#include <QTabWidget>
#include <QGroupBox>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QSerialPortInfo>

SettingsDialog::SettingsDialog( QWidget* parent )
    : QDialog( parent )
{
    setObjectName( QStringLiteral("SettingsDialog") );
    setWindowTitle( tr("Settings") );
    setSizeGripEnabled( true );

    auto *tabWidget = new QTabWidget( this );
    tabWidget->addTab( createDeviceTab(), tr("Device") );
    tabWidget->addTab( createTerminalTab(), tr("Terminal") );

    auto *buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this );
    connect( buttons, &QDialogButtonBox::accepted, this, &QDialog::accept );
    connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );

    auto *layout = new QVBoxLayout( this );
    layout->addWidget( tabWidget );
    layout->addWidget( buttons );

    resize( 560, 380 );
}

SettingsDialog::~SettingsDialog()
{
}

QWidget* SettingsDialog::createDeviceTab()
{
    auto *page = new QWidget( this );

    DeviceComboBox = new QComboBox( page );
    DeviceComboBox->setEditable( true );
    DeviceComboBox->setToolTip( tr("Device special file name") );
    // Discover real serial ports via QSerialPortInfo instead of the
    // original's hard-coded /dev/ttyS0..3 list.
    const auto ports = QSerialPortInfo::availablePorts();
    if ( ports.isEmpty() ) {
        DeviceComboBox->addItem( QStringLiteral("/dev/ttyS0") );
    } else {
        for ( const QSerialPortInfo &info : ports ) {
            DeviceComboBox->addItem( info.portName().startsWith(QLatin1String("/dev/")) || info.portName().startsWith(QLatin1String("COM"))
                                      ? info.portName()
                                      : info.systemLocation() );
        }
    }

    BaudRateComboBox = new QComboBox( page );
    BaudRateComboBox->setToolTip( tr("Baud Rate (bps)") );
    const QStringList bauds = { "50","75","110","134","150","300","600","1200","1800","2400",
                                 "4800","9600","19200","38400","57600","115200","230400" };
    BaudRateComboBox->addItems( bauds );
    BaudRateComboBox->setCurrentText( QStringLiteral("9600") );

    auto *rxQueueGroup = new QGroupBox( tr("RX Queue"), page );
    RxQueueSpinBox = new QSpinBox( rxQueueGroup );
    RxQueueSpinBox->setRange( 1024, 32768 );
    RxQueueSpinBox->setSingleStep( 1024 );
    FlushRateSpinBox = new QSpinBox( rxQueueGroup );
    FlushRateSpinBox->setRange( 0, 4096 );
    FlushRateSpinBox->setSingleStep( 16 );
    FlushRateSpinBox->setValue( 256 );
    auto *rxQueueLayout = new QFormLayout( rxQueueGroup );
    rxQueueLayout->addRow( tr("Size:"), RxQueueSpinBox );
    rxQueueLayout->addRow( new QLabel( tr("bytes"), rxQueueGroup ) );
    rxQueueLayout->addRow( tr("Flush Interval:"), FlushRateSpinBox );
    rxQueueLayout->addRow( new QLabel( tr("milliseconds"), rxQueueGroup ) );

    auto *framingGroup = new QGroupBox( tr("Framing"), page );
    StartBitsComboBox = new QComboBox( framingGroup );
    StartBitsComboBox->addItems( { "1", "2" } );
    DataBitsComboBox = new QComboBox( framingGroup );
    DataBitsComboBox->addItems( { "5", "6", "7", "8" } );
    DataBitsComboBox->setCurrentText( QStringLiteral("8") );
    StopBitsComboBox = new QComboBox( framingGroup );
    StopBitsComboBox->addItems( { "1", "1.5", "2" } );
    ParityComboBox = new QComboBox( framingGroup );
    ParityComboBox->addItems( { "NONE", "EVEN", "ODD" } );
    auto *framingLayout = new QFormLayout( framingGroup );
    framingLayout->addRow( tr("Start bits:"), StartBitsComboBox );
    framingLayout->addRow( tr("Data bits:"), DataBitsComboBox );
    framingLayout->addRow( tr("Stop bits:"), StopBitsComboBox );
    framingLayout->addRow( tr("Parity:"), ParityComboBox );

    FlowControlComboBox = new QComboBox( page );
    FlowControlComboBox->addItems( { "XON/XOFF", "RTS/CTS", "NONE" } );
    FlowControlComboBox->setCurrentText( QStringLiteral("NONE") );

    auto *topForm = new QFormLayout();
    topForm->addRow( tr("Device:"), DeviceComboBox );
    topForm->addRow( tr("Baud Rate:"), BaudRateComboBox );
    topForm->addRow( tr("Flow Control:"), FlowControlComboBox );

    auto *lowerRow = new QHBoxLayout();
    lowerRow->addWidget( framingGroup );
    lowerRow->addWidget( rxQueueGroup );

    auto *pageLayout = new QVBoxLayout( page );
    pageLayout->addLayout( topForm );
    pageLayout->addLayout( lowerRow );
    pageLayout->addStretch( 1 );

    return page;
}

QWidget* SettingsDialog::createTerminalTab()
{
    auto *page = new QWidget( this );

    auto *emulationGroup = new QGroupBox( tr("Emulation"), page );
    EmulationComboBox = new QComboBox( emulationGroup );
    EmulationComboBox->addItem( QStringLiteral("VT102") );
    VisualBellCheckBox = new QCheckBox( tr("Visual Bell"), emulationGroup );
    LocalEchoCheckBox = new QCheckBox( tr("Local Echo"), emulationGroup );
    auto *emulationLayout = new QVBoxLayout( emulationGroup );
    emulationLayout->addWidget( EmulationComboBox );
    emulationLayout->addWidget( VisualBellCheckBox );
    emulationLayout->addWidget( LocalEchoCheckBox );
    emulationLayout->addStretch( 1 );

    auto *historyGroup = new QGroupBox( tr("History Buffer"), page );
    MemoryBufferRadioButton = new QRadioButton( tr("Memory buffer"), historyGroup );
    MemoryBufferRadioButton->setChecked( true );
    ScrollBufferSpinBox = new QSpinBox( historyGroup );
    ScrollBufferSpinBox->setRange( 0, 4096 );
    ScrollBufferSpinBox->setSingleStep( 256 );
    ScrollBufferSpinBox->setValue( 1024 );
    auto *linesLabel1 = new QLabel( tr("lines max."), historyGroup );

    // The file-backed scroll buffer was never functional in the original
    // (see komportfilescrollbuffer.cpp - cell() always returned null), so
    // its controls are kept only for layout/config compatibility and stay
    // disabled.
    FileBufferRadioButton = new QRadioButton( tr("File buffer"), historyGroup );
    FileBufferSizeSpinBox = new QSpinBox( historyGroup );
    FileBufferSizeSpinBox->setRange( 1024, 102400 );
    FileBufferSizeSpinBox->setSingleStep( 1024 );
    FileBufferSizeSpinBox->setValue( 4096 );
    FileBufferSizeSpinBox->setEnabled( false );
    auto *linesLabel2 = new QLabel( tr("lines max."), historyGroup );
    linesLabel2->setEnabled( false );

    connect( MemoryBufferRadioButton, &QRadioButton::toggled, ScrollBufferSpinBox, &QWidget::setEnabled );
    connect( MemoryBufferRadioButton, &QRadioButton::toggled, linesLabel1, &QWidget::setEnabled );
    connect( FileBufferRadioButton, &QRadioButton::toggled, FileBufferSizeSpinBox, &QWidget::setEnabled );
    connect( FileBufferRadioButton, &QRadioButton::toggled, linesLabel2, &QWidget::setEnabled );

    auto *historyLayout = new QGridLayout( historyGroup );
    historyLayout->addWidget( MemoryBufferRadioButton, 0, 0 );
    historyLayout->addWidget( ScrollBufferSpinBox, 0, 1 );
    historyLayout->addWidget( linesLabel1, 0, 2 );
    historyLayout->addWidget( FileBufferRadioButton, 1, 0 );
    historyLayout->addWidget( FileBufferSizeSpinBox, 1, 1 );
    historyLayout->addWidget( linesLabel2, 1, 2 );

    auto *pageLayout = new QHBoxLayout( page );
    pageLayout->addWidget( emulationGroup );
    pageLayout->addWidget( historyGroup );

    return page;
}
