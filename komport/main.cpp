/***************************************************************************
                          main.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : Mon Feb 17 00:05:54 EST 2003
    copyright            : (C) 2003 by Mike Sharkey
    email                : michael@sharkey.servebeer.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>

#include "komport.h"

static const char *description =
	I18N_NOOP("Komport - Serial port communication\nand terminal emulator.");
// INSERT A DESCRIPTION FOR YOUR APPLICATION HERE
	
	
static KCmdLineOptions options[] =
{
  { "+[File]", I18N_NOOP("file to open"), 0 },
  { 0, 0, 0 }
  // INSERT YOUR COMMANDLINE OPTIONS HERE
};

int main(int argc, char *argv[])
{

	KAboutData aboutData( "komport", I18N_NOOP("Komport"),
		VERSION, description, KAboutData::License_GPL,
		"(c) 2003, Mike Sharkey", 0, "http://sharkey.servebeer.com/~michael/komport", "michael@sharkey.servebeer.com");
	aboutData.addAuthor("Mike Sharkey",0, "michael@sharkey.servebeer.com");
	KCmdLineArgs::init( argc, argv, &aboutData );
	KCmdLineArgs::addCmdLineOptions( options ); // Add our own options.

  KApplication app;
 
  if (app.isRestored())
  {
    RESTORE(KomportApp);
  }
  else 
  {
    KomportApp *komport = new KomportApp();
    komport->show();

    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
		
		if (args->count())
		{
        komport->openDocumentFile(args->arg(0));
		}
		else
		{
		  komport->openDocumentFile();
		}
		args->clear();
  }

  return app.exec();
}  
