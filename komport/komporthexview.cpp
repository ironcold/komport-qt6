/***************************************************************************
                          komporthexview.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : 2026 (new in the Qt6 port)
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "komporthexview.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QFontDatabase>
#include <QTimer>
#include <QSettings>

KomportHexView::KomportHexView(QWidget *parent)
: QWidget(parent)
, mRxOffset(0)
, mTxOffset(0)
{
  mLog = new QPlainTextEdit(this);
  mLog->setReadOnly(true);
  mLog->setLineWrapMode(QPlainTextEdit::NoWrap);
  mLog->setFont( QFontDatabase::systemFont(QFontDatabase::FixedFont) );
  mLog->setMaximumBlockCount(20000); // bound memory use on long sessions

  auto *clearButton = new QPushButton( tr("Clear"), this );
  connect( clearButton, &QPushButton::clicked, this, &KomportHexView::clearLog );

  mRxCheck = new QCheckBox( tr("RX"), this );
  mRxCheck->setChecked(true);
  mRxCheck->setToolTip( tr("Show received bytes") );
  mTxCheck = new QCheckBox( tr("TX"), this );
  mTxCheck->setChecked(true);
  mTxCheck->setToolTip( tr("Show sent bytes") );

  auto *topBar = new QHBoxLayout();
  topBar->addWidget( new QLabel( tr("Hex Monitor"), this ) );
  topBar->addSpacing(12);
  topBar->addWidget( mRxCheck );
  topBar->addWidget( mTxCheck );
  topBar->addStretch(1);
  topBar->addWidget( clearButton );

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(2,2,2,2);
  layout->addLayout(topBar);
  layout->addWidget(mLog);

  mFlushTimer = new QTimer(this);
  mFlushTimer->setInterval(300);
  connect( mFlushTimer, &QTimer::timeout, this, &KomportHexView::flushPending );
  mFlushTimer->start();
}

KomportHexView::~KomportHexView()
{
}

void KomportHexView::appendRx(char _ch){ appendByte(Direction::Rx, _ch); }
void KomportHexView::appendTx(char _ch){ appendByte(Direction::Tx, _ch); }

void KomportHexView::appendByte(Direction _dir, char _ch)
{
  // The panel is hidden by default and often stays that way for a whole
  // session - don't spend cycles formatting/logging bytes nobody can see.
  // Nothing is lost by skipping while hidden: this is a live monitor, not
  // a persistent log (that's what the session logger is for), so there is
  // no backlog to catch up on once the panel is shown again.
  if ( !isVisible() ) return;
  if ( _dir == Direction::Rx && !mRxCheck->isChecked() ) return;
  if ( _dir == Direction::Tx && !mTxCheck->isChecked() ) return;

  QByteArray &buf = (_dir == Direction::Rx) ? mRxBuffer : mTxBuffer;
  buf.append(_ch);
  if ( buf.size() >= 16 ) {
    flushLine(_dir);
  }
}

void KomportHexView::flushPending()
{
  if ( !mRxBuffer.isEmpty() ) flushLine(Direction::Rx);
  if ( !mTxBuffer.isEmpty() ) flushLine(Direction::Tx);
}

void KomportHexView::flushLine(Direction _dir)
{
  QByteArray &buf = (_dir == Direction::Rx) ? mRxBuffer : mTxBuffer;
  quint64 &offset = (_dir == Direction::Rx) ? mRxOffset : mTxOffset;
  if ( buf.isEmpty() ) return;
  mLog->appendPlainText( formatRow( _dir == Direction::Rx ? "RX" : "TX", offset, buf ) );
  offset += static_cast<quint64>( buf.size() );
  buf.clear();
}

void KomportHexView::saveSettings(QSettings *_settings) const
{
  if ( !_settings ) return;
  _settings->beginGroup( QStringLiteral("HexMonitor") );
  _settings->setValue( QStringLiteral("RxEnabled"), mRxCheck->isChecked() );
  _settings->setValue( QStringLiteral("TxEnabled"), mTxCheck->isChecked() );
  _settings->endGroup();
}

void KomportHexView::loadSettings(QSettings *_settings)
{
  if ( !_settings ) return;
  _settings->beginGroup( QStringLiteral("HexMonitor") );
  // No contains()-guard, same reasoning as KomportMacroBar::loadSettings():
  // a profile without its own "HexMonitor" group must reset to the
  // default (both checked), not keep whatever the previously loaded
  // profile left these checkboxes at.
  mRxCheck->setChecked( _settings->value( QStringLiteral("RxEnabled"), true ).toBool() );
  mTxCheck->setChecked( _settings->value( QStringLiteral("TxEnabled"), true ).toBool() );
  _settings->endGroup();
}

void KomportHexView::clearLog()
{
  mLog->clear();
  mRxBuffer.clear();
  mTxBuffer.clear();
  mRxOffset = 0;
  mTxOffset = 0;
}

QString KomportHexView::formatRow(const char *_tag, quint64 _offset, const QByteArray &_bytes)
{
  QString line;
  line += QLatin1String(_tag);
  line += QLatin1Char(' ');
  line += QStringLiteral("%1").arg(_offset, 8, 16, QLatin1Char('0'));
  line += QStringLiteral("  ");
  for ( int i = 0; i < 16; ++i ) {
    if ( i < _bytes.size() ) {
      line += QStringLiteral("%1 ").arg( static_cast<unsigned char>(_bytes.at(i)), 2, 16, QLatin1Char('0') );
    } else {
      line += QStringLiteral("   ");
    }
    if ( i == 7 ) line += QLatin1Char(' ');
  }
  line += QStringLiteral(" |");
  for ( int i = 0; i < _bytes.size(); ++i ) {
    unsigned char c = static_cast<unsigned char>( _bytes.at(i) );
    line += ( c >= 0x20 && c < 0x7f ) ? QChar(QLatin1Char(static_cast<char>(c))) : QChar(QLatin1Char('.'));
  }
  line += QLatin1Char('|');
  return line;
}
