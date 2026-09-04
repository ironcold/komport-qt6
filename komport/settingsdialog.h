/****************************************************************************
** Form interface generated from reading ui file './settingsdialog.ui'
**
** Created: Sun Oct 12 05:41:36 2003
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.1.1   edited Nov 21 17:40 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <qvariant.h>
#include <qdialog.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class KURLRequester;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QFrame;
class QGroupBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QTabWidget;
class QWidget;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    SettingsDialog( QWidget* parent = 0, const char* name = 0, bool modal = FALSE, WFlags fl = 0 );
    ~SettingsDialog();

    QTabWidget* tabWidget;
    QWidget* Widget2;
    QLabel* BaudRateLabel;
    QLabel* textLabel1;
    QComboBox* BaudRateComboBox;
    QComboBox* DeviceComboBox;
    QGroupBox* RXQueueGroupBox;
    QSpinBox* RxQueueSpinBox;
    QSpinBox* FlushRateSpinBox;
    QLabel* textLabel2;
    QLabel* textLabel1_2;
    QLabel* textLabel1_3;
    QLabel* textLabel2_2;
    QComboBox* FlowControlComboBox;
    QGroupBox* groupBox4;
    QComboBox* StartBitsComboBox;
    QComboBox* DataBitsComboBox;
    QComboBox* StopBitsComboBox;
    QComboBox* ParityComboBox;
    QLabel* textLabel3;
    QLabel* textLabel2_3;
    QLabel* textLabel1_4;
    QLabel* textLabel4_2;
    QLabel* textLabel4;
    QWidget* Widget3;
    QButtonGroup* buttonGroup1;
    KURLRequester* FileBufferURLRequeste;
    QRadioButton* FileBufferRadioButton;
    QFrame* line1;
    QRadioButton* MemoryBufferRadioButton;
    QSpinBox* ScrollBufferSpinBox;
    QLabel* textLines1;
    QLabel* textLines2;
    QSpinBox* FileBufferSizeSpinBox;
    QGroupBox* groupBox3;
    QComboBox* EmulationComboBox;
    QCheckBox* VisualBellCheckBox;
    QCheckBox* LocalEchoCheckBox;
    QPushButton* buttonHelp;
    QPushButton* buttonOk;
    QPushButton* buttonCancel;

protected:
    QGridLayout* SettingsDialogLayout;
    QHBoxLayout* Layout1;

protected slots:
    virtual void languageChange();
};

#endif // SETTINGSDIALOG_H
