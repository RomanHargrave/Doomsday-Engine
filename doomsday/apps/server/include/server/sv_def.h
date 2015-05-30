/** @file sv_def.h Server Definitions.
 * @ingroup server
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef __DOOMSDAY_SERVER_H__
#define __DOOMSDAY_SERVER_H__

#include "dd_def.h"
#include "network/protocol.h"
//#include "network/sys_network.h"
#include <de/Record>

struct material_s;

#ifdef __cplusplus
extern "C" {
#endif

#define SV_WELCOME_STRING   "Doomsday " DOOMSDAY_VERSION_TEXT " Server (R22)"

// Anything closer than this is always taken into consideration when
// deltas are being generated.
#define CLOSE_MOBJ_DIST     512

// Anything farther than this will never be taken into consideration.
#define FAR_MOBJ_DIST       1500

extern int svMaxPlayers;
extern int allowFrames; // Allow sending of frames.
extern int frameInterval; // In tics.
extern int netRemoteUser; // The client who is currently logged in.
extern char* netPassword; // Remote login password.

void            Sv_Shutdown(void);
void            Sv_StartNetGame(void);
void            Sv_StopNetGame(void);
dd_bool         Sv_PlayerArrives(nodeid_t nodeID, char const *name);
void            Sv_PlayerLeaves(nodeid_t nodeID);
void            Sv_Handshake(int playernum, dd_bool newplayer);
void            Sv_GetPackets(void);

/**
 * Sends a console message to one or more clients.
 *
 * @param to     Client number to send text to. Use a negative number to
 *               broadcast to everybody.
 * @param flags  @ref consolePrintFlags
 * @param text   Text to send.
 */
void            Sv_SendText(int to, int flags, const char* text);

void            Sv_Ticker(timespan_t ticLength);
int             Sv_Latency(byte cmdTime);
void            Sv_Kick(int who);
void            Sv_GetInfo(serverinfo_t* info);
size_t          Sv_InfoToString(serverinfo_t* info, ddstring_t* msg);
de::Record *    Sv_InfoToRecord(serverinfo_t *info);
int             Sv_GetNumPlayers(void);
int             Sv_GetNumConnected(void);
dd_bool         Sv_CheckBandwidth(int playerNumber);
dd_bool         Sv_CanTrustClientPos(int plrNum);

/**
 * Returns a unique id for material @a mat that can be passed on to clients.
 */
unsigned int Sv_IdForMaterial(Material *mat);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
