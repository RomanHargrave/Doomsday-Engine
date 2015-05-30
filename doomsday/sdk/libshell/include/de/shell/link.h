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

#ifndef LIBSHELL_LINK_H
#define LIBSHELL_LINK_H

#include "libshell.h"
#include <de/Address>
#include <de/Socket>
#include <de/Time>
#include <de/Transmitter>
#include <de/shell/AbstractLink>
#include <de/shell/Protocol>
#include <QObject>

namespace de {
namespace shell {

/**
 * Network connection to a server using the shell protocol.
 */
class LIBSHELL_PUBLIC Link : public AbstractLink
{
    Q_OBJECT

public:
    /**
     * Opens a connection to a server over the network.
     *
     * @param domain   Domain/IP address of the server.
     * @param timeout  Keep trying until this much time has passed.
     */
    Link(String const &domain, TimeDelta const &timeout = 0);

    /**
     * Opens a connection to a server over the network.
     *
     * @param address  Address of the server.
     */
    Link(Address const &address);

    /**
     * Takes over an existing socket.
     *
     * @param openSocket  Socket. Link takes ownership.
     */
    Link(Socket *openSocket);

    /**
     * Shell protocol for constructing and interpreting packets.
     */
    Protocol &protocol();

protected:
    Packet *interpret(Message const &msg);
    void initiateCommunications();

private:
    DENG2_PRIVATE(d)
};

} // namespace shell
} // namespace de

#endif // LIBSHELL_LINK_H
