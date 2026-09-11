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
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

#include "komport.h"

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);

  QCoreApplication::setOrganizationName( QStringLiteral("Komport-Qt6") );
  QCoreApplication::setApplicationName( QStringLiteral("Komport-Qt6") );
  QCoreApplication::setApplicationVersion( QStringLiteral(KOMPORT_VERSION) );

  // Milestone 6: load the system locale's translation, if one is
  // available, before anything below (including the --help text just a
  // few lines down) gets a chance to call tr() - installed translators
  // only affect strings translated *after* installation, not retroactively.
  // komport/translations/*.ts (compiled to .qm at build time, see
  // CMakeLists.txt's qt6_add_translation()/-nounfinished, embedded via
  // Qt's resource system under ":/translations/") is this app's own
  // strings; qtbase's own translations (OK/Cancel/file-dialog text etc.)
  // ship separately with the Qt installation itself - both are optional:
  // load() returns false and leaves app/qtTranslator untouched if no
  // matching file exists (e.g. an English system, or a language this app
  // has no .ts file for yet), which is the correct fallback - the
  // original English tr() source strings display either way.
  QTranslator translator;
  if ( translator.load(QLocale::system(), QStringLiteral("komport"), QStringLiteral("_"), QStringLiteral(":/translations")) ) {
    QCoreApplication::installTranslator( &translator );
  }
  QTranslator qtTranslator;
  if ( qtTranslator.load(QLocale::system(), QStringLiteral("qtbase"), QStringLiteral("_"),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath)) ) {
    QCoreApplication::installTranslator( &qtTranslator );
  }

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QObject::tr("Komport-Qt6 - Serial port communication and terminal emulator.") );
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
