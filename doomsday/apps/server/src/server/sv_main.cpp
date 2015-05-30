/** @file sv_main.cpp Network server.
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

#include <math.h>

#define DENG_NO_API_MACROS_SERVER
#include "api_server.h"

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_filesys.h"
#include "de_network.h"
#include "de_play.h"
#include "de_misc.h"
#include "de_defs.h"

#include "api_materialarchive.h"

#include <de/ArrayValue>
#include <de/NumberValue>
#include <de/Log>

using namespace de;

// This is absolute maximum bandwidth rating. Frame size is practically
// unlimited with this score.
#define MAX_BANDWIDTH_RATING    100

// When the difference between clientside and serverside positions is this
// much, server will update its position to match the clientside position,
// which is assumed to be correct.
#define WARP_LIMIT              300

void    Sv_ClientCoords(int playerNum);

int     netRemoteUser = 0; // The client who is currently logged in.
char   *netPassword = (char *) ""; // Remote login password.

// This is the limit when accepting new clients.
int     svMaxPlayers = DDMAXPLAYERS;

static MaterialArchive *materialDict;

/**
 * @defgroup pathToStringFlags  Path To String Flags
 * @ingroup flags
 */
///@{
#define PTSF_QUOTED                     0x1 ///< Add double quotes around the path.
#define PTSF_TRANSFORM_EXCLUDE_PATH     0x2 ///< Exclude the path; e.g., c:/doom/myaddon.wad => myaddon.wad
#define PTSF_TRANSFORM_EXCLUDE_EXT      0x4 ///< Exclude the extension; e.g., c:/doom/myaddon.wad => c:/doom/myaddon
///@}

#define DEFAULT_PATHTOSTRINGFLAGS       (PTSF_QUOTED)

/**
 * @param files      List of files from which to compose the path string.
 * @param flags      @ref pathToStringFlags
 * @param delimiter  If not @c NULL, path fragments in the resultant string
 *                   will be delimited by this.
 *
 * @return  New string containing a concatenated, possibly delimited set of
 *          all file paths in the list.
 */
static String composeFilePathString(FS1::FileList &files, int flags = DEFAULT_PATHTOSTRINGFLAGS,
                                    String const &delimiter = ";")
{
    String result;
    DENG2_FOR_EACH_CONST(FS1::FileList, i, files)
    {
        File1 &file = (*i)->file();

        if(flags & PTSF_QUOTED)
            result.append('"');

        if(flags & PTSF_TRANSFORM_EXCLUDE_PATH)
        {
            if(flags & PTSF_TRANSFORM_EXCLUDE_EXT)
                result.append(file.name().fileNameWithoutExtension());
            else
                result.append(file.name());
        }
        else
        {
            String path = file.composePath();
            if(flags & PTSF_TRANSFORM_EXCLUDE_EXT)
            {
                result.append(path.fileNamePath() + '/' + path.fileNameWithoutExtension());
            }
            else
            {
                result.append(path);
            }
        }

        if(flags & PTSF_QUOTED)
            result.append('"');

        if(*i != files.last())
            result.append(delimiter);
    }

    return result;
}

static bool findCustomFilesPredicate(File1 &file, void * /*parameters*/)
{
    return file.hasCustom();
}

/**
 * Compiles a list of file names, separated by @a delimiter.
 */
static void composePWADFileList(char *outBuf, size_t outBufSize, char const *delimiter)
{
    if(!outBuf || 0 == outBufSize) return;
    memset(outBuf, 0, outBufSize);

    FS1::FileList foundFiles;
    if(!App_FileSystem().findAll<de::Wad>(findCustomFilesPredicate, 0/*no params*/, foundFiles)) return;

    String str = composeFilePathString(foundFiles, PTSF_TRANSFORM_EXCLUDE_PATH, delimiter);
    QByteArray strUtf8 = str.toUtf8();
    strncpy(outBuf, strUtf8.constData(), outBufSize);
}

/**
 * Fills the provided struct with information about the local server.
 */
void Sv_GetInfo(serverinfo_t *info)
{
    DENG_ASSERT(info != 0);

    de::zapPtr(info);

    Map &map = App_WorldSystem().map();

    // Let's figure out what we want to tell about ourselves.
    info->version = DOOMSDAY_VERSION;
    dd_snprintf(info->plugin, sizeof(info->plugin) - 1, "%s %s", (char *) gx.GetVariable(DD_PLUGIN_NAME), (char *) gx.GetVariable(DD_PLUGIN_VERSION_SHORT));
    strncpy(info->gameIdentityKey, App_CurrentGame().identityKey().toUtf8().constData(), sizeof(info->gameIdentityKey) - 1);
    strncpy(info->gameConfig, (char const *) gx.GetVariable(DD_GAME_CONFIG), sizeof(info->gameConfig) - 1);
    strncpy(info->name, serverName, sizeof(info->name) - 1);
    strncpy(info->description, serverInfo, sizeof(info->description) - 1);
    info->numPlayers = Sv_GetNumPlayers();

    // The server player is there, it's just hidden.
    info->maxPlayers = DDMAXPLAYERS - (isDedicated ? 1 : 0);

    // Don't go over the limit.
    if(info->maxPlayers > svMaxPlayers)
        info->maxPlayers = svMaxPlayers;

    info->canJoin = (isServer != 0 && Sv_GetNumPlayers() < svMaxPlayers);

    // Identifier of the current map.
    String mapPath = (map.def()? map.def()->composeUri().path() : "(unknown map)");
    qstrncpy(info->map, mapPath.toUtf8().constData(), sizeof(info->map) - 1);

    // These are largely unused at the moment... Mainly intended for
    // the game's custom values.
    std::memcpy(info->data, serverData, sizeof(info->data));

    // Also include the port we're using.
    info->port = nptIPPort;

    // Let's compile a list of client names.
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        if(clients[i].connected)
            M_LimitedStrCat(info->clientNames, clients[i].name, 15, ';', sizeof(info->clientNames));
    }

    // Some WAD names.
    composePWADFileList(info->pwads, sizeof(info->pwads), ";");

    // This should be a CRC number that describes all the loaded data.
    info->loadedFilesCRC = App_FileSystem().loadedFilesCRC();;
}

de::Record *Sv_InfoToRecord(serverinfo_t *info)
{
    de::Record *rec = new de::Record;

    rec->addNumber ("port",  info->port);
    rec->addText   ("name",  info->name);
    rec->addText   ("info",  info->description);
    rec->addNumber ("ver",   info->version);
    rec->addText   ("game",  info->plugin);
    rec->addText   ("mode",  info->gameIdentityKey);
    rec->addText   ("setup", info->gameConfig);
    rec->addText   ("iwad",  info->iwad);
    rec->addNumber ("wcrc",  info->loadedFilesCRC);
    rec->addText   ("pwads", info->pwads);
    rec->addText   ("map",   info->map);
    rec->addNumber ("nump",  info->numPlayers);
    rec->addNumber ("maxp",  info->maxPlayers);
    rec->addBoolean("open",  info->canJoin);
    rec->addText   ("plrn",  info->clientNames);

    de::ArrayValue &data = rec->addArray("data").value<de::ArrayValue>();
    for(uint i = 0; i < sizeof(info->data) / sizeof(info->data[0]); ++i)
    {
        data << de::NumberValue(info->data[i]);
    }

    return rec;
}

/**
 * @return  Length of the string.
 */
size_t Sv_InfoToString(serverinfo_t* info, ddstring_t* msg)
{
    unsigned int i;

    Str_Appendf(msg, "port:%i\n", info->port);
    Str_Appendf(msg, "name:%s\n", info->name);
    Str_Appendf(msg, "info:%s\n", info->description);
    Str_Appendf(msg, "ver:%i\n", info->version);
    Str_Appendf(msg, "game:%s\n", info->plugin);
    Str_Appendf(msg, "mode:%s\n", info->gameIdentityKey);
    Str_Appendf(msg, "setup:%s\n", info->gameConfig);
    Str_Appendf(msg, "iwad:%s\n", info->iwad);
    Str_Appendf(msg, "wcrc:%i\n", info->loadedFilesCRC);
    Str_Appendf(msg, "pwads:%s\n", info->pwads);
    Str_Appendf(msg, "map:%s\n", info->map);
    Str_Appendf(msg, "nump:%i\n", info->numPlayers);
    Str_Appendf(msg, "maxp:%i\n", info->maxPlayers);
    Str_Appendf(msg, "open:%i\n", info->canJoin);
    Str_Appendf(msg, "plrn:%s\n", info->clientNames);
    for(i = 0; i < sizeof(info->data) / sizeof(info->data[0]); ++i)
    {
        Str_Appendf(msg, "data%i:%x\n", i, info->data[i]);
    }
    return Str_Length(msg);
}

/**
 * @return              gametic - cmdtime.
 */
int Sv_Latency(byte cmdtime)
{
    return Net_TimeDelta(SECONDS_TO_TICKS(gameTime), cmdtime);
}

/**
 * For local players.
 */
/* $unifiedangles */
/*
void Sv_FixLocalAngles(dd_bool clearFixAnglesFlag)
{
    ddplayer_t *pl;
    int         i;

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        pl = players + i;
        if(!pl->inGame || !(pl->flags & DDPF_LOCAL))
            continue;

        // This is not for clients.
        if(isDedicated && i == 0)
            continue;

        if(pl->flags & DDPF_FIXANGLES)
        {
            if(clearFixAnglesFlag)
            {
                pl->flags &= ~DDPF_FIXANGLES;
            }
            else
            {
                pl->clAngle = pl->mo->angle;
                pl->clLookDir = pl->lookDir;
            }
        }
    }
}
*/

void Sv_HandlePlayerInfoFromClient(client_t* sender)
{
    int console = Reader_ReadByte(msgReader); // ignored
    char oldName[PLAYERNAMELEN];
    size_t len;

    LOG_AS("Sv_HandlePlayerInfoFromClient");

    DENG_ASSERT(netBuffer.player == (sender - clients));
    LOG_NET_VERBOSE("from=%i, console=%i") << netBuffer.player << console;
    console = netBuffer.player;

    strcpy(oldName, sender->name);

    len = Reader_ReadUInt16(msgReader);
    len = MIN_OF(PLAYERNAMELEN - 1, len); // there is a maximum size
    Reader_Read(msgReader, sender->name, len);
    sender->name[len] = 0;

    LOG_NET_NOTE("Player %s renamed to %s") << oldName << sender->name;

    // Relay to others.
    Net_SendPlayerInfo(console, DDSP_ALL_PLAYERS);
}

/**
 * Handles a server-specific network message. Assumes that Msg_BeginRead()
 * has already been called to begin reading the message.
 */
void Sv_HandlePacket(void)
{
    ident_t             id;
    int                 i, mask, from = netBuffer.player;
    player_t           *plr = &ddPlayers[from];
    ddplayer_t         *ddpl = &plr->shared;
    client_t           *sender = &clients[from];
    int                 msgfrom;
    char               *msg;
    char                buf[17];
    size_t              len;

    LOG_AS("Sv_HandlePacket");

    switch(netBuffer.msg.type)
    {
    case PCL_HELLO:
    case PCL_HELLO2:
        // Get the ID of the client.
        id = Reader_ReadUInt32(msgReader);
        LOG_NET_XVERBOSE("Hello from client %i (%08X)") << from << id;

        // Check for duplicate IDs.
        if(!ddpl->inGame && !sender->handshake)
        {
            // Console 0 is always reserved for the server itself (not a player).
            for(i = 1; i < DDMAXPLAYERS; ++i)
            {
                if(clients[i].connected && clients[i].id == id)
                {
                    // Send a message to everybody.
                    LOG_NET_WARNING("New client connection refused: duplicate ID (%08x)") << id;
                    LOGDEV_NET_WARNING("ID conflict from=%i, i=%i") << from << i;
                    N_TerminateClient(from);
                    break;
                }
            }

            if(i < DDMAXPLAYERS)
                break; // Can't continue, refused!
        }

        // This is OK.
        sender->id = id;

        if(netBuffer.msg.type == PCL_HELLO2)
        {
            // Check the game mode (max 16 chars).
            Reader_Read(msgReader, buf, 16);
            if(strnicmp(buf, App_CurrentGame().identityKey().toUtf8().constData(), 16))
            {
                LOG_NET_ERROR("Client's game ID is incompatible: %-.16s") << buf;
                N_TerminateClient(from);
                break;
            }
        }

        // The client requests a handshake.
        if(!ddpl->inGame && !sender->handshake)
        {
            // This'll be true until the client says it's ready.
            sender->handshake = true;

            // The player is now in the game.
            ddPlayers[from].shared.inGame = true;

            // Tell the game about this.
            gx.NetPlayerEvent(from, DDPE_ARRIVAL, 0);

            // Send the handshake packets.
            Sv_Handshake(from, true);

            // Note the time when the player entered.
            sender->enterTime = Timer_RealSeconds();
        }
        else if(ddpl->inGame)
        {
            // The player is already in the game but requests a new
            // handshake. Perhaps it's starting to record a demo.
            Sv_Handshake(from, false);
        }
        break;

    case PKT_OK:
        // The client says it's ready to receive frames.
        sender->ready = true;
        LOG_NET_VERBOSE("OK (\"ready!\") from client %i (%08X)") << from << sender->id;
        if(sender->handshake)
        {
            // The handshake is complete. The client has acknowledged it
            // and sends its regards.
            sender->handshake = false;
            // Send a clock sync message.
            Msg_Begin(PSV_SYNC);
            Writer_WriteFloat(msgWriter, gameTime);
            Msg_End();
            Net_SendBuffer(from, 0);
            // Send welcome string.
            Sv_SendText(from, SV_CONSOLE_PRINT_FLAGS, SV_WELCOME_STRING "\n");
        }
        break;

    case PKT_CHAT:
        // The first byte contains the sender.
        msgfrom = Reader_ReadByte(msgReader);
        // Is the message for us?
        mask = Reader_ReadUInt32(msgReader);
        // Copy the message into a buffer.
        len = Reader_ReadUInt16(msgReader);
        msg = (char *) M_Malloc(len + 1);
        Reader_Read(msgReader, msg, len);
        msg[len] = 0;
        // Message for us? Show it locally.
        if(mask & 1)
        {
            Net_ShowChatMessage(msgfrom, msg);
            gx.NetPlayerEvent(msgfrom, DDPE_CHAT_MESSAGE, msg);
        }

        // Servers relay chat messages to all the recipients.
        Net_WriteChatMessage(msgfrom, mask, msg);
        for(i = 1; i < DDMAXPLAYERS; ++i)
            if(ddPlayers[i].shared.inGame && (mask & (1 << i)) && i != from)
            {
                Net_SendBuffer(i, 0);
            }
        M_Free(msg);
        break;

    case PCL_FINALE_REQUEST: {
        finaleid_t fid = Reader_ReadUInt32(msgReader);
        uint16_t params = Reader_ReadUInt16(msgReader);
        LOGDEV_NET_MSG("PCL_FINALE_REQUEST: fid=%i params=%i") << fid << params;
        if(params == 1)
        {
            // Skip.
            FI_ScriptRequestSkip(fid);
        }
        break; }

    case PKT_PLAYER_INFO:
        Sv_HandlePlayerInfoFromClient(sender);
        break;

    default:
        LOGDEV_NET_ERROR("Invalid value: netBuffer.msg.type = %i") << netBuffer.msg.type;
        break;
    }
}

/**
 * Handles a login packet. If the password is OK and no other client
 * is current logged in, a response is sent.
 */
void Sv_Login(void)
{
    char password[300];
    byte passLen = 0;

    if(netRemoteUser)
    {
        Sv_SendText(netBuffer.player, SV_CONSOLE_PRINT_FLAGS,
                    "Sv_Login: A client is already logged in.\n");
        return;
    }

    LOG_AS("Sv_Login");

    // Check the password.
    passLen = Reader_ReadByte(msgReader);
    de::zap(password);
    Reader_Read(msgReader, password, passLen);
    if(strcmp(password, netPassword))
    {
        Sv_SendText(netBuffer.player, SV_CONSOLE_PRINT_FLAGS,
                    "Sv_Login: Invalid password.\n");
        return;
    }

    // OK!
    netRemoteUser = netBuffer.player;
    LOG_NET_NOTE("%s (client %i) logged in" ) << clients[netRemoteUser].name << netRemoteUser;
    // Send a confirmation packet to the client.
    Msg_Begin(PKT_LOGIN);
    Writer_WriteByte(msgWriter, true);        // Yes, you're logged in.
    Msg_End();
    Net_SendBuffer(netRemoteUser, 0);
}

/**
 * Executes the command in the message buffer.
 * Usually sent by Con_Send.
 */
void Sv_ExecuteCommand(void)
{
    int flags;
    byte cmdSource;
    unsigned short len;
    dd_bool silent;
    char *cmd = 0;

    LOG_AS("Sv_ExecuteCommand");

    if(!netRemoteUser)
    {
        LOGDEV_NET_ERROR("Command received but no one's logged in!");
        return;
    }

    // The command packet is very simple.
    len = Reader_ReadUInt16(msgReader);
    silent = (len & 0x8000) != 0;
    len &= 0x7fff;
    switch(netBuffer.msg.type)
    {
    /*case PKT_COMMAND:
        cmdSource = CMDS_UNKNOWN; // unknown command source.
        break;*/

    case PKT_COMMAND2:
        // New format includes flags and command source.
        // Flags are currently unused but added for future expansion.
        flags = Reader_ReadUInt16(msgReader);
        DENG_UNUSED(flags);
        cmdSource = Reader_ReadByte(msgReader);
        break;

    default:
        DENG_ASSERT(!"Sv_ExecuteCommand: Not a command packet!");
        return;
    }

    // Make a copy of the command.
    cmd = (char *) M_Malloc(len + 1);
    Reader_Read(msgReader, cmd, len);
    cmd[len] = 0;

    Con_Execute(cmdSource, cmd, silent, true);

    M_Free(cmd);
}

/**
 * Server's packet handler.
 */
void Sv_GetPackets(void)
{
    int         netconsole;
    client_t   *sender;

    while(Net_GetPacket())
    {
        Msg_BeginRead();
        switch(netBuffer.msg.type)
        {
        case PCL_GOODBYE:
            // The client is leaving.
            N_TerminateClient(netBuffer.player);
            break;

        case PKT_COORDS:
            Sv_ClientCoords(netBuffer.player);
            break;

        case PCL_ACK_SHAKE:
            // The client has acknowledged our handshake.
            // Note the time (this isn't perfectly accurate, though).
            netconsole = netBuffer.player;
            if(netconsole >= 0 && netconsole < DDMAXPLAYERS)
            {
                sender = &clients[netconsole];
                sender->shakePing = Timer_RealMilliseconds() - sender->shakePing;
                LOG_NET_MSG("Client %i ping at handshake: %i ms")
                        << netconsole << sender->shakePing;
            }
            break;

        case PCL_ACK_PLAYER_FIX: {
            player_t* plr = &ddPlayers[netBuffer.player];
            ddplayer_t* ddpl = &plr->shared;
            fixcounters_t* acked = &ddpl->fixAcked;

            acked->angles = Reader_ReadInt32(msgReader);
            acked->origin = Reader_ReadInt32(msgReader);
            acked->mom = Reader_ReadInt32(msgReader);

            LOGDEV_NET_XVERBOSE_DEBUGONLY("PCL_ACK_PLAYER_FIX: (%i) Angles %i (%i), pos %i (%i), mom %i (%i)",
                        netBuffer.player << acked->angles << ddpl->fixCounter.angles <<
                        acked->origin << ddpl->fixCounter.origin << acked->mom <<
                        ddpl->fixCounter.mom);
            break; }

        case PKT_PING:
            Net_PingResponse();
            break;

        case PCL_HELLO:
        case PCL_HELLO2:
        case PKT_OK:
        case PKT_CHAT:
        case PKT_PLAYER_INFO:
        case PCL_FINALE_REQUEST:
            Sv_HandlePacket();
            break;

        case PKT_LOGIN:
            Sv_Login();
            break;

        //case PKT_COMMAND:
        case PKT_COMMAND2:
            Sv_ExecuteCommand();
            break;

        default:
            if(netBuffer.msg.type >= PKT_GAME_MARKER)
            {
                // A client has sent a game specific packet.
                gx.HandlePacket(netBuffer.player, netBuffer.msg.type,
                                netBuffer.msg.data, netBuffer.length);
            }
            break;
        }
        Msg_EndRead();
    }
}

/**
 * Assign a new console to the player. Returns true if successful.
 * Called by N_Update().
 */
dd_bool Sv_PlayerArrives(unsigned int nodeID, char const *name)
{
    LOG_AS("Sv_PlayerArrives");
    LOG_NET_NOTE("'%s' has arrived") << name;

    // We need to find the new player a client entry.
    for(int i = 1; i < DDMAXPLAYERS; ++i)
    {
        client_t *cl = &clients[i];

        if(!cl->connected)
        {
            player_t   *plr  = &ddPlayers[i];
            ddplayer_t *ddpl = &plr->shared;

            // This'll do.
            cl->connected = true;
            cl->ready = false;
            cl->nodeID = nodeID;
            cl->viewConsole = i;
            cl->lastTransmit = -1;
            strncpy(cl->name, name, PLAYERNAMELEN);

            ddpl->fixAcked.angles =
                ddpl->fixAcked.origin =
                ddpl->fixAcked.mom = -1;

            // Clear the view filter.
            de::zap(ddpl->filterColor);
            ddpl->flags &= ~DDPF_VIEW_FILTER;

            Sv_InitPoolForClient(i);
            Smoother_Clear(cl->smoother);

            LOG_NET_MSG("'%s' assigned to console %i (node:%u)") << cl->name << i << nodeID;

            // In order to get in the game, the client must first
            // shake hands. It'll request this by sending a Hello packet.
            // We'll be waiting...
            cl->handshake = false;
            return true;
        }
    }

    return false;
}

/**
 * Remove the specified player from the game. Called by N_Update().
 */
void Sv_PlayerLeaves(unsigned int nodeID)
{
    int                 plrNum = N_IdentifyPlayer(nodeID);
    dd_bool             wasInGame;
    player_t           *plr;
    client_t           *cl;

    if(plrNum == -1) return; // Bogus?

    LOG_AS("Sv_PlayerLeaves");

    // Log off automatically.
    if(netRemoteUser == plrNum)
        netRemoteUser = 0;

    cl = &clients[plrNum];
    plr = &ddPlayers[plrNum];

    LOG_NET_NOTE("'%s' (console %i) has left, was connected for %.1f seconds")
            << cl->name << plrNum << (Timer_RealSeconds() - cl->enterTime);

    wasInGame = plr->shared.inGame;
    plr->shared.inGame = false;
    cl->connected = false;
    cl->ready = false;
    //cl->updateCount = 0;
    cl->handshake = false;
    cl->nodeID = 0;
    cl->bandwidthRating = BWR_DEFAULT;

    // Remove the player's data from the register.
    Sv_PlayerRemoved(plrNum);

    if(wasInGame)
    {
        // Inform the DLL about this.
        gx.NetPlayerEvent(plrNum, DDPE_EXIT, NULL);

        // Inform other clients about this.
        Msg_Begin(PSV_PLAYER_EXIT);
        Writer_WriteByte(msgWriter, plrNum);
        Msg_End();
        Net_SendBuffer(NSP_BROADCAST, 0);
    }

    // This client no longer has an ID number.
    cl->id = 0;
}

/**
 * The player will be sent the introductory handshake packets.
 */
void Sv_Handshake(int plrNum, dd_bool newPlayer)
{
    StringArray* ar;
    int i;
    uint playersInGame = 0;

    LOG_AS("Sv_Handshake");
    LOG_NET_VERBOSE("Shaking hands with player %i (newPlayer:%b)") << plrNum << newPlayer;

    for(i = 0; i < DDMAXPLAYERS; ++i)
        if(clients[i].connected)
            playersInGame |= 1 << i;

    Msg_Begin(PSV_HANDSHAKE);
    Writer_WriteByte(msgWriter, SV_VERSION);
    Writer_WriteByte(msgWriter, plrNum);
    Writer_WriteUInt32(msgWriter, playersInGame);
    Writer_WriteFloat(msgWriter, gameTime);
    Msg_End();
    Net_SendBuffer(plrNum, 0);

    // Include the list of material Ids.
    Msg_Begin(PSV_MATERIAL_ARCHIVE);
    MaterialArchive_Write(materialDict, msgWriter);
    Msg_End();
    Net_SendBuffer(plrNum, 0);

    // Include the list of thing Ids.
    ar = Def_ListMobjTypeIDs();
    Msg_Begin(PSV_MOBJ_TYPE_ID_LIST);
    StringArray_Write(ar, msgWriter);
    Msg_End();
    Net_SendBuffer(plrNum, 0);
    StringArray_Delete(ar);

    // Include the list of state Ids.
    ar = Def_ListStateIDs();
    Msg_Begin(PSV_MOBJ_STATE_ID_LIST);
    StringArray_Write(ar, msgWriter);
    Msg_End();
    Net_SendBuffer(plrNum, 0);
    StringArray_Delete(ar);

    if(newPlayer)
    {
        // Note the time when the handshake was sent.
        clients[plrNum].shakePing = Timer_RealMilliseconds();
    }

    // The game DLL wants to shake hands as well?
    gx.NetWorldEvent(DDWE_HANDSHAKE, plrNum, (void *) &newPlayer);

    // Propagate client information.
    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        if(clients[i].connected)
        {
            Net_SendPlayerInfo(i, plrNum);
        }

        // Send the new player's info to other players.
        if(newPlayer && i != 0 && i != plrNum && clients[i].connected)
        {
            Net_SendPlayerInfo(plrNum, i);
        }
    }

    if(!newPlayer)
    {
        // This is not a new player (just a re-handshake) but we'll
        // nevertheless re-init the client's state register. For new
        // players this is done in Sv_PlayerArrives.
        Sv_InitPoolForClient(plrNum);
    }

    ddPlayers[plrNum].shared.flags |= DDPF_FIXANGLES | DDPF_FIXORIGIN | DDPF_FIXMOM;
}

void Sv_StartNetGame(void)
{
    int                 i;

    // Reset all the counters and other data.
    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        client_t           *client = &clients[i];
        player_t           *plr = &ddPlayers[i];
        ddplayer_t         *ddpl = &plr->shared;

        ddpl->inGame = false;
        ddpl->flags &= ~DDPF_CAMERA;

        client->connected = false;
        client->ready = false;
        client->nodeID = 0;
        client->enterTime = 0;
        client->lastTransmit = -1;
        client->fov = 90;
        client->viewConsole = -1;
        de::zap(client->name);
        client->bandwidthRating = BWR_DEFAULT;
        Smoother_Clear(client->smoother);
    }
    gameTime = 0;
    firstNetUpdate = true;
    netRemoteUser = 0;

    // The server is always player number zero.
    consolePlayer = displayPlayer = 0;

    netGame = true;
    isServer = true;
    allowSending = true;

    // Prepare the material dictionary we'll be using with clients.
    materialDict = MaterialArchive_New(false);

    LOGDEV_NET_XVERBOSE("Prepared material dictionary with %i materials")
            << MaterialArchive_Count(materialDict);

    if(!isDedicated)
    {
        player_t           *plr = &ddPlayers[consolePlayer];
        ddplayer_t         *ddpl = &plr->shared;
        client_t           *cl = &clients[consolePlayer];

        ddpl->inGame = true;
        cl->connected = true;
        cl->ready = true;
        cl->viewConsole = 0;
        strcpy(cl->name, playerName);
    }
}

void Sv_StopNetGame(void)
{
    if(materialDict)
    {
        MaterialArchive_Delete(materialDict);
        materialDict = 0;
    }
}

unsigned int Sv_IdForMaterial(Material* mat)
{
    assert(materialDict);
    return MaterialArchive_FindUniqueSerialId(materialDict, mat);
}

void Sv_SendText(int to, int con_flags, const char* text)
{
    uint32_t len = MIN_OF(0xffff, strlen(text));

    Msg_Begin(PSV_CONSOLE_TEXT);
    Writer_WriteUInt32(msgWriter, con_flags & ~CPF_TRANSMIT);
    Writer_WriteUInt16(msgWriter, len);
    Writer_Write(msgWriter, text, len);
    Msg_End();
    Net_SendBuffer(to, 0);
}

/**
 * Asks a client to disconnect. Clients will immediately disconnect
 * after receiving the PSV_SERVER_CLOSE message.
 */
void Sv_Kick(int who)
{
    if(!clients[who].connected)
        return;

    Sv_SendText(who, SV_CONSOLE_PRINT_FLAGS, "You were kicked out!\n");
    Msg_Begin(PSV_SERVER_CLOSE);
    Msg_End();
    Net_SendBuffer(who, 0);
}

/**
 * Sends player @a plrNum's position, momentum and/or angles override to all
 * clients.
 */
void Sv_SendPlayerFixes(int plrNum)
{
    int                 fixes = 0;
    player_t           *plr = &ddPlayers[plrNum];
    ddplayer_t         *ddpl = &plr->shared;

    if(!(ddpl->flags & (DDPF_FIXANGLES | DDPF_FIXORIGIN | DDPF_FIXMOM)))
    {
        // Nothing to fix.
        return;
    }

    LOG_AS("Sv_SendPlayerFixes");

    // Start writing a player fix message.
    Msg_Begin(PSV_PLAYER_FIX);

    // Which player is being fixed?
    Writer_WriteByte(msgWriter, plrNum);

    // Indicate what is included in the message.
    if(ddpl->flags & DDPF_FIXANGLES) fixes |= 1;
    if(ddpl->flags & DDPF_FIXORIGIN) fixes |= 2;
    if(ddpl->flags & DDPF_FIXMOM) fixes |= 4;

    Writer_WriteUInt32(msgWriter, fixes);
    Writer_WriteUInt16(msgWriter, ddpl->mo->thinker.id);

    LOGDEV_NET_MSG("Fixing mobj %i of player %i") << ddpl->mo->thinker.id << plrNum;

    // Increment counters.
    if(ddpl->flags & DDPF_FIXANGLES)
    {
        Writer_WriteInt32(msgWriter, ++ddpl->fixCounter.angles);
        Writer_WriteUInt32(msgWriter, ddpl->mo->angle);
        Writer_WriteFloat(msgWriter, ddpl->lookDir);

        LOGDEV_NET_MSG("Sent angles (%i): angle=%x lookdir=%.2f")
                << ddpl->fixCounter.angles << ddpl->mo->angle << ddpl->lookDir;
    }

    if(ddpl->flags & DDPF_FIXORIGIN)
    {
        Writer_WriteInt32(msgWriter, ++ddpl->fixCounter.origin);
        Writer_WriteFloat(msgWriter, ddpl->mo->origin[VX]);
        Writer_WriteFloat(msgWriter, ddpl->mo->origin[VY]);
        Writer_WriteFloat(msgWriter, ddpl->mo->origin[VZ]);

        LOGDEV_NET_MSG("Sent position (%i): %s")
                << ddpl->fixCounter.origin << Vector3d(ddpl->mo->origin).asText();
    }

    if(ddpl->flags & DDPF_FIXMOM)
    {
        Writer_WriteInt32(msgWriter, ++ddpl->fixCounter.mom);
        Writer_WriteFloat(msgWriter, ddpl->mo->mom[MX]);
        Writer_WriteFloat(msgWriter, ddpl->mo->mom[MY]);
        Writer_WriteFloat(msgWriter, ddpl->mo->mom[MZ]);

        LOGDEV_NET_MSG("Sent momentum (%i): %s")
                << ddpl->fixCounter.mom << Vector3d(ddpl->mo->mom).asText();
    }

    Msg_End();

    // Send the fix message to everyone.
    Net_SendBuffer(DDSP_ALL_PLAYERS, 0);

    ddpl->flags &= ~(DDPF_FIXANGLES | DDPF_FIXORIGIN | DDPF_FIXMOM);

    LOGDEV_NET_VERBOSE("Cleared FIX flags of player %i") << plrNum;

    // Clear the smoother for this client.
    if(clients[plrNum].smoother)
    {
        Smoother_Clear(clients[plrNum].smoother);
    }
}

void Sv_Ticker(timespan_t ticLength)
{
    int i;

    if(!isDedicated) return;

    // Note last angles for all players.
    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        player_t *plr = &ddPlayers[i];

        if(!plr->shared.inGame || !plr->shared.mo)
            continue;

        // Update the smoother?
        if(clients[i].smoother)
        {
            Smoother_Advance(clients[i].smoother, ticLength);
        }

        if(DD_IsSharpTick())
        {
            plr->shared.lastAngle = plr->shared.mo->angle;
        }

        /*
        if(clients[i].bwrAdjustTime > 0)
        {
            // BWR adjust time tics away.
            clients[i].bwrAdjustTime--;
        }*/

        // Increment counter, send new data.
        Sv_SendPlayerFixes(i);
    }
}

/**
 * @return              The number of players in the game.
 */
int Sv_GetNumPlayers(void)
{
    int                 i, count;

    // Clients can't count.
    if(isClient)
        return 1;

    for(i = count = 0; i < DDMAXPLAYERS; ++i)
    {
        player_t           *plr = &ddPlayers[i];

        if(plr->shared.inGame && plr->shared.mo)
            count++;
    }

    return count;
}

/**
 * @return              The number of connected clients.
 */
int Sv_GetNumConnected(void)
{
    int                 i, count = 0;

    // Clients can't count.
    if(isClient)
        return 1;

    for(i = isDedicated ? 1 : 0; i < DDMAXPLAYERS; ++i)
        if(clients[i].connected)
            count++;

    return count;
}

/**
 * The bandwidth rating is updated according to the status of the
 * player's send queue. Returns true if a new packet may be sent.
 *
 * @todo This functionality needs to be restored: servers can't simply output
 * an arbitrary amount of data to clients with no regard to the available
 * bandwidth.
 */
dd_bool Sv_CheckBandwidth(int /*playerNumber*/)
{
    return true;
    /*
    client_t           *client = &clients[playerNumber];
    uint                qSize = N_GetSendQueueSize(playerNumber);
    uint                limit = 400;

    return true;

    // If there are too many messages in the queue, the player's bandwidth
    // is overrated.
    if(qSize > limit)
    {
        // Drop quickly to allow the send queue to clear out sooner.
        client->bandwidthRating -= 10;
    }

    // If the send queue is practically empty, we can use more bandwidth.
    // (Providing we have BWR adjust time.)
    if(qSize < limit / 20 && client->bwrAdjustTime > 0)
    {
        client->bandwidthRating++;

        // Increase BWR only once during the adjust time.
        client->bwrAdjustTime = 0;
    }

    // Do not go past the boundaries, though.
    if(client->bandwidthRating < 0)
    {
        client->bandwidthRating = 0;
    }
    if(client->bandwidthRating > MAX_BANDWIDTH_RATING)
    {
        client->bandwidthRating = MAX_BANDWIDTH_RATING;
    }

    // New messages will not be sent if there's too much already.
    return qSize <= 10 * limit;
    */
}

/**
 * Reads a PKT_COORDS packet from the message buffer. We trust the
 * client's position and change ours to match it. The client better not
 * be cheating.
 */
void Sv_ClientCoords(int plrNum)
{
    player_t           *plr = &ddPlayers[plrNum];
    ddplayer_t         *ddpl = &plr->shared;
    mobj_t             *mo = ddpl->mo;
    int                 clz;
    float               clientGameTime;
    float               clientPos[3];
    angle_t             clientAngle;
    float               clientLookDir;
    dd_bool             onFloor = false;

    // If mobj or player is invalid, the message is discarded.
    if(!mo || !ddpl->inGame || (ddpl->flags & DDPF_DEAD))
        return;

    clientGameTime = Reader_ReadFloat(msgReader);

    clientPos[VX] = Reader_ReadFloat(msgReader);
    clientPos[VY] = Reader_ReadFloat(msgReader);

    clz = Reader_ReadInt32(msgReader);
    if(clz == DDMININT)
    {
        clientPos[VZ] = mo->floorZ;
        onFloor = true;
    }
    else
    {
        clientPos[VZ] = FIX2FLT(clz);
    }

    // The angles.
    clientAngle = ((angle_t) Reader_ReadUInt16(msgReader)) << 16;
    clientLookDir = P_ShortToLookDir(Reader_ReadInt16(msgReader));

    // Movement intent.
    ddpl->forwardMove = FIX2FLT(Reader_ReadChar(msgReader) << 13);
    ddpl->sideMove = FIX2FLT(Reader_ReadChar(msgReader) << 13);

    if(ddpl->fixCounter.angles == ddpl->fixAcked.angles && !(ddpl->flags & DDPF_FIXANGLES))
    {
        LOGDEV_NET_XVERBOSE_DEBUGONLY("Sv_ClientCoords: Setting angles for player %i: %x, %f",
                                     plrNum << clientAngle << clientLookDir);

        mo->angle = clientAngle;
        ddpl->lookDir = clientLookDir;
    }

    LOGDEV_NET_XVERBOSE_DEBUGONLY("Sv_ClientCoords: Received coords for player %i: %f, %f, %f",
                                 plrNum << clientPos[VX] << clientPos[VY] << clientPos[VZ]);

    // If we aren't about to forcibly change the client's position, update
    // with new pos if it's valid. But it must be a valid pos.
    if(Sv_CanTrustClientPos(plrNum))
    {
        LOGDEV_NET_XVERBOSE_DEBUGONLY("Sv_ClientCoords: Setting coords for player %i: %f, %f, %f",
                                     plrNum << clientPos[VX] << clientPos[VY] << clientPos[VZ]);

        Smoother_AddPos(clients[plrNum].smoother, clientGameTime,
                        clientPos[VX], clientPos[VY], clientPos[VZ], onFloor);
    }
}

dd_bool Sv_CanTrustClientPos(int plrNum)
{
    player_t* plr = &ddPlayers[plrNum];
    ddplayer_t* ddpl = &plr->shared;

    if(ddpl->fixCounter.origin == ddpl->fixAcked.origin && !(ddpl->flags & DDPF_FIXORIGIN))
    {
        return true;
    }
    // Server's position is valid, client is not up-to-date.
    return false;
}

/**
 * Console command for terminating a remote console connection.
 */
D_CMD(Logout)
{
    DENG2_UNUSED3(src, argc, argv);

    // Only servers can execute this command.
    if(!netRemoteUser || !isServer)
        return false;
    // Notice that the server WILL execute this command when a client
    // is logged in and types "logout".
    Sv_SendText(netRemoteUser, SV_CONSOLE_PRINT_FLAGS, "Goodbye...\n");
    // Send a logout packet.
    Msg_Begin(PKT_LOGIN);
    Writer_WriteByte(msgWriter, false);       // You're outta here.
    Msg_End();
    Net_SendBuffer(netRemoteUser, 0);
    netRemoteUser = 0;
    return true;
}

DENG_DECLARE_API(Server) =
{
    { DE_API_SERVER },
    Sv_CanTrustClientPos
};
