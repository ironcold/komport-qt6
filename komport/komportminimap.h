/***************************************************************************
                          komportminimap.h  -  Komport Serial Port Communicator
                             -------------------
    begin                : 2026 (new in the Qt6 port)
    copyright            : (C) 2026 by Harald Stürmer
    email                : ironcold@ironcold.de

    A Kate-style "minimap" scrollbar for KomportView: instead of a plain
    QScrollBar, this renders the whole scrollback+live screen as a shrunk
    text-density silhouette, highlights the currently visible portion, and
    shows a text preview tooltip on hover. Drop-in replacement for the
    QScrollBar it used to be (same value()/setValue()/maximum()/
    setMaximum()/valueChanged() surface) - see the "Layout-Korrektur"/
    "Kate-Like Minimap" milestone in TODO-ARCHIVE.md for why this exists:
    the original QScrollBar was a manually-positioned sibling of the view
    rather than a proper child, which broke once the view moved into a
    QSplitter (visible as a stray floating bar in early screenshots). This
    widget is instead a real child of KomportView, so its position is
    always correct relative to its immediate parent no matter how that
    parent itself is embedded elsewhere.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KOMPORTMINIMAP_H
#define KOMPORTMINIMAP_H

#include <QWidget>

class KomportView;

/** Renders KomportView's combined scrollback+live history as a shrunk
 *  text-density silhouette (a rough analog of Kate's minimap - not a
 *  literal miniature text rendering, since the character grid isn't
 *  backed by a QTextDocument to reuse for that), with a highlighted
 *  "currently visible" band and a hover text preview. Colors come from
 *  the widget's own QPalette, so it automatically follows whatever
 *  system theme (Breeze Light/Dark, ...) is active - no theme-specific
 *  code needed. */
class KomportMinimapScrollBar : public QWidget
{
  Q_OBJECT
public:
  /** _view supplies the content (KomportView::totalHistoryRows()/
   *  cellAtHistoryRow()) this minimap renders and scrolls. */
  explicit KomportMinimapScrollBar(KomportView *_view, QWidget *_parent = nullptr);

  int value() const { return mValue; }
  int maximum() const { return mMaximum; }

public slots:
  /** _min is accepted for QScrollBar API compatibility but always treated
   *  as 0 - nothing in this codebase ever used a non-zero minimum. */
  void setRange(int _min, int _max);
  void setMaximum(int _max);
  void setValue(int _value);

signals:
  void valueChanged(int _value);

protected:
  void paintEvent(QPaintEvent *_e) override;
  void mousePressEvent(QMouseEvent *_e) override;
  void mouseMoveEvent(QMouseEvent *_e) override;
  void wheelEvent(QWheelEvent *_e) override;
  bool event(QEvent *_e) override; // intercepts QEvent::ToolTip for the hover preview

private:
  void scrollToPixelY(int _y);
  /** plain-text preview of a few rows around the given minimap pixel y,
   *  for the hover tooltip */
  QString previewAt(int _y) const;

  KomportView *mView;
  int mValue;
  int mMaximum;
};

#endif // KOMPORTMINIMAP_H
