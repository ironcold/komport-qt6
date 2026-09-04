/***************************************************************************
                          main.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : Mon Feb 17 2003
    copyright            : (C) 2003 by Mike Sharkey
    email                : michael@sharkey.servebeer.com
    ported to Qt6         : 2026
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QUrl>

#include "komport.h"

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);

  QCoreApplication::setOrganizationName( QStringLiteral("Komport") );
  QCoreApplication::setApplicationName( QStringLiteral("Komport") );
  QCoreApplication::setApplicationVersion( QStringLiteral(KOMPORT_VERSION) );

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QObject::tr("Komport - Serial port communication and terminal emulator.") );
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addPositionalArgument( QStringLiteral("file"), QObject::tr("file to open"), QStringLiteral("[file]") );
  parser.process(app);

  KomportApp *komport = new KomportApp();
  komport->show();

  const QStringList args = parser.positionalArguments();
  if ( !args.isEmpty() )
  {
    komport->openDocumentFile( QUrl::fromUserInput(args.at(0)) );
  }
  else
  {
    komport->openDocumentFile();
  }

  return app.exec();
}
