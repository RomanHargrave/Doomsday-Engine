/** @file serversystem.cpp  Subsystem for tending to clients.
 *
 * @authors Copyright © 2013-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#include "serversystem.h"

#include <de/c_wrapper.h>
#include <de/timer.h>
#include <de/Address>
#include <de/Beacon>
#include <de/ByteRefArray>
#include <de/Garbage>
#include <de/ListenSocket>
#include <de/TextApp>

#include "api_console.h"

#include "serverapp.h"
#include "shellusers.h"
#include "remoteuser.h"

#include "server/sv_def.h"
#include "server/sv_frame.h"

#include "network/net_main.h"
#include "network/net_buf.h"
#include "network/net_event.h"
#include "network/monitor.h"
#include "network/masterserver.h"

#include "dd_main.h"
#include "dd_loop.h"
#include "sys_system.h"
#include "world/map.h"
#include "world/p_players.h"

using namespace de;

int nptIPPort = 0; ///< Server TCP port (cvar).

static de::duint16 Server_ListenPort()
{
    return (!nptIPPort ? DEFAULT_TCP_PORT : nptIPPort);
}

DENG2_PIMPL(ServerSystem)
{
    bool inited = false;

    /// Beacon for informing clients that a server is present.
    Beacon beacon = { DEFAULT_UDP_PORT };
    Time lastBeaconUpdateAt;

    ListenSocket *serverSock = nullptr;

    QMap<Id, RemoteUser *> users;
    ShellUsers shellUsers;

    Instance(Public *i) : Base(i) {}
    ~Instance() { deinit(); }

    bool isStarted() const
    {
        return serverSock != nullptr;
    }

    bool init(duint16 port)
    {
        // Note: re-initialization is allowed, so we don't check for inited now.

        LOG_NET_NOTE("Server listening on TCP port %i") << port;

        deinit();

        // Open a listening TCP socket. It will accept client connections.
        DENG2_ASSERT(!serverSock);
        if(!(serverSock = new ListenSocket(port)))
            return false;

        QObject::connect(serverSock, SIGNAL(incomingConnection()), thisPublic, SLOT(handleIncomingConnection()));

        // Update the beacon with the new port.
        beacon.start(port);

        App_WorldSystem().audienceForMapChange() += shellUsers;

        inited = true;
        return true;
    }

    void clearUsers()
    {
        // Clear the client nodes.
        for(RemoteUser *u : users.values())
        {
            delete u;
        }
        DENG2_ASSERT(users.isEmpty());
    }

    void deinit()
    {
        if(!inited) return;
        inited = false;

        if(ServerApp::appExists())
        {
            App_WorldSystem().audienceForMapChange() -= shellUsers;
        }

        beacon.stop();

        // Close the listening socket.
        delete serverSock;
        serverSock = 0;

        clearUsers();
    }

    RemoteUser &findUser(Id const &id) const
    {
        DENG2_ASSERT(users.contains(id));
        return *users[id];
    }

    void updateBeacon(Clock const &clock)
    {
        if(lastBeaconUpdateAt.since() > 0.5)
        {
            lastBeaconUpdateAt = clock.time();

            // Update the status message in the server's presence beacon.
            if(serverSock && App_WorldSystem().hasMap())
            {
                serverinfo_t info;
                Sv_GetInfo(&info);

                std::unique_ptr<Record> rec(Sv_InfoToRecord(&info));
                Block msg;
                de::Writer(msg).withHeader() << *rec;
                beacon.setMessage(msg);
            }
        }
    }

    /**
     * The client is removed from the game immediately. This is used when
     * the server needs to terminate a client's connection abnormally.
     */
    void terminateNode(Id const &id)
    {
        if(id)
        {
            DENG2_ASSERT(users.contains(id));

            delete users[id];

            DENG2_ASSERT(!users.contains(id));
        }
    }

    void printStatus()
    {
        if(serverSock)
        {
            LOG_NOTE("SERVER: Listening on TCP port %i") << serverSock->port();
        }
        else
        {
            LOG_NOTE("SERVER: No server socket open");
        }

        int first = true;
        for(int i = 1; i < DDMAXPLAYERS; ++i)
        {
            client_t *cl = &clients[i];
            player_t *plr = &ddPlayers[i];
            if(cl->nodeID)
            {
                DENG2_ASSERT(users.contains(cl->nodeID));

                RemoteUser *user = users[cl->nodeID];
                if(first)
                {
                    LOG_MSG(_E(m) "P# Name:      Nd Jo Hs Rd Gm Age:");
                    first = false;
                }

                LOG_MSG(_E(m) "%2i %-10s %2i %c  %c  %c  %c  %f sec")
                        << i << cl->name << cl->nodeID
                        << (user->isJoined()? '*' : ' ')
                        << (cl->handshake? '*' : ' ')
                        << (cl->ready? '*' : ' ')
                        << (plr->shared.inGame? '*' : ' ')
                        << (Timer_RealSeconds() - cl->enterTime);
            }
        }
        if(first)
        {
            LOG_MSG("No clients connected");
        }

        if(shellUsers.count())
        {
            LOG_MSG("%i shell user%s")
                    << shellUsers.count()
                    << (shellUsers.count() == 1? "" : "s");
        }

        N_PrintBufferInfo();

        LOG_MSG(_E(b) "Configuration:");
        LOG_MSG("  Port for hosting games (net-ip-port): %i") << Con_GetInteger("net-ip-port");
        LOG_MSG("  Shell password (server-password): \"%s\"") << netPassword;
    }
};

ServerSystem::ServerSystem() : d(new Instance(this))
{}

void ServerSystem::start(duint16 port)
{
    d->init(port);
}

void ServerSystem::stop()
{
    d->deinit();
}

bool ServerSystem::isListening() const
{
    return d->isStarted();
}

void ServerSystem::terminateNode(Id const &id)
{
    d->terminateNode(id);
}

RemoteUser &ServerSystem::user(Id const &id) const
{
    if(!d->users.contains(id))
    {
        throw IdError("ServerSystem::user", "User " + id.asText() + " does not exist");
    }
    return *d->users[id];
}

bool ServerSystem::isUserAllowedToJoin(RemoteUser &/*user*/) const
{
    // If the server is full, attempts to connect are canceled.
    return (Sv_GetNumConnected() < svMaxPlayers);
}

void ServerSystem::convertToShellUser(RemoteUser *user)
{
    DENG2_ASSERT(user);
    LOG_AS("convertToShellUser");

    Socket *socket = user->takeSocket();

    LOGDEV_NET_VERBOSE("Remote user %s converted to shell user") << user->id();
    user->deleteLater();

    d->shellUsers.add(new ShellUser(socket));
}

void ServerSystem::timeChanged(Clock const &clock)
{
    if(Sys_IsShuttingDown())
        return; // Shouldn't run this while shutting down.

    Garbage_Recycle();

    // Adjust loop rate depending on whether players are in game.
    int count = 0;
    for(int i = 1; i < DDMAXPLAYERS; ++i)
    {
        if(ddPlayers[i].shared.inGame) count++;
    }

    DENG2_TEXT_APP->loop().setRate(count? 35 : 3);

    Loop_RunTics();

    // Update clients at regular intervals.
    Sv_TransmitFrame();

    d->updateBeacon(clock);

    /// @todo There's no need to queue packets via net_buf, just handle
    /// them right away.
    Sv_GetPackets();

    /// @todo Kick unjoined nodes who are silent for too long.
}

void ServerSystem::handleIncomingConnection()
{
    LOG_AS("ServerSystem");
    forever
    {
        Socket *sock = d->serverSock->accept();
        if(!sock) break;

        RemoteUser *user = new RemoteUser(sock);
        connect(user, SIGNAL(userDestroyed()), this, SLOT(userDestroyed()));
        d->users.insert(user->id(), user);

        // Immediately handle pending messages, if there are any.
        user->handleIncomingPackets();
    }
}

void ServerSystem::userDestroyed()
{
    RemoteUser *u = static_cast<RemoteUser *>(sender());

    LOG_AS("ServerSystem");
    LOGDEV_NET_VERBOSE("Removing user %s") << u->id();

    d->users.remove(u->id());

    LOG_NET_VERBOSE("%i remote users and %i shell users remain")
            << d->users.size() << d->shellUsers.count();
}

void ServerSystem::printStatus()
{
    d->printStatus();
}

ServerSystem &App_ServerSystem()
{
    return ServerApp::serverSystem();
}

//---------------------------------------------------------------------------

void Server_Register()
{
    C_VAR_INT("net-ip-port", &nptIPPort, CVF_NO_MAX, 0, 0);

#ifdef _DEBUG
    C_CMD("netfreq", NULL, NetFreqs);
#endif
}

dd_bool N_ServerOpen()
{
    App_ServerSystem().start(Server_ListenPort());

    // The game module may have something that needs doing before we actually begin.
    if(gx.NetServerStart)
    {
        gx.NetServerStart(true);
    }

    Sv_StartNetGame();

    // The game DLL might want to do something now that the server is started.
    if(gx.NetServerStart)
    {
        gx.NetServerStart(false);
    }

    if(masterAware)
    {
        // Let the master server know that we are running a public server.
        N_MasterAnnounceServer(true);
    }

    return true;
}

dd_bool N_ServerClose()
{
    if(!App_ServerSystem().isListening()) return true;

    if(masterAware)
    {
        // Bye-bye, master server.
        N_MAClear();
        N_MasterAnnounceServer(false);
    }

    if(gx.NetServerStop)
    {
        gx.NetServerStop(true);
    }

    Net_StopGame();
    Sv_StopNetGame();

    if(gx.NetServerStop)
    {
        gx.NetServerStop(false);
    }

    App_ServerSystem().stop();
    return true;
}

void N_PrintNetworkStatus()
{
    App_ServerSystem().printStatus();
}
