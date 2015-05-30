/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_LISTENSOCKET_H
#define LIBDENG2_LISTENSOCKET_H

#include "../libcore.h"

#include <QObject>
#include <QList>
#include <QThread>
#include <QDebug>

namespace de {

class Socket;

/**
 * TCP/IP server socket.  It can only be used for accepting incoming TCP/IP
 * connections.  Normal communications using a listen socket are not possible.
 *
 * @ingroup net
 */
class DENG2_PUBLIC ListenSocket : public QObject
{
    Q_OBJECT

public:
    /// Opening the socket failed. @ingroup errors
    DENG2_ERROR(OpenError);

public:
    /// Open a listen socket on the specified port.
    ListenSocket(duint16 port);

    /// Returns the port the socket is listening on.
    duint16 port() const;

    /// Returns an incoming connection. Caller takes ownership of
    /// the returned object.
    Socket *accept();

signals:
    /**
     * Notifies when a new incoming connection is available.
     * Call accept() to get the Socket object.
     */
    void incomingConnection();

protected slots:
    void acceptNewConnection();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_LISTENSOCKET_H
