/** @file serverfinder.cpp  Looks up servers via beacon.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include "de/shell/ServerFinder"
#include <de/Beacon>
#include <de/Reader>
#include <de/TextValue>
#include <de/NumberValue>
#include <de/App>
#include <QMap>
#include <QTimer>

namespace de {
namespace shell {

static TimeDelta MSG_EXPIRATION_SECS = 4;

DENG2_PIMPL_NOREF(ServerFinder)
{
    Beacon beacon;
    struct Found
    {
        Record *message;
        Time at;

        Found() : message(0), at(Time()) {}
    };
    QMap<Address, Found> servers;

    Instance() : beacon(DEFAULT_PORT) {}

    ~Instance()
    {
        clearServers();
    }

    void clearServers()
    {
        foreach(Found const &found, servers.values())
        {
            delete found.message;
        }
        servers.clear();
    }

    bool removeExpired()
    {
        bool changed = false;

        QMutableMapIterator<Address, Found> iter(servers);
        while(iter.hasNext())
        {
            Found &found = iter.next().value();
            if(found.at.since() > MSG_EXPIRATION_SECS)
            {
                delete found.message;
                iter.remove();
                changed = true;
            }
        }

        return changed;
    }
};

ServerFinder::ServerFinder() : d(new Instance)
{
    try
    {
        connect(&d->beacon, SIGNAL(found(de::Address, de::Block)), this, SLOT(found(de::Address, de::Block)));
        QTimer::singleShot(1000, this, SLOT(expire()));

        if(!App::appExists() || !App::commandLine().has("-nodiscovery"))
        {
            d->beacon.discover(0 /* no timeout */, 2);
        }
    }
    catch(Beacon::PortError const &er)
    {
        LOG_WARNING("Automatic server discovery is not available:\n") << er.asText();
    }
}

ServerFinder::~ServerFinder()
{
    d.reset();
}

void ServerFinder::clear()
{
    d->clearServers();
}

QList<Address> ServerFinder::foundServers() const
{
    return d->servers.keys();
}

String ServerFinder::name(Address const &server) const
{
    return messageFromServer(server).gets("name");
}

int ServerFinder::playerCount(Address const &server) const
{
    return messageFromServer(server).geti("nump");
}

int ServerFinder::maxPlayers(Address const &server) const
{
    return messageFromServer(server).geti("maxp");
}

Record const &ServerFinder::messageFromServer(Address const &address) const
{
    if(!d->servers.contains(address))
    {
        /// @throws NotFoundError @a address not found in the registry of server responses.
        throw NotFoundError("ServerFinder::messageFromServer",
                            "No message from server " + address.asText());
    }
    return *d->servers[address].message;
}

void ServerFinder::found(Address host, Block block)
{
    // Normalize the local host address.
    if(host.isLocal()) host.setHost(QHostAddress::LocalHost);

    try
    {
        LOG_TRACE("Received a server message from %s with %i bytes")
                << host << block.size();

        // Replace or insert the information for this host.
        Instance::Found found;
        if(d->servers.contains(host))
        {
            found.message = d->servers[host].message;
            d->servers[host].at = Time();
        }
        else
        {
            found.message = new Record;
            d->servers.insert(host, found);
        }
        Reader(block).withHeader() >> *found.message;

        emit updated();
    }
    catch(Error const &)
    {
        // Remove the message that failed to deserialize.
        if(d->servers.contains(host))
        {
            delete d->servers[host].message;
            d->servers.remove(host);
        }
    }
}

void ServerFinder::expire()
{
    if(d->removeExpired())
    {
        emit updated();
    }
    QTimer::singleShot(1000, this, SLOT(expire()));
}

} // namespace shell
} // namespace de
