/***************************************************************************
                          itransport.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : 2026
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

#include "itransport.h"

// ITransport has no logic of its own; this translation unit exists so that the
// interface has out-of-line definitions and so that the Core-only compile proof
// (tests/session_contract_compile.cpp, built against Qt6::Core alone) covers an
// actual implementation unit of the contract.

ITransport::ITransport(QObject *parent)
: QObject(parent)
{
}

ITransport::~ITransport() = default;
