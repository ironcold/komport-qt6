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
#include "komportcharset.h"

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
#include <QFontComboBox>
#include <QFontDatabase>
#include <QColorDialog>
#include <QFont>
#include <QApplication>
#include <QPalette>

namespace {

// Predefined color-scheme templates (Milestone 5). "Custom" (index 0,
// added separately in createAppearanceTab() rather than kept here) is not
// a real scheme - it's what the combo box falls back to showing whenever
// the current fg/bg don't exactly match one of these, so it's never
// something the user directly "picks" to get colors from.
struct ColorScheme { const char *name; QColor fg; QColor bg; };
const ColorScheme kColorSchemes[] = {
  // KDE Breeze's light/dark palette text/window colors.
  { "Breeze Light",           QColor(0x23,0x26,0x29), QColor(0xfc,0xfc,0xfc) },
  { "Breeze Dark",            QColor(0xfc,0xfc,0xfc), QColor(0x23,0x26,0x29) },
  // Classic phosphor-green retro terminal.
  { "Green on Black",         QColor(0x33,0xff,0x33), QColor(0x00,0x00,0x00) },
  // Low-glare, easy-on-the-eyes light scheme.
  { "Black on Light Yellow",  QColor(0x00,0x00,0x00), QColor(0xff,0xff,0xdc) },
};

} // namespace

SettingsDialog::SettingsDialog( QWidget* parent )
    : QDialog( parent )
    , mForegroundColor( QApplication::palette().color(QPalette::Text) )
    , mBackgroundColor( QApplication::palette().color(QPalette::Base) )
{
    setObjectName( QStringLiteral("SettingsDialog") );
    setWindowTitle( tr("Settings") );
    setSizeGripEnabled( true );

    auto *tabWidget = new QTabWidget( this );
    tabWidget->addTab( createDeviceTab(), tr("Device") );
    tabWidget->addTab( createTerminalTab(), tr("Terminal") );
    tabWidget->addTab( createAppearanceTab(), tr("Appearance") );

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
    // Minimum 1, not 0: KomportSerial::setFlushRate() now rejects 0 too
    // (QTimer::start(0) re-fires on every single event-loop iteration -
    // an idle busy-poll, not a "flush immediately" setting), but the
    // dialog shouldn't offer a value it's just going to silently bump up
    // to 1 anyway.
    FlushRateSpinBox->setRange( 1, 4096 );
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
    // Milestone 7: retro/industrial byte-level character-set translation -
    // see KomportCharset for the mapping tables. Codex review finding:
    // an earlier version relied on the combo box's row position matching
    // KomportCharset::Id's numeric value (via toIndex()/fromIndex()) -
    // correct today, but silently fragile against a future reordering of
    // either the enum or names(). Each item now carries its actual Id
    // explicitly as Qt::UserRole data instead, so display order and
    // stored value can never drift apart even if one of them changes.
    auto *charsetLabel = new QLabel( tr("Character Set:"), emulationGroup );
    CharsetComboBox = new QComboBox( emulationGroup );
    // displayEntries() is the single source of truth for name<->Id pairing
    // (see its own comment) - no separate list to keep in sync by hand.
    for ( const auto &entry : KomportCharset::displayEntries() ) {
      CharsetComboBox->addItem( entry.second, static_cast<int>(entry.first) );
    }
    CharsetComboBox->setToolTip( tr("Translate the raw byte stream between the serial\n"
                                     "device and the terminal display - for retro/industrial\n"
                                     "gear that doesn't speak plain ASCII/Latin-1.") );
    VisualBellCheckBox = new QCheckBox( tr("Visual Bell"), emulationGroup );
    LocalEchoCheckBox = new QCheckBox( tr("Local Echo"), emulationGroup );
    auto *emulationLayout = new QVBoxLayout( emulationGroup );
    emulationLayout->addWidget( EmulationComboBox );
    emulationLayout->addWidget( charsetLabel );
    emulationLayout->addWidget( CharsetComboBox );
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

QWidget* SettingsDialog::createAppearanceTab()
{
    auto *page = new QWidget( this );

    auto *fontGroup = new QGroupBox( tr("Font"), page );
    FontComboBox = new QFontComboBox( fontGroup );
    // Monospace-filtered (Milestone 5): every character cell in the grid is
    // a fixed pixel size (see KomportView::setCellSize()) with each glyph
    // centered in it - a proportional font still "works" (nothing crashes)
    // but looks visibly uneven, so don't offer one in the first place.
    FontComboBox->setFontFilters( QFontComboBox::MonospacedFonts );
    FontComboBox->setToolTip( tr("Monospace fonts only - every character cell in the\nterminal grid must be the same width.") );
    FontSizeSpinBox = new QSpinBox( fontGroup );
    FontSizeSpinBox->setRange( 6, 72 );
    FontSizeSpinBox->setValue( QFontDatabase::systemFont(QFontDatabase::FixedFont).pointSize() );
    FontSpacingSpinBox = new QSpinBox( fontGroup );
    FontSpacingSpinBox->setRange( 50, 300 );
    FontSpacingSpinBox->setSingleStep( 5 );
    FontSpacingSpinBox->setValue( 100 );
    FontSpacingSpinBox->setSuffix( QStringLiteral("%") );
    FontSpacingSpinBox->setToolTip( tr("Letter spacing, as a percentage of the font's normal\ncharacter width. 100% is normal spacing.") );
    auto *fontLayout = new QFormLayout( fontGroup );
    fontLayout->addRow( tr("Family:"), FontComboBox );
    fontLayout->addRow( tr("Size:"), FontSizeSpinBox );
    fontLayout->addRow( tr("Spacing:"), FontSpacingSpinBox );

    auto *colorGroup = new QGroupBox( tr("Colors"), page );
    ColorSchemeComboBox = new QComboBox( colorGroup );
    // Index 0 is "Custom" - not a real scheme, just what this combo shows
    // whenever the current fg/bg don't exactly match one of the presets
    // below (see syncColorSchemeComboToCurrentColors()). Never itself
    // applies a color when selected - selecting it manually is a no-op,
    // consistent with there being no "custom" colors to apply.
    ColorSchemeComboBox->addItem( tr("Custom") );
    for ( const ColorScheme &scheme : kColorSchemes ) {
        ColorSchemeComboBox->addItem( tr(scheme.name) );
    }
    ForegroundColorButton = new QPushButton( tr("Text Color…"), colorGroup );
    BackgroundColorButton = new QPushButton( tr("Background Color…"), colorGroup );
    setColorButtonSwatch( ForegroundColorButton, mForegroundColor );
    setColorButtonSwatch( BackgroundColorButton, mBackgroundColor );

    connect( ColorSchemeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int _index) {
        if ( _index <= 0 || _index > int(sizeof(kColorSchemes)/sizeof(kColorSchemes[0])) ) return; // "Custom" - nothing to apply
        const ColorScheme &scheme = kColorSchemes[_index-1];
        mForegroundColor = scheme.fg;
        mBackgroundColor = scheme.bg;
        setColorButtonSwatch( ForegroundColorButton, mForegroundColor );
        setColorButtonSwatch( BackgroundColorButton, mBackgroundColor );
    } );
    connect( ForegroundColorButton, &QPushButton::clicked, this, [this]() {
        const QColor picked = QColorDialog::getColor( mForegroundColor, this, tr("Text Color") );
        if ( !picked.isValid() ) return; // dialog cancelled
        mForegroundColor = picked;
        setColorButtonSwatch( ForegroundColorButton, mForegroundColor );
        // Picking a color by hand almost certainly no longer matches
        // whichever preset was selected (if any) - reflect that honestly
        // instead of leaving a stale scheme name showing.
        syncColorSchemeComboToCurrentColors();
    } );
    connect( BackgroundColorButton, &QPushButton::clicked, this, [this]() {
        const QColor picked = QColorDialog::getColor( mBackgroundColor, this, tr("Background Color") );
        if ( !picked.isValid() ) return;
        mBackgroundColor = picked;
        setColorButtonSwatch( BackgroundColorButton, mBackgroundColor );
        syncColorSchemeComboToCurrentColors();
    } );

    auto *colorLayout = new QFormLayout( colorGroup );
    colorLayout->addRow( tr("Scheme:"), ColorSchemeComboBox );
    colorLayout->addRow( ForegroundColorButton );
    colorLayout->addRow( BackgroundColorButton );

    auto *pageLayout = new QVBoxLayout( page );
    pageLayout->addWidget( fontGroup );
    pageLayout->addWidget( colorGroup );
    pageLayout->addStretch( 1 );

    return page;
}

QFont SettingsDialog::selectedFont() const
{
    QFont f = FontComboBox->currentFont();
    f.setPointSize( FontSizeSpinBox->value() );
    // The font combo is already monospace-filtered, but a caller applying
    // this font directly (KomportView::setTerminalFont()) benefits from
    // the hint being explicit too, not just "whatever this family happens
    // to default to".
    f.setFixedPitch( true );
    // Milestone 5's spec explicitly asked for a spacing control alongside
    // family/size (Codex review finding: the first version only exposed
    // those two). PercentageSpacing (100 = normal) maps directly onto the
    // spinbox's own 50-300% range.
    f.setLetterSpacing( QFont::PercentageSpacing, FontSpacingSpinBox->value() );
    return f;
}

void SettingsDialog::setSelectedFont(const QFont &_font)
{
    FontComboBox->setCurrentFont( _font );
    // A default-constructed/unset QFont's pointSize() is -1 (pixel-size-only)
    // - fall back to the same system fixed-font size used elsewhere as the
    // baseline default rather than briefly showing a nonsensical negative
    // spinbox value.
    const int pt = _font.pointSize();
    FontSizeSpinBox->setValue( pt > 0 ? pt : QFontDatabase::systemFont(QFontDatabase::FixedFont).pointSize() );
    // Only trust letterSpacing() when it's actually percentage-based -
    // AbsoluteSpacing (pixels) is a different unit this spinbox doesn't
    // represent, and a font that never had spacing set at all defaults to
    // PercentageSpacing/100 anyway, so this covers the common case too.
    FontSpacingSpinBox->setValue( _font.letterSpacingType() == QFont::PercentageSpacing
                                   ? qRound( _font.letterSpacing() ) : 100 );
}

void SettingsDialog::setColors(const QColor &_fg, const QColor &_bg)
{
    mForegroundColor = _fg;
    mBackgroundColor = _bg;
    setColorButtonSwatch( ForegroundColorButton, mForegroundColor );
    setColorButtonSwatch( BackgroundColorButton, mBackgroundColor );
    syncColorSchemeComboToCurrentColors();
}

void SettingsDialog::setColorButtonSwatch( QPushButton *_button, const QColor &_color )
{
    // Contrasting label text so the button's own caption stays legible
    // regardless of how dark or light the picked swatch color is.
    const QColor textColor = _color.lightness() > 128 ? QColor(Qt::black) : QColor(Qt::white);
    _button->setStyleSheet( QStringLiteral("background-color: %1; color: %2;")
                             .arg( _color.name(), textColor.name() ) );
}

void SettingsDialog::syncColorSchemeComboToCurrentColors()
{
    ColorSchemeComboBox->blockSignals( true ); // avoid re-triggering the scheme-applies-colors handler
    int matchIndex = 0; // "Custom" - the default when nothing below matches
    for ( int i = 0; i < int(sizeof(kColorSchemes)/sizeof(kColorSchemes[0])); ++i ) {
        if ( kColorSchemes[i].fg == mForegroundColor && kColorSchemes[i].bg == mBackgroundColor ) {
            matchIndex = i + 1; // +1: index 0 is "Custom", presets start at 1
            break;
        }
    }
    ColorSchemeComboBox->setCurrentIndex( matchIndex );
    ColorSchemeComboBox->blockSignals( false );
}
