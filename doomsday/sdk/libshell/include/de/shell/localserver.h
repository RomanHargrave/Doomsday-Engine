/** @file localserver.h  Starting and stopping local servers.
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

#ifndef LIBSHELL_LOCALSERVER_H
#define LIBSHELL_LOCALSERVER_H

#include "Link"
#include <de/Error>
#include <de/NativePath>
#include <QStringList>

namespace de {
namespace shell {

/**
 * Utility for starting and stopping local servers.
 */
class LIBSHELL_PUBLIC LocalServer
{
public:
    /// Failed to locate the server executable. @ingroup errors
    DENG2_ERROR(NotFoundError);

public:
    LocalServer();

    /**
     * Sets the name of the server.
     *
     * @param name  Name.
     */
    void setName(String const &name);

    void start(duint16 port,
               String const &gameMode,
               QStringList additionalOptions = QStringList(),
               NativePath const &runtimePath = "");

    void stop();

    /**
     * Opens a link for communicating with the server. The returned link will
     * initially be in the Link::Connecting state.
     *
     * @return Link to the local server. Caller gets ownership.
     */
    Link *openLink();

    /**
     * Reads the path of the error log. This is useful after a failed server start.
     *
     * @return Native path of the error log.
     */
    NativePath errorLogPath() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace shell
} // namespace de

#endif // LIBSHELL_LOCALSERVER_H
