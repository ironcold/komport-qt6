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

class QComboBox;
class QSpinBox;
class QCheckBox;
class QRadioButton;
class QGroupBox;

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

private:
    QWidget* createDeviceTab();
    QWidget* createTerminalTab();
};

#endif // SETTINGSDIALOG_H
