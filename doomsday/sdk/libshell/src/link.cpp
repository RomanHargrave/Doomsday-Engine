/** @file link.h  Network connection to a server.
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

#include "de/shell/Link"
#include "de/shell/Protocol"
#include <de/Message>
#include <de/Socket>
#include <de/Time>
#include <de/Log>
#include <de/ByteRefArray>

namespace de {
namespace shell {

DENG2_PIMPL(Link)
{
    Protocol protocol;

    Instance(Public *i) : Base(i)
    {}
};

Link::Link(String const &domain, TimeDelta const &timeout) : d(new Instance(this))
{
    connectDomain(domain, timeout);
}

Link::Link(Address const &address) : d(new Instance(this))
{
    connectHost(address);
}

Link::Link(Socket *openSocket) : d(new Instance(this))
{
    takeOver(openSocket);
}

Protocol &Link::protocol()
{
    return d->protocol;
}

Packet *Link::interpret(Message const &msg)
{
    return d->protocol.interpret(msg);
}

void Link::initiateCommunications()
{
    // Tell the server to switch to shell mode (v1).
    *this << ByteRefArray("Shell", 5);
}

} // namespace shell
} // namespace de
