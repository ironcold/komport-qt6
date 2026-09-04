/***************************************************************************
                          komportmacrobar.h  -  Komport Serial Port Communicator
                             -------------------
    begin                : 2026 (new in the Qt6 port)
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de

    A row of programmable quick-command buttons (e.g. "show
    running-config", "exit", "wr mem") docked to the bottom of the main
    window, so a repeated command is one click instead of retyping it
    under pressure.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KOMPORTMACROBAR_H
#define KOMPORTMACROBAR_H

#include <QWidget>
#include <QVector>
#include <QString>

class QPushButton;
class QSettings;

/** A fixed number of programmable buttons. Left-click sends the button's
 *  command (KomportApp appends the configured line ending); left-click on
 *  an unconfigured (empty-command) button, or right-click on any button,
 *  opens a small editor for that slot. Persisted via QSettings under a
 *  "Macros" group.
 */
class KomportMacroBar : public QWidget
{
  Q_OBJECT
public:
  explicit KomportMacroBar(QWidget *parent = nullptr);
  ~KomportMacroBar() override;

  void loadSettings(QSettings *_settings);
  void saveSettings(QSettings *_settings) const;

signals:
  /** a macro button was clicked with a non-empty command configured */
  void macroTriggered(const QString &_command);

private slots:
  void slotButtonClicked(int _index);
  void slotEditRequested(int _index);

private:
  struct MacroSlot {
    QString label;
    QString command;
  };

  void editSlot(int _index);
  void updateButtonText(int _index);

  static const int SlotCount = 8;
  QVector<MacroSlot> mSlots;
  QVector<QPushButton*> mButtons;
};

#endif // KOMPORTMACROBAR_H
