/** @file net_main.cpp  Client/server networking.
 *
 * Player number zero is always the server. In single-player games there is only
 * the server present.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#include <stdlib.h>             // for atoi()

#include "de_platform.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_ui.h"

#ifdef _DEBUG
#  include "ui/zonedebug.h"
#endif

#ifdef __CLIENT__
#  include "api_fontrender.h"
//#  include "render/rend_console.h"
#  include "render/rend_main.h"
#  include "render/lightgrid.h"
#  include "render/blockmapvisual.h"
#  include "edit_bias.h"
#  include "ui/inputdebug.h"
#  include "ui/widgets/taskbarwidget.h"
#endif

#ifdef __SERVER__
#  include "serversystem.h"
#endif

#include "dd_def.h"
#include "dd_main.h"
#include "dd_loop.h"
#include "world/p_players.h"

#include <de/Value>
#include <de/Version>
#include <de/charsymbols.h>

// MACROS ------------------------------------------------------------------

#define OBSOLETE                CVF_NO_ARCHIVE|CVF_HIDE // Old ccmds.

// The threshold is the average ack time * mul.
#define ACK_THRESHOLD_MUL       1.5f

// Never wait a too short time for acks.
#define ACK_MINIMUM_THRESHOLD   50

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

#ifdef __CLIENT__
D_CMD(Login); // in cl_main.c
#endif

#ifdef __SERVER__
D_CMD(Logout); // in sv_main.c
#endif

D_CMD(Ping); // in net_ping.c

int     Sv_GetRegisteredMobj(struct pool_s *, thid_t, struct mobjdelta_s *);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(Chat);
D_CMD(MakeCamera);
D_CMD(Net);
D_CMD(SetTicks);

#ifdef __CLIENT__
D_CMD(Connect);
D_CMD(SetConsole);
D_CMD(SetName);
#endif

#ifdef __SERVER__
D_CMD(Kick);
#endif

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

char   *serverName = (char *) "Doomsday";
char   *serverInfo = (char *) "Multiplayer Host";
char   *playerName = (char *) "Player";
int     serverData[3];          // Some parameters passed to master server.

client_t clients[DDMAXPLAYERS];   // All network data for the players.

int     netGame; // true if a netGame is in progress
int     isServer; // true if this computer is an open server.
int     isClient; // true if this computer is a client

// Gotframe is true if a frame packet has been received.
int     gotFrame = false;

dd_bool firstNetUpdate = true;

byte    monitorMsgQueue = false;
byte    netShowLatencies = false;
byte    netDev = false;
float   netConnectTime;
//int     netCoordTime = 17;
float   netConnectTimeout = 10;
float   netSimulatedLatencySeconds = 0;

// Local packets are stored into this buffer.
dd_bool reboundPacket;
netbuffer_t reboundStore;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

#ifdef __CLIENT__
static int coordTimer = 0;
#endif

// CODE --------------------------------------------------------------------

void Net_Register(void)
{
    // Cvars
    C_VAR_BYTE("net-queue-show", &monitorMsgQueue, 0, 0, 1);
    C_VAR_BYTE("net-dev", &netDev, 0, 0, 1);
#ifdef _DEBUG
    C_VAR_FLOAT("net-dev-latency", &netSimulatedLatencySeconds, CVF_NO_MAX, 0, 0);
#endif
    //C_VAR_BYTE("net-nosleep", &netDontSleep, 0, 0, 1);
    //C_VAR_CHARPTR("net-master-address", &masterAddress, 0, 0, 0);
    //C_VAR_INT("net-master-port", &masterPort, 0, 0, 65535);
    //C_VAR_CHARPTR("net-master-path", &masterPath, 0, 0, 0);
    C_VAR_CHARPTR("net-name", &playerName, 0, 0, 0);

#ifdef __CLIENT__
    // Cvars (client)
    C_VAR_FLOAT("client-connect-timeout", &netConnectTimeout, CVF_NO_MAX, 0, 0);
#endif

#ifdef __SERVER__
    // Cvars (server)
    C_VAR_CHARPTR("server-name", &serverName, 0, 0, 0);
    C_VAR_CHARPTR("server-info", &serverInfo, 0, 0, 0);
    C_VAR_INT("server-public", &masterAware, 0, 0, 1);
    C_VAR_CHARPTR("server-password", &netPassword, 0, 0, 0);
    C_VAR_BYTE("server-latencies", &netShowLatencies, 0, 0, 1);
    C_VAR_INT("server-frame-interval", &frameInterval, CVF_NO_MAX, 0, 0);
    C_VAR_INT("server-player-limit", &svMaxPlayers, 0, 0, DDMAXPLAYERS);
#endif

    // Ccmds
    C_CMD_FLAGS("chat", NULL, Chat, CMDF_NO_NULLGAME);
    C_CMD_FLAGS("chatnum", NULL, Chat, CMDF_NO_NULLGAME);
    C_CMD_FLAGS("chatto", NULL, Chat, CMDF_NO_NULLGAME);
    C_CMD_FLAGS("conlocp", "i", MakeCamera, CMDF_NO_NULLGAME);
#ifdef __CLIENT__
    C_CMD_FLAGS("connect", NULL, Connect, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    /// @todo Must reimplement using libshell. -jk
    //C_CMD_FLAGS("login", NULL, Login, CMDF_NO_NULLGAME);
#endif
#ifdef __SERVER__
    //C_CMD_FLAGS("logout", "", Logout, CMDF_NO_NULLGAME);
    C_CMD_FLAGS("kick", "i", Kick, CMDF_NO_NULLGAME);
#endif
    C_CMD_FLAGS("net", NULL, Net, CMDF_NO_NULLGAME);
    C_CMD_FLAGS("ping", NULL, Ping, CMDF_NO_NULLGAME);
    C_CMD_FLAGS("say", NULL, Chat, CMDF_NO_NULLGAME);
    C_CMD_FLAGS("saynum", NULL, Chat, CMDF_NO_NULLGAME);
    C_CMD_FLAGS("sayto", NULL, Chat, CMDF_NO_NULLGAME);
#ifdef __CLIENT__
    C_CMD("setname", "s", SetName);
    C_CMD("setcon", "i", SetConsole);
#endif
    C_CMD("settics", "i", SetTicks);

#ifdef __CLIENT__
    N_Register();
#endif
#ifdef __SERVER__
    Server_Register();
#endif
}

void Net_Init(void)
{
    int i;

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        memset(clients + i, 0, sizeof(clients[i]));
        clients[i].viewConsole = -1;
        Net_AllocClientBuffers(i);
    }

    memset(&netBuffer, 0, sizeof(netBuffer));
    netBuffer.headerLength = netBuffer.msg.data - (byte *) &netBuffer.msg;
    // The game is always started in single-player mode.
    netGame = false;
}

void Net_Shutdown(void)
{
    netGame = false;
    N_Shutdown();
    Net_DestroyArrays();
}

#undef Net_GetPlayerName
DENG_EXTERN_C const char* Net_GetPlayerName(int player)
{
    return clients[player].name;
}

#undef Net_GetPlayerID
DENG_EXTERN_C ident_t Net_GetPlayerID(int player)
{
    if(!clients[player].connected)
        return 0;

    return clients[player].id;
}

/**
 * Sends the contents of the netBuffer.
 */
void Net_SendBuffer(int toPlayer, int spFlags)
{
#ifdef __CLIENT__
    // Don't send anything during demo playback.
    if(playback)
        return;
#endif

    netBuffer.player = toPlayer;

    // A rebound packet?
    if(spFlags & SPF_REBOUND)
    {
        reboundStore = netBuffer;
        reboundPacket = true;
        return;
    }

#ifdef __CLIENT__
    Demo_WritePacket(toPlayer);
#endif

    // Can we send the packet?
    if(spFlags & SPF_DONT_SEND)
        return;

    // Send the packet to the network.
    N_SendPacket(spFlags);
}

/**
 * @return @c false, if there are no packets waiting.
 */
dd_bool Net_GetPacket(void)
{
    if(reboundPacket) // Local packets rebound.
    {
        netBuffer = reboundStore;
        netBuffer.player = consolePlayer;
        //netBuffer.cursor = netBuffer.msg.data;
        reboundPacket = false;
        return true;
    }

#ifdef __CLIENT__
    if(playback)
    {   // We're playing a demo. This overrides all other packets.
        return Demo_ReadPacket();
    }
#endif

    if(!netGame)
    {   // Packets cannot be received.
        return false;
    }

    if(!N_GetPacket())
        return false;

#ifdef __CLIENT__
    // Are we recording a demo?
    if(isClient && clients[consolePlayer].recording)
    {
        Demo_WritePacket(consolePlayer);
    }
#endif

    return true;
}

#undef Net_PlayerSmoother
DENG_EXTERN_C Smoother* Net_PlayerSmoother(int player)
{
    if(player < 0 || player >= DDMAXPLAYERS)
        return 0;

    return clients[player].smoother;
}

void Net_SendPlayerInfo(int srcPlrNum, int destPlrNum)
{
    size_t nameLen;

    DENG_ASSERT(srcPlrNum >= 0 && srcPlrNum < DDMAXPLAYERS);
    nameLen = strlen(clients[srcPlrNum].name);

    LOG_AS("Net_SendPlayerInfo");
    LOGDEV_NET_VERBOSE("src=%i dest=%i name=%s")
            << srcPlrNum << destPlrNum << clients[srcPlrNum].name;

    Msg_Begin(PKT_PLAYER_INFO);
    Writer_WriteByte(msgWriter, srcPlrNum);
    Writer_WriteUInt16(msgWriter, nameLen);
    Writer_Write(msgWriter, clients[srcPlrNum].name, nameLen);
    Msg_End();
    Net_SendBuffer(destPlrNum, 0);
}

/**
 * This is the public interface of the message sender.
 */
#undef Net_SendPacket
DENG_EXTERN_C void Net_SendPacket(int to_player, int type, const void* data, size_t length)
{
    unsigned int flags = 0;

#ifndef DENG_WRITER_TYPECHECK
    Msg_Begin(type);
    if(data) Writer_Write(msgWriter, data, length);
    Msg_End();
#else
    assert(length <= NETBUFFER_MAXSIZE);
    netBuffer.msg.type = type;
    netBuffer.length = length;
    if(data) memcpy(netBuffer.msg.data, data, length);
#endif

    if(isClient)
    {   // As a client we can only send messages to the server.
        Net_SendBuffer(0, flags);
    }
    else
    {   // The server can send packets to any player.
        // Only allow sending to the sixteen possible players.
        Net_SendBuffer(to_player & DDSP_ALL_PLAYERS ? NSP_BROADCAST
                       : (to_player & 0xf), flags);
    }
}

/**
 * Prints the message in the console.
 */
void Net_ShowChatMessage(int plrNum, const char* message)
{
    const char* fromName = (plrNum > 0? clients[plrNum].name : "[sysop]");
    const char* sep      = (plrNum > 0? ":"                  : "");
    LOG_NOTE("%s%s%s %s")
            << (!plrNum? _E(1) : _E(D))
            << fromName << sep << message;
}

/**
 * After a long period with no updates (map setup), calling this will reset
 * the tictimer so that no time seems to have passed.
 */
void Net_ResetTimer(void)
{
    int i;

    firstNetUpdate = true;

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        if(/*!clients[i].connected ||*/ !clients[i].smoother) continue;
        Smoother_Clear(clients[i].smoother);
    }
}

/**
 * @return @c true, if the specified player is a real, local player.
 */
dd_bool Net_IsLocalPlayer(int plrNum)
{
    player_t *plr = &ddPlayers[plrNum];

    return plr->shared.inGame && (plr->shared.flags & DDPF_LOCAL);
}

/**
 * Send the local player(s) ticcmds to the server.
 */
void Net_SendCommands(void)
{
}

static void Net_DoUpdate(void)
{
    static int          lastTime = 0;

    int                 nowTime, newTics;

    /**
     * This timing is only used by the client when it determines if it is
     * time to send ticcmds or coordinates to the server.
     */

    // Check time.
    nowTime = Timer_Ticks();

    // Clock reset?
    if(firstNetUpdate)
    {
        firstNetUpdate = false;
        lastTime = nowTime;
    }
    newTics = nowTime - lastTime;
    if(newTics <= 0)
        return; // Nothing new to update.

    lastTime = nowTime;

    // This is as far as dedicated servers go.
#ifdef __CLIENT__

    /**
     * Clients will periodically send their coordinates to the server so
     * any prediction errors can be fixed. Client movement is almost
     * entirely local.
     */

    coordTimer -= newTics;
    if(isClient && coordTimer <= 0 &&
       ddPlayers[consolePlayer].shared.mo)
    {
        mobj_t *mo = ddPlayers[consolePlayer].shared.mo;

        coordTimer = 1; //netCoordTime; // 35/2

        Msg_Begin(PKT_COORDS);
        Writer_WriteFloat(msgWriter, gameTime);
        Writer_WriteFloat(msgWriter, mo->origin[VX]);
        Writer_WriteFloat(msgWriter, mo->origin[VY]);
        if(mo->origin[VZ] == mo->floorZ)
        {
            // This'll keep us on the floor even in fast moving sectors.
            Writer_WriteInt32(msgWriter, DDMININT);
        }
        else
        {
            Writer_WriteInt32(msgWriter, FLT2FIX(mo->origin[VZ]));
        }
        // Also include angles.
        Writer_WriteUInt16(msgWriter, mo->angle >> 16);
        Writer_WriteInt16(msgWriter, P_LookDirToShort(ddPlayers[consolePlayer].shared.lookDir));
        // Control state.
        Writer_WriteChar(msgWriter, FLT2FIX(ddPlayers[consolePlayer].shared.forwardMove) >> 13);
        Writer_WriteChar(msgWriter, FLT2FIX(ddPlayers[consolePlayer].shared.sideMove) >> 13);
        Msg_End();

        Net_SendBuffer(0, 0);
    }
#endif // __CLIENT__
}

/**
 * Handle incoming packets, clients send ticcmds and coordinates to
 * the server.
 */
void Net_Update(void)
{
    Net_DoUpdate();

    // Check for received packets.
#ifdef __CLIENT__
    Cl_GetPackets();
#endif
}

void Net_AllocClientBuffers(int clientId)
{
    if(clientId < 0 || clientId >= DDMAXPLAYERS) return;

    assert(!clients[clientId].smoother);

    // Movement smoother.
    clients[clientId].smoother = Smoother_New();
}

void Net_DestroyArrays(void)
{
    int i;

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        if(clients[i].smoother)
        {
            Smoother_Delete(clients[i].smoother);
        }
    }

    memset(clients, 0, sizeof(clients));
}

/**
 * This is the network one-time initialization (into single-player mode).
 */
void Net_InitGame(void)
{
#ifdef __CLIENT__
    Cl_InitID();
#endif

    // In single-player mode there is only player number zero.
    consolePlayer = displayPlayer = 0;

    // We're in server mode if we aren't a client.
    isServer = true;

    // Netgame is true when we're aware of the network (i.e. other players).
    netGame = false;

    ddPlayers[0].shared.inGame = true;
    ddPlayers[0].shared.flags |= DDPF_LOCAL;

#ifdef __CLIENT__
    clients[0].id = clientID;
#endif
    clients[0].ready = true;
    clients[0].connected = true;
    clients[0].viewConsole = 0;
    clients[0].lastTransmit = -1;
}

void Net_StopGame()
{
    LOG_AS("Net_StopGame");

#ifdef __SERVER__
    if(isServer)
    {
        // We are an open server.
        // This means we should inform all the connected clients that the
        // server is about to close.
        Msg_Begin(PSV_SERVER_CLOSE);
        Msg_End();
        Net_SendBuffer(NSP_BROADCAST, 0);
    }
#endif

#ifdef __CLIENT__
    LOGDEV_NET_MSG("Sending PCL_GOODBYE");

    // We are a connected client.
    Msg_Begin(PCL_GOODBYE);
    Msg_End();
    Net_SendBuffer(0, 0);

    // Must stop recording, we're disconnecting.
    Demo_StopRecording(consolePlayer);
    Cl_CleanUp();
    isClient = false;
    netLoggedIn = false;
#endif

    // Netgame has ended.
    netGame = false;
    isServer = true;
    allowSending = false;

#ifdef __SERVER__
    // No more remote users.
    netRemoteUser = 0;
#endif

    // All remote players are forgotten.
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        player_t *plr = &ddPlayers[i];
        client_t *cl = &clients[i];

        plr->shared.inGame = false;
        cl->ready = cl->connected = false;
        cl->id = 0;
        cl->nodeID = 0;
        cl->viewConsole = -1;
        plr->shared.flags &= ~(DDPF_CAMERA | DDPF_CHASECAM | DDPF_LOCAL);
    }

    // We're about to become player zero, so update it's view angles to
    // match our current ones.
    if(ddPlayers[0].shared.mo)
    {
        /* $unifiedangles */
        ddPlayers[0].shared.mo->angle =
            ddPlayers[consolePlayer].shared.mo->angle;
        ddPlayers[0].shared.lookDir =
            ddPlayers[consolePlayer].shared.lookDir;
    }

    LOGDEV_NET_NOTE("Reseting console and view players to zero");

    consolePlayer = displayPlayer = 0;
    ddPlayers[0].shared.inGame = true;
    clients[0].ready = true;
    clients[0].connected = true;
    clients[0].viewConsole = 0;
    ddPlayers[0].shared.flags |= DDPF_LOCAL;
}

/**
 * @return              Delta based on 'now' (- future, + past).
 */
int Net_TimeDelta(byte now, byte then)
{
    int                 delta;

    if(now >= then)
    {   // Simple case.
        delta = now - then;
    }
    else
    {   // There's a wraparound.
        delta = 256 - then + now;
    }

    // The time can be in the future. We'll allow one second.
    if(delta > 220)
        delta -= 256;

    return delta;
}

#ifdef __CLIENT__
/// @return  @c true iff a demo is currently being recorded.
static dd_bool recordingDemo(void)
{
    int i;
    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        if(ddPlayers[i].shared.inGame && clients[i].recording)
            return true;
    }
    return false;
}
#endif

#ifdef __CLIENT__

void Net_DrawDemoOverlay(void)
{
    char buf[160], tmp[40];
    int x = DENG_GAMEVIEW_WIDTH - 10, y = 10;

    if(!recordingDemo() || !(SECONDS_TO_TICKS(gameTime) & 8))
        return;

    strcpy(buf, "[");
    { int i, c;
    for(i = c = 0; i < DDMAXPLAYERS; ++i)
    {
        if(!(!ddPlayers[i].shared.inGame || !clients[i].recording))
        {
            // This is a "real" player (or camera).
            if(c++)
                strcat(buf, ",");

            sprintf(tmp, "%i:%s", i, clients[i].recordPaused ? "-P-" : "REC");
            strcat(buf, tmp);
        }
    }}
    strcat(buf, "]");

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Go into screen projection mode.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT, 0, -1, 1);

    glEnable(GL_TEXTURE_2D);

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(1, 1, 1, 1);
    FR_DrawTextXY3(buf, x, y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

    glDisable(GL_TEXTURE_2D);

    // Restore original matrix.
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

#endif // __CLIENT__

void Net_Drawer()
{
#ifdef __CLIENT__
    // Draw the blockmap debug display.
    Rend_BlockmapDebug();

    // Draw the light range debug display.
    Rend_DrawLightModMatrix();

# ifdef DENG2_DEBUG
    // Draw the input debug display.
    I_DebugDrawer();
# endif

    // Draw the demo recording overlay.
    Net_DrawDemoOverlay();

# ifdef DENG2_DEBUG
    Z_DebugDrawer();
# endif
#endif // __CLIENT__
}

void Net_Ticker(timespan_t time)
{
    int         i;
    client_t   *cl;

    // Network event ticker.
    N_NETicker(time);

#ifdef __SERVER__
    if(netDev)
    {
        static int printTimer = 0;

        if(printTimer++ > TICSPERSEC)
        {
            printTimer = 0;
            for(i = 0; i < DDMAXPLAYERS; ++i)
            {
                if(Sv_IsFrameTarget(i))
                {
                    LOGDEV_NET_MSG("%i(rdy%i): avg=%05ims thres=%05ims "
                                   "bwr=%05i maxfs=%05ib unakd=%05i")
                            << i << clients[i].ready << 0 << 0
                            << clients[i].bandwidthRating
                            << Sv_GetMaxFrameSize(i)
                            << Sv_CountUnackedDeltas(i);
                }
            }
        }
    }
#endif // __SERVER__

    // The following stuff is only for netgames.
    if(!netGame)
        return;

    // Check the pingers.
    for(i = 0, cl = clients; i < DDMAXPLAYERS; ++i, cl++)
    {
        // Clients can only ping the server.
        if(!(isClient && i) && i != consolePlayer)
        {
            if(cl->ping.sent)
            {
                // The pinger is active.
                if(Timer_RealMilliseconds() - cl->ping.sent > PING_TIMEOUT)    // Timed out?
                {
                    cl->ping.times[cl->ping.current] = -1;
                    Net_SendPing(i, 0);
                }
            }
        }
    }
}

de::String ServerInfo_AsStyledText(serverinfo_t const *sv)
{
#define TABBED(A, B) _E(Ta)_E(l) "  " A _E(.) " " _E(\t) B "\n"
    return de::String(_E(b) "%1" _E(.) "\n%2\n" _E(T`)
                      TABBED("Joinable:", "%5")
                      TABBED("Players:", "%3 / %4%12")
                      TABBED("Game:", "%8\n%9\n%11 %10")
                      TABBED("PWADs:", "%13")
                      TABBED("Address:", "%6:%7")
                      /*TABBED("Ping:", "%8 ms (approx)")*/)
            .arg(sv->name)
            .arg(sv->description)
            .arg(sv->numPlayers)
            .arg(sv->maxPlayers)
            .arg(sv->canJoin? "Yes" : "No") // 5
            .arg(sv->address)
            .arg(sv->port)
            //.arg(sv->ping)
            .arg(sv->plugin)
            .arg(sv->gameIdentityKey) // 10
            .arg(sv->gameConfig)
            .arg(sv->map)
            .arg(!de::String(sv->clientNames).isEmpty()? de::String(_E(2) " (%1)" _E(.)).arg(sv->clientNames) : "")
            .arg(de::String(sv->pwads).isEmpty()? de::String(DENG2_CHAR_MDASH) : de::String(sv->pwads)); // 14
#undef TABBED
}

/**
 * Prints server/host information into the console. The header line is
 * printed if 'info' is NULL.
 */
void ServerInfo_Print(serverinfo_t const *info, int index)
{
    /// @todo Update table for de::Log. -jk
    ///
    if(!info)
    {
        LOG_NET_MSG(_E(m)"    %-20s P/M  L Ver:  Game:            Location:") << "Name:";
    }
    else
    {
        LOG_NET_MSG(_E(m)"%-2i: %-20s %i/%-2i %c %-5i %-16s %s:%i")
                << index << info->name << info->numPlayers << info->maxPlayers
                << (info->canJoin? ' ' : '*') << info->version << info->plugin
                << info->address << info->port;
        LOG_NET_MSG("    %s p:%ims %-40s") << info->map << info->ping << info->description;
        LOG_NET_MSG("    %s (CRC:%x) %s") << info->gameIdentityKey << info->loadedFilesCRC << info->gameConfig;

        // Optional: PWADs in use.
        if(info->pwads[0])
            LOG_NET_MSG("    PWADs: %s") << info->pwads;

        // Optional: names of players.
        if(info->clientNames[0])
            LOG_NET_MSG("    Players: %s") << info->clientNames;

        // Optional: data values.
        if(info->data[0] || info->data[1] || info->data[2])
        {
            LOG_NET_MSG("    Data: (%08x, %08x, %08x)") << info->data[0]
                    << info->data[1] << info->data[2];
        }
    }
}

/**
 * Composes a PKT_CHAT network message.
 */
void Net_WriteChatMessage(int from, int toMask, const char* message)
{
    size_t len = strlen(message);
    len = MIN_OF(len, 0xffff);

    Msg_Begin(PKT_CHAT);
    Writer_WriteByte(msgWriter, from);
    Writer_WriteUInt32(msgWriter, toMask);
    Writer_WriteUInt16(msgWriter, len);
    Writer_Write(msgWriter, message, len);
    Msg_End();
}

/**
 * All arguments are sent out as a chat message.
 */
D_CMD(Chat)
{
    DENG2_UNUSED(src);

    char    buffer[100];
    int     i, mode = !stricmp(argv[0], "chat") ||
        !stricmp(argv[0], "say") ? 0 : !stricmp(argv[0], "chatNum") ||
        !stricmp(argv[0], "sayNum") ? 1 : 2;
    unsigned short mask = 0;

    if(argc == 1)
    {
        LOG_SCR_NOTE("Usage: %s %s(text)") << argv[0]
                << (!mode ? "" : mode == 1 ? "(plr#) " : "(name) ");
        LOG_SCR_MSG("Chat messages are max 80 characters long. Use quotes to get around "
                    "arg processing.");
        return true;
    }

    LOG_AS("chat (Cmd)");

    // Chatting is only possible when connected.
    if(!netGame)
        return false;

    // Too few arguments?
    if(mode && argc < 3)
        return false;

    // Assemble the chat message.
    strcpy(buffer, argv[!mode ? 1 : 2]);
    for(i = (!mode ? 2 : 3); i < argc; ++i)
    {
        strcat(buffer, " ");
        strncat(buffer, argv[i], 80 - (strlen(buffer) + strlen(argv[i]) + 1));
    }
    buffer[80] = 0;

    // Send the message.
    switch(mode)
    {
    case 0: // chat
        mask = ~0;
        break;

    case 1: // chatNum
        mask = 1 << atoi(argv[1]);
        break;

    case 2: // chatTo
        {
        dd_bool     found = false;

        for(i = 0; i < DDMAXPLAYERS && !found; ++i)
        {
            if(!stricmp(clients[i].name, argv[1]))
            {
                mask = 1 << i;
                found = true;
            }
        }
        break;
        }

    default:
        LOG_SCR_ERROR("Invalid value, mode = %i") << mode;
        break;
    }

    Net_WriteChatMessage(consolePlayer, mask, buffer);

    if(!isClient)
    {
        if(mask == (unsigned short) ~0)
        {
            Net_SendBuffer(NSP_BROADCAST, 0);
        }
        else
        {
            for(i = 1; i < DDMAXPLAYERS; ++i)
                if(ddPlayers[i].shared.inGame && (mask & (1 << i)))
                    Net_SendBuffer(i, 0);
        }
    }
    else
    {
        Net_SendBuffer(0, 0);
    }

    // Show the message locally.
    Net_ShowChatMessage(consolePlayer, buffer);

    // Inform the game, too.
    gx.NetPlayerEvent(consolePlayer, DDPE_CHAT_MESSAGE, buffer);
    return true;
}

#ifdef __SERVER__
D_CMD(Kick)
{
    DENG2_UNUSED2(src, argc);

    int     num;

    LOG_AS("kick (Cmd)")

    if(!netGame)
    {
        LOG_SCR_ERROR("This is not a network game");
        return false;
    }

    if(!isServer)
    {
        LOG_SCR_ERROR("Only allowed on the server");
        return false;
    }

    num = atoi(argv[1]);
    if(num < 1 || num >= DDMAXPLAYERS)
    {
        LOG_NET_ERROR("Invalid client number");
        return false;
    }

    if(netRemoteUser == num)
    {
        LOG_NET_ERROR("Can't kick the client who's logged in");
        return false;
    }

    Sv_Kick(num);
    return true;
}
#endif // __SERVER__

#ifdef __CLIENT__
D_CMD(SetName)
{
    DENG2_UNUSED2(src, argc);

    Con_SetString("net-name", argv[1]);

    if(!netGame)
        return true;

    // The server does not have a name.
    if(!isClient) return false;

    memset(clients[consolePlayer].name, 0, sizeof(clients[consolePlayer].name));
    strncpy(clients[consolePlayer].name, argv[1], PLAYERNAMELEN - 1);

    Net_SendPlayerInfo(consolePlayer, 0);
    return true;
}
#endif

D_CMD(SetTicks)
{
    DENG2_UNUSED2(src, argc);

//  extern double lastSharpFrameTime;

    firstNetUpdate = true;
    Timer_SetTicksPerSecond(strtod(argv[1], 0));
//  lastSharpFrameTime = Sys_GetTimef();
    return true;
}

D_CMD(MakeCamera)
{
    DENG2_UNUSED2(src, argc);

    /*  int cp;
       mobj_t *mo;
       ddplayer_t *conp = players + consolePlayer;

       if(argc < 2) return true;
       cp = atoi(argv[1]);
       clients[cp].connected = true;
       clients[cp].ready = true;
       clients[cp].updateCount = UPDATECOUNT;
       ddPlayers[cp].flags |= DDPF_CAMERA;
       ddPlayers[cp].inGame = true; // !!!
       Sv_InitPoolForClient(cp);
       mo = Z_Malloc(sizeof(mobj_t), PU_MAP, 0);
       memset(mo, 0, sizeof(*mo));
       mo->origin[VX] = conp->mo->origin[VX];
       mo->origin[VY] = conp->mo->origin[VY];
       mo->origin[VZ] = conp->mo->origin[VZ];
       mo->bspLeaf = conp->mo->bspLeaf;
       ddPlayers[cp].mo = mo;
       displayPlayer = cp; */

    // Create a new local player.
    int cp;

    LOG_AS("makecam (Cmd)");

    cp = atoi(argv[1]);
    if(cp < 0 || cp >= DDMAXPLAYERS)
        return false;

    if(clients[cp].connected)
    {
        LOG_ERROR("Client %i already connected") << cp;
        return false;
    }

    clients[cp].connected = true;
    clients[cp].ready = true;
    clients[cp].viewConsole = cp;
    ddPlayers[cp].shared.flags |= DDPF_LOCAL;
    Smoother_Clear(clients[cp].smoother);

#ifdef __SERVER__
    Sv_InitPoolForClient(cp);
#endif

#ifdef __CLIENT__
    R_SetupDefaultViewWindow(cp);

    // Update the viewports.
    R_SetViewGrid(0, 0);
#endif

    return true;
}

#ifdef __CLIENT__

D_CMD(SetConsole)
{
    DENG2_UNUSED2(src, argc);

    int cp = atoi(argv[1]);
    if(ddPlayers[cp].shared.inGame)
    {
        consolePlayer = displayPlayer = cp;
    }

    // Update the viewports.
    R_SetViewGrid(0, 0);
    return true;
}

int Net_StartConnection(const char* address, int port)
{
    LOG_AS("Net_StartConnection");
    LOG_NET_MSG("Connecting to %s (port %i)...") << address << port;

    // Start searching at the specified location.
    Net_ServerLink().connectDomain(de::String("%1:%2").arg(address).arg(port), 7 /*timeout*/);
    return true;
}

/**
 * Intelligently connect to a server. Just provide an IP address and the
 * rest is automatic.
 */
D_CMD(Connect)
{
    DENG2_UNUSED(src);

    char *ptr;
    int port = 0;

    if(argc < 2 || argc > 3)
    {
        LOG_SCR_NOTE("Usage: %s (ip-address) [port]") << argv[0];
        LOG_SCR_MSG("A TCP/IP connection is created to the given server. If a port is not "
                    "specified port zero will be used.");
        return true;
    }

    if(netGame)
    {
        LOG_NET_ERROR("Already connected");
        return false;
    }

    // If there is a port specified in the address, use it.
    port = 0;
    if((ptr = strrchr(argv[1], ':')))
    {
        port = strtol(ptr + 1, 0, 0);
        *ptr = 0;
    }
    if(argc == 3)
    {
        port = strtol(argv[2], 0, 0);
    }

    return Net_StartConnection(argv[1], port);
}

#endif // __CLIENT__

/**
 * The 'net' console command.
 */
D_CMD(Net)
{
    DENG2_UNUSED(src);

    dd_bool success = true;

    if(argc == 1) // No args?
    {
        LOG_SCR_NOTE("Usage: %s (cmd/args)") << argv[0];
        LOG_SCR_MSG("Commands:");
        LOG_SCR_MSG("  init");
        LOG_SCR_MSG("  shutdown");
        LOG_SCR_MSG("  info");
        LOG_SCR_MSG("  request");
#ifdef __CLIENT__
        LOG_SCR_MSG("  setup client");
        LOG_SCR_MSG("  search (address) [port]   (local or targeted query)");
        LOG_SCR_MSG("  servers   (asks the master server)");
        LOG_SCR_MSG("  connect (idx)");
        LOG_SCR_MSG("  mconnect (m-idx)");
        LOG_SCR_MSG("  disconnect");
#endif
#ifdef __SERVER__
        LOG_SCR_MSG("  announce");
#endif
        return true;
    }

    if(argc == 2) // One argument?
    {
        if(!stricmp(argv[1], "announce"))
        {
            N_MasterAnnounceServer(true);
        }
        else if(!stricmp(argv[1], "request"))
        {
            N_MasterRequestList();
        }
        else if(!stricmp(argv[1], "servers"))
        {
            N_MAPost(MAC_REQUEST);
            N_MAPost(MAC_WAIT);
            N_MAPost(MAC_LIST);
        }
        else if(!stricmp(argv[1], "info"))
        {
            N_PrintNetworkStatus();
            LOG_NET_MSG("Network game: %b") << netGame;
            LOG_NET_MSG("This is console %i (local player %i)") << consolePlayer << P_ConsoleToLocal(consolePlayer);
        }
#ifdef __CLIENT__
        else if(!stricmp(argv[1], "disconnect"))
        {
            if(!netGame)
            {
                LOG_NET_ERROR("This client is not connected to a server");
                return false;
            }

            if(!isClient)
            {
                LOG_NET_ERROR("This is not a client");
                return false;
            }

            Net_ServerLink().disconnect();

            LOG_NET_NOTE("Disconnected");
        }
#endif
        else
        {
            LOG_SCR_ERROR("Invalid arguments");
            return false; // Bad args.
        }
    }

    if(argc == 3) // Two arguments?
    {
#ifdef __CLIENT__
        if(!stricmp(argv[1], "search"))
        {
            Net_ServerLink().discover(argv[2]);
        }
        else if(!stricmp(argv[1], "connect"))
        {
            if(netGame)
            {
                LOG_NET_ERROR("Already connected");
                return false;
            }

            int index = strtoul(argv[2], 0, 10);
            serverinfo_t info;
            if(Net_ServerLink().foundServerInfo(index, &info))
            {
                ServerInfo_Print(&info, index);
                Net_ServerLink().connectDomain(de::String("%1:%2").arg(info.address).arg(info.port), 5);
            }
        }
        else if(!stricmp(argv[1], "mconnect"))
        {
            serverinfo_t    info;

            if(N_MasterGet(strtol(argv[2], 0, 0), &info))
            {
                // Connect using TCP/IP.
                return Con_Executef(CMDS_CONSOLE, false, "connect %s %i",
                                    info.address, info.port);
            }
            else
                return false;
        }
        else if(!stricmp(argv[1], "setup"))
        {
            // Start network setup.
            if(!stricmp(argv[2], "client"))
            {
                ClientWindow::main().taskBar().close();
                ClientWindow::main().taskBar().showMultiplayer();
            }
            else return false;
        }
#endif
    }

#ifdef __CLIENT__
    if(argc == 4)
    {
        if(!stricmp(argv[1], "search"))
        {
            //success = N_LookForHosts(argv[2], strtol(argv[3], 0, 0), 0);
            Net_ServerLink().discover(de::String(argv[2]) + ":" + argv[3]);
        }
    }
#endif

    return success;
}

/**
 * Extracts the label and value from a string.  'max' is the maximum
 * allowed length of a token, including terminating \0.
 */
static dd_bool tokenize(char const *line, char *label, char *value, int max)
{
    const char *src = line;
    const char *colon = strchr(src, ':');

    // The colon must exist near the beginning.
    if(!colon || colon - src >= SVINFO_VALID_LABEL_LEN)
        return false;

    // Copy the label.
    memset(label, 0, max);
    strncpy(label, src, MIN_OF(colon - src, max - 1));

    // Copy the value.
    memset(value, 0, max);
    strncpy(value, colon + 1, MIN_OF(strlen(line) - (colon - src + 1), (unsigned) max - 1));

    // Everything is OK.
    return true;
}

void ServerInfo_FromRecord(serverinfo_t *info, de::Record const &rec)
{
    de::zapPtr(info);

    info->port           = (int)  rec["port"].value().asNumber();
    info->version        = (int)  rec["ver" ].value().asNumber();
    info->loadedFilesCRC = (uint) rec["wcrc"].value().asNumber();
    info->numPlayers     = (int)  rec["nump"].value().asNumber();
    info->maxPlayers     = (int)  rec["maxp"].value().asNumber();
    info->canJoin        =        rec["open"].value().isTrue();

#define COPY_STR(Member, VarName) \
    strncpy(Member, rec[VarName].value().asText().toUtf8(), sizeof(Member) - 1);

    COPY_STR(info->name,            "name" );
    COPY_STR(info->description,     "info" );
    COPY_STR(info->plugin,          "game" );
    COPY_STR(info->gameIdentityKey, "mode" );
    COPY_STR(info->gameConfig,      "setup");
    COPY_STR(info->iwad,            "iwad" );
    COPY_STR(info->pwads,           "pwads");
    COPY_STR(info->map,             "map"  );
    COPY_STR(info->clientNames,     "plrn" );

#undef COPY_STR
}

dd_bool ServerInfo_FromString(serverinfo_t *info, char const *valuePair)
{
    char label[SVINFO_TOKEN_LEN], value[SVINFO_TOKEN_LEN];

    // Extract the label and value. The maximum length of a value is
    // TOKEN_LEN. Labels are returned in lower case.
    if(!tokenize(valuePair, label, value, sizeof(value)))
    {
        // Badly formed lines are ignored.
        return false;
    }

    if(!strcmp(label, "at"))
    {
        strncpy(info->address, value, sizeof(info->address) - 1);
    }
    else if(!strcmp(label, "port"))
    {
        info->port = strtol(value, 0, 0);
    }
    else if(!strcmp(label, "ver"))
    {
        info->version = strtol(value, 0, 0);
    }
    else if(!strcmp(label, "map"))
    {
        strncpy(info->map, value, sizeof(info->map) - 1);
    }
    else if(!strcmp(label, "game"))
    {
        strncpy(info->plugin, value, sizeof(info->plugin) - 1);
    }
    else if(!strcmp(label, "name"))
    {
        strncpy(info->name, value, sizeof(info->name) - 1);
    }
    else if(!strcmp(label, "info"))
    {
        strncpy(info->description, value, sizeof(info->description) - 1);
    }
    else if(!strcmp(label, "nump"))
    {
        info->numPlayers = strtol(value, 0, 0);
    }
    else if(!strcmp(label, "maxp"))
    {
        info->maxPlayers = strtol(value, 0, 0);
    }
    else if(!strcmp(label, "open"))
    {
        info->canJoin = strtol(value, 0, 0);
    }
    else if(!strcmp(label, "mode"))
    {
        strncpy(info->gameIdentityKey, value, sizeof(info->gameIdentityKey) - 1);
    }
    else if(!strcmp(label, "setup"))
    {
        strncpy(info->gameConfig, value, sizeof(info->gameConfig) - 1);
    }
    else if(!strcmp(label, "iwad"))
    {
        strncpy(info->iwad, value, sizeof(info->iwad) - 1);
    }
    else if(!strcmp(label, "wcrc"))
    {
        info->loadedFilesCRC = strtol(value, 0, 0);
    }
    else if(!strcmp(label, "pwads"))
    {
        strncpy(info->pwads, value, sizeof(info->pwads) - 1);
    }
    else if(!strcmp(label, "plrn"))
    {
        strncpy(info->clientNames, value, sizeof(info->clientNames) - 1);
    }
    else if(!strcmp(label, "data0"))
    {
        info->data[0] = strtol(value, 0, 16);
    }
    else if(!strcmp(label, "data1"))
    {
        info->data[1] = strtol(value, 0, 16);
    }
    else if(!strcmp(label, "data2"))
    {
        info->data[2] = strtol(value, 0, 16);
    }
    else
    {
        // Unknown labels are ignored.
        return false;
    }
    return true;
}

de::String Net_UserAgent()
{
    return QString(DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_TEXT) +
           " (" + de::Version().operatingSystem() + ")";
}
