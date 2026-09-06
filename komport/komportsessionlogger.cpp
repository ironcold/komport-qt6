/***************************************************************************
                          komportsessionlogger.cpp  -  Komport Serial Port Communicator
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

#include "komportsessionlogger.h"

#include <QDateTime>

namespace {
  // mLineBuffer only ever gets flushed to disk on '\n' (see logChar()
  // below) - a device that sends a very long line, a binary/non-text
  // stream, or one that only ever uses bare '\r' without '\n' would
  // otherwise make it grow without bound for as long as logging stays on.
  // Force a flush once a single line gets implausibly long for a terminal
  // session; the log still records everything, just split across more
  // timestamped lines than usual.
  constexpr int MaxLineBufferLength = 4096;
}

KomportSessionLogger::KomportSessionLogger(QObject *parent)
: QObject(parent)
{
}

KomportSessionLogger::~KomportSessionLogger()
{
  stopLogging();
}

bool KomportSessionLogger::startLogging(const QString &_path)
{
  stopLogging();
  mFile.setFileName(_path);
  if ( !mFile.open(QIODevice::WriteOnly | QIODevice::Text) ) {
    return false;
  }
  mStream.setDevice(&mFile);
  mStream << "# Komport session log started "
          << QDateTime::currentDateTime().toString(Qt::ISODate) << Qt::endl;
  return true;
}

void KomportSessionLogger::stopLogging()
{
  if ( mFile.isOpen() ) {
    flushLine();
    mStream << "# Komport session log ended "
            << QDateTime::currentDateTime().toString(Qt::ISODate) << Qt::endl;
    mStream.flush();
    mStream.setDevice(nullptr);
    mFile.close();
  }
  mLineBuffer.clear();
}

void KomportSessionLogger::logChar(char _ch)
{
  if ( !mFile.isOpen() ) return;
  if ( _ch == '\n' ) {
    flushLine();
  } else if ( _ch != '\r' ) {
    // strip bare CRs - they only ever show up paired with a following LF
    // in the terminal streams this logs, and drawing a lone '\r' inside a
    // text-file line reads as noise rather than content.
    mLineBuffer.append(_ch);
    if ( mLineBuffer.size() >= MaxLineBufferLength ) {
      flushLine();
    }
  }
}

void KomportSessionLogger::flushLine()
{
  if ( mLineBuffer.isEmpty() ) return;
  const QString timestamp = QDateTime::currentDateTime().toString( QStringLiteral("HH:mm:ss.zzz") );
  mStream << '[' << timestamp << "] " << QString::fromLocal8Bit(mLineBuffer) << Qt::endl;
  mLineBuffer.clear();
}
