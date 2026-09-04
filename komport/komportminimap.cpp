/***************************************************************************
                          komportminimap.cpp  -  Komport Serial Port Communicator
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

#include "komportminimap.h"
#include "komportview.h"
#include "komportcellarray.h"
#include "komportcell.h"

#include <QPainter>
#include <QMouseEvent>
#include <QHelpEvent>
#include <QToolTip>
#include <QStringList>

static const int MINIMAP_WIDTH = 90;

KomportMinimapScrollBar::KomportMinimapScrollBar(KomportView *_view, QWidget *_parent)
: QWidget(_parent)
, mView(_view)
, mValue(0)
, mMaximum(0)
{
  setFixedWidth(MINIMAP_WIDTH);
  setCursor(Qt::PointingHandCursor);
  setMouseTracking(true); // so dragging with the button already down still tracks smoothly
}

void KomportMinimapScrollBar::setRange(int _min, int _max)
{
  Q_UNUSED(_min); // always 0 in practice - nothing here ever used a non-zero minimum
  setMaximum(_max);
}

void KomportMinimapScrollBar::setMaximum(int _max)
{
  mMaximum = qMax(0, _max);
  mValue = qBound(0, mValue, mMaximum);
  update();
}

void KomportMinimapScrollBar::setValue(int _value)
{
  int clamped = qBound(0, _value, mMaximum);
  if ( clamped == mValue ) return;
  mValue = clamped;
  update();
  // Real QScrollBar emits valueChanged() on any actual change, whichever
  // way it was set - callers throughout komportview.cpp (resetScroll() in
  // particular) rely on that to trigger a repaint, the same way they did
  // with the QScrollBar this replaces. Without this, resetScroll() (called
  // after essentially every incoming line) silently stopped updating the
  // view.
  emit valueChanged(mValue);
}

void KomportMinimapScrollBar::paintEvent(QPaintEvent *_e)
{
  Q_UNUSED(_e);
  QPainter p(this);
  p.fillRect(rect(), palette().color(QPalette::Base));

  if ( !mView ) return;
  const int total = mView->totalHistoryRows();
  const int gridWidth = mView->cellArray()->arrayWidth();
  if ( total <= 0 || gridWidth <= 0 || height() <= 0 ) return;

  // Text-density silhouette: one sampled content row per pixel row, one
  // dot per non-blank column in that row. Cheap enough to redo on every
  // paint (only triggered by resize/scroll, not continuously) even for a
  // few thousand rows of scrollback.
  QColor dot = palette().color(QPalette::Text);
  dot.setAlpha(170);
  p.setPen(dot);
  for ( int py = 0; py < height(); ++py ) {
    int row = qMin(total - 1, py * total / height());
    for ( int x = 0; x < gridWidth; ++x ) {
      KomportCell *cell = mView->cellAtHistoryRow(x, row);
      if ( cell && cell->character() != QChar(' ') ) {
        int px = x * width() / gridWidth;
        p.drawPoint(px, py);
      }
    }
  }

  // Highlight the currently visible band, same "scrolled" math as
  // KomportView::getCell()/resetScroll() use.
  const int viewportRows = mView->cellArray()->arrayHeight();
  const int scrolled = mMaximum - mValue;
  int viewTopRow = total - viewportRows - scrolled;
  viewTopRow = qBound(0, viewTopRow, qMax(0, total - viewportRows));
  int y1 = viewTopRow * height() / total;
  int y2 = (viewTopRow + viewportRows) * height() / total;
  if ( y2 <= y1 ) y2 = y1 + 1;

  QColor band = palette().color(QPalette::Highlight);
  band.setAlpha(60);
  p.fillRect(0, y1, width(), y2 - y1, band);
  p.setPen(palette().color(QPalette::Highlight));
  p.drawRect(0, y1, width() - 1, (y2 - y1) - 1);
}

void KomportMinimapScrollBar::scrollToPixelY(int _y)
{
  if ( !mView || mMaximum <= 0 ) return;
  const int total = mView->totalHistoryRows();
  if ( total <= 0 || height() <= 0 ) return;

  int row = qBound(0, _y * total / height(), total - 1);
  const int viewportRows = mView->cellArray()->arrayHeight();
  int scrolled = qBound(0, total - viewportRows - row, mMaximum);
  setValue(mMaximum - scrolled); // emits valueChanged() itself if it actually changed
}

void KomportMinimapScrollBar::mousePressEvent(QMouseEvent *_e)
{
  if ( _e->button() == Qt::LeftButton ) scrollToPixelY( _e->position().toPoint().y() );
}

void KomportMinimapScrollBar::mouseMoveEvent(QMouseEvent *_e)
{
  if ( _e->buttons() & Qt::LeftButton ) scrollToPixelY( _e->position().toPoint().y() );
}

void KomportMinimapScrollBar::wheelEvent(QWheelEvent *_e)
{
  // See KomportView::wheelEvent() for why this accumulates rather than
  // truncating each event's angleDelta() individually: many mice/touchpads
  // split a single notch across several small-delta events, which a plain
  // /120 would round down to zero every time.
  mAccumWheelDelta += _e->angleDelta().y();
  int steps = mAccumWheelDelta / 120; // 120 = one notch
  if ( steps != 0 ) {
    mAccumWheelDelta -= steps * 120;
    setValue( mValue + steps * 3 ); // a few lines per notch, like a normal scrollbar; emits itself
  }
  _e->accept();
}

QString KomportMinimapScrollBar::previewAt(int _y) const
{
  if ( !mView ) return QString();
  const int total = mView->totalHistoryRows();
  const int gridWidth = mView->cellArray()->arrayWidth();
  if ( total <= 0 || gridWidth <= 0 || height() <= 0 ) return QString();

  int row = qBound(0, _y * total / height(), total - 1);
  QStringList lines;
  for ( int r = qMax(0, row - 3); r <= qMin(total - 1, row + 3); ++r ) {
    QString line;
    for ( int x = 0; x < gridWidth; ++x ) {
      KomportCell *cell = mView->cellAtHistoryRow(x, r);
      line += cell ? cell->character() : QChar(' ');
    }
    // trailing-space-trimmed, but keep the line even if it's all blank so
    // the preview's vertical position still lines up with the minimap
    while ( line.endsWith(QChar(' ')) ) line.chop(1);
    lines << line;
  }
  return lines.join(QChar('\n'));
}

bool KomportMinimapScrollBar::event(QEvent *_e)
{
  if ( _e->type() == QEvent::ToolTip ) {
    auto *he = static_cast<QHelpEvent*>(_e);
    QString preview = previewAt( he->pos().y() );
    if ( !preview.isEmpty() ) {
      QToolTip::showText( he->globalPos(), preview, this );
    } else {
      QToolTip::hideText();
    }
    return true;
  }
  return QWidget::event(_e);
}
