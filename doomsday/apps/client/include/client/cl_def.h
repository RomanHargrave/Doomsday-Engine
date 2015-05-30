/** @file cl_def.h  Client definitions.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DENG_CLIENT_H
#define DENG_CLIENT_H

#include "world/p_object.h"

#define SHORTP(x)       (*(short*) (x))
#define USHORTP(x)      (*(unsigned short*) (x))

DENG_EXTERN_C ident_t clientID;
DENG_EXTERN_C int serverTime;
DENG_EXTERN_C bool handshakeReceived;
DENG_EXTERN_C int gameReady;
DENG_EXTERN_C bool netLoggedIn;
DENG_EXTERN_C int clientPaused;

void Cl_InitID();

void Cl_CleanUp();

/**
 * Client's packet handler. Handles all the events the server sends.
 */
void Cl_GetPackets();

/**
 * Client-side game ticker.
 */
void Cl_Ticker(timespan_t ticLength);

int Cl_GameReady();

/**
 * Sends a hello packet.
 * PCL_HELLO2 includes the Game ID (16 chars).
 */
void Cl_SendHello();

#endif // DENG_CLIENT_H
