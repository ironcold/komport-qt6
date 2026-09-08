/***************************************************************************
                          settingsdialog.h  -  Komport Serial Port Communicator
                             -------------------
    Serial port settings form
    original author      : Mike Sharkey <michael@sharkey.servebeer.com>
    ported to Qt6         : 2026, hand-written to replace the Qt3-Designer
                             .ui file (settingsdialog.ui), which uic in Qt6
                             cannot read.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QColor>

class QComboBox;
class QSpinBox;
class QCheckBox;
class QRadioButton;
class QGroupBox;
class QFontComboBox;
class QPushButton;
class QFont;

/** Connection/terminal settings dialog.
 *
 * Same fields as the original KDE3 dialog (Device tab: device/baud rate/
 * RX queue/framing/flow control; Terminal tab: history buffer/emulation/
 * bell/echo), rebuilt with Qt6 layouts instead of the original's fixed
 * pixel coordinates. The device combo box is now populated from
 * QSerialPortInfo::availablePorts() instead of a hard-coded ttyS0..3 list.
 * The KURLRequester-based "file buffer" path picker is gone - the file
 * scroll buffer feature it configured was never functional to begin with
 * (see komportfilescrollbuffer.cpp), so its path field is now a plain,
 * disabled-by-default QLineEdit kept only so the layout/behavior otherwise
 * matches the original.
 */
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog( QWidget* parent = nullptr );
    ~SettingsDialog() override;

    // Device tab
    QComboBox* DeviceComboBox;
    QComboBox* BaudRateComboBox;
    QSpinBox* RxQueueSpinBox;
    QSpinBox* FlushRateSpinBox;
    QComboBox* FlowControlComboBox;
    QComboBox* StartBitsComboBox;
    QComboBox* DataBitsComboBox;
    QComboBox* StopBitsComboBox;
    QComboBox* ParityComboBox;

    // Terminal tab
    QRadioButton* MemoryBufferRadioButton;
    QRadioButton* FileBufferRadioButton;
    QSpinBox* ScrollBufferSpinBox;
    QSpinBox* FileBufferSizeSpinBox;
    QComboBox* EmulationComboBox;
    QCheckBox* VisualBellCheckBox;
    QCheckBox* LocalEchoCheckBox;

    // Appearance tab (Milestone 5: font family/size/spacing, color-scheme
    // presets, fg/bg color pickers)
    QFontComboBox* FontComboBox;
    QSpinBox* FontSizeSpinBox;
    /** letter spacing, as a percentage of the font's normal advance width
     *  (100 = normal) - see QFont::setLetterSpacing(QFont::PercentageSpacing, ...) */
    QSpinBox* FontSpacingSpinBox;
    QComboBox* ColorSchemeComboBox;
    QPushButton* ForegroundColorButton;
    QPushButton* BackgroundColorButton;

    /** the font currently selected on the Appearance tab (family from
     *  FontComboBox, size from FontSizeSpinBox, letter spacing from
     *  FontSpacingSpinBox) */
    QFont selectedFont() const;
    /** pre-fill the Appearance tab's font controls */
    void setSelectedFont(const QFont &_font);
    /** current fg/bg as picked (explicitly, or via a scheme preset) on the
     *  Appearance tab */
    QColor foregroundColor() const { return mForegroundColor; }
    QColor backgroundColor() const { return mBackgroundColor; }
    /** pre-fill the Appearance tab's color swatches/scheme selection.
     *  Selects whichever preset's colors match exactly, or "Custom" if
     *  none do - same rule setColorButtonSwatch()/color-button clicks use
     *  to decide when to fall back to "Custom" themselves. */
    void setColors(const QColor &_fg, const QColor &_bg);

private:
    QWidget* createDeviceTab();
    QWidget* createTerminalTab();
    QWidget* createAppearanceTab();
    /** paint _button's background to show _color as a swatch */
    static void setColorButtonSwatch(QPushButton *_button, const QColor &_color);
    /** re-select whichever ColorSchemeComboBox entry matches
     *  mForegroundColor/mBackgroundColor exactly, or "Custom" (index 0) if
     *  none do - called after every color change so the combo box always
     *  honestly reflects the current fg/bg, never a stale preset name. */
    void syncColorSchemeComboToCurrentColors();

    QColor mForegroundColor;
    QColor mBackgroundColor;
};

#endif // SETTINGSDIALOG_H
