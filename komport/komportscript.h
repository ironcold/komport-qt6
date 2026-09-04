/***************************************************************************
                          komportscript.h  -  Komport Serial Port Communicator
                             -------------------
    begin                : Thu Feb 20 2003
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

#ifndef KOMPORTSCRIPT_H
#define KOMPORTSCRIPT_H

#include <QObject>

/**impliments scripting
  *@author Mike Sharkey
  */

class KomportScript : public QObject  {
public:
	explicit KomportScript(QObject *parent = nullptr);
	~KomportScript() override;
};

#endif
