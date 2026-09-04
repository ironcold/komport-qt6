/***************************************************************************
                          komporthexview.cpp  -  Komport Serial Port Communicator
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
#include <QLabel>
#include <QFontDatabase>
#include <QTimer>

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

  auto *topBar = new QHBoxLayout();
  topBar->addWidget( new QLabel( tr("Hex Monitor (RX/TX)"), this ) );
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
