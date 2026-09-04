/***************************************************************************
                          komportsessionlogger.h  -  Komport Serial Port Communicator
                             -------------------
    New in the Qt6 port (2026): one-click session logging - everything
    that scrolls across the terminal gets written to a text file, one
    timestamped line at a time, so an admin can review or hand over a
    console session afterwards.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KOMPORTSESSIONLOGGER_H
#define KOMPORTSESSIONLOGGER_H

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QByteArray>

/** Logs the received (displayed) byte stream to a text file, one
 *  timestamped line at a time - connect its logChar() slot to
 *  KomportSerial::receivedChar(char). */
class KomportSessionLogger : public QObject
{
  Q_OBJECT
public:
  explicit KomportSessionLogger(QObject *parent = nullptr);
  ~KomportSessionLogger() override;

  bool isLogging() const { return mFile.isOpen(); }
  QString fileName() const { return mFile.fileName(); }

public slots:
  /** open _path and start logging; returns false (and logs nothing) if the
   *  file could not be opened for writing */
  bool startLogging(const QString &_path);
  /** flush any pending partial line and close the log file */
  void stopLogging();
  /** feed one received byte into the log */
  void logChar(char _ch);

private:
  void flushLine();

  QFile mFile;
  QTextStream mStream;
  QByteArray mLineBuffer;
};

#endif // KOMPORTSESSIONLOGGER_H
