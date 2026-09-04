/***************************************************************************
                          komportmacrobar.cpp  -  Komport Serial Port Communicator
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "komportmacrobar.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QInputDialog>
#include <QLineEdit>
#include <QSettings>

KomportMacroBar::KomportMacroBar(QWidget *parent)
: QWidget(parent)
{
  // A handful of sensible, immediately useful defaults (the rest of the
  // slots start out empty for the user to fill in) - editable at any time
  // via right-click, or left-click on an empty one.
  mSlots.resize(SlotCount);
  mSlots[0] = { tr("Show Config"), QStringLiteral("show running-config") };
  mSlots[1] = { tr("Save (wr mem)"), QStringLiteral("wr mem") };
  mSlots[2] = { tr("Exit"), QStringLiteral("exit") };

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(4,2,4,2);

  mButtons.resize(SlotCount);
  for ( int i = 0; i < SlotCount; ++i ) {
    auto *button = new QPushButton(this);
    button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect( button, &QPushButton::clicked, this, [this,i](){ slotButtonClicked(i); } );
    connect( button, &QPushButton::customContextMenuRequested, this, [this,i](const QPoint&){ slotEditRequested(i); } );
    layout->addWidget(button);
    mButtons[i] = button;
    updateButtonText(i);
  }
}

KomportMacroBar::~KomportMacroBar()
{
}

void KomportMacroBar::updateButtonText(int _index)
{
  const MacroSlot &slot = mSlots.at(_index);
  QPushButton *button = mButtons.at(_index);
  if ( slot.label.isEmpty() && slot.command.isEmpty() ) {
    button->setText( tr("(unused)") );
    button->setToolTip( tr("Left-click to configure a quick command; right-click to edit at any time.") );
  } else {
    button->setText( slot.label.isEmpty() ? slot.command : slot.label );
    button->setToolTip( slot.command );
  }
}

void KomportMacroBar::slotButtonClicked(int _index)
{
  const MacroSlot &slot = mSlots.at(_index);
  if ( slot.command.isEmpty() ) {
    editSlot(_index);
  } else {
    emit macroTriggered(slot.command);
  }
}

void KomportMacroBar::slotEditRequested(int _index)
{
  editSlot(_index);
}

void KomportMacroBar::editSlot(int _index)
{
  MacroSlot &slot = mSlots[_index];
  bool ok = false;
  QString label = QInputDialog::getText( this, tr("Edit Quick Command"),
      tr("Button label (shown on the button):"), QLineEdit::Normal, slot.label, &ok );
  if ( !ok ) return;
  QString command = QInputDialog::getText( this, tr("Edit Quick Command"),
      tr("Command to send (the configured line ending is appended automatically):"),
      QLineEdit::Normal, slot.command, &ok );
  if ( !ok ) return;
  slot.label = label;
  slot.command = command;
  updateButtonText(_index);
}

void KomportMacroBar::loadSettings(QSettings *_settings)
{
  if ( !_settings ) return;
  _settings->beginGroup( QStringLiteral("Macros") );
  for ( int i = 0; i < SlotCount; ++i ) {
    const QString key = QStringLiteral("Slot%1").arg(i);
    if ( _settings->contains(key + QStringLiteral("/Command")) ) {
      mSlots[i].label = _settings->value(key + QStringLiteral("/Label")).toString();
      mSlots[i].command = _settings->value(key + QStringLiteral("/Command")).toString();
    }
    updateButtonText(i);
  }
  _settings->endGroup();
}

void KomportMacroBar::saveSettings(QSettings *_settings) const
{
  if ( !_settings ) return;
  _settings->beginGroup( QStringLiteral("Macros") );
  for ( int i = 0; i < SlotCount; ++i ) {
    const QString key = QStringLiteral("Slot%1").arg(i);
    _settings->setValue(key + QStringLiteral("/Label"), mSlots.at(i).label);
    _settings->setValue(key + QStringLiteral("/Command"), mSlots.at(i).command);
  }
  _settings->endGroup();
}
