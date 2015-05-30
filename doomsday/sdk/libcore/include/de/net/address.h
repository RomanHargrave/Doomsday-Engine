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

#ifndef LIBDENG2_ADDRESS_H
#define LIBDENG2_ADDRESS_H

#include "../libcore.h"
#include "../Log"

#include <QHostAddress>
#include <QTextStream>

namespace de {

class String;
    
/**
 * IP address.
 *
 * @ingroup net
 */
class DENG2_PUBLIC Address : public LogEntry::Arg::Base
{
public:
    Address();

    /**
     * Constructs an Address.
     *
     * @param address  IP address. E.g., "localhost" or "127.0.0.1".
     *                 Domain names are not allowed.
     * @param port     Port number.
     */
    Address(QHostAddress const &address, duint16 port = 0);

    Address(char const *address, duint16 port = 0);

    Address(Address const &other);

    Address &operator = (Address const &other);

    bool operator < (Address const &other) const;

    /**
     * Checks two addresses for equality.
     *
     * @param other  Address.
     *
     * @return @c true if the addresses are equal.
     */
    bool operator == (Address const &other) const;

    bool isNull() const;

    /**
     * Returns the host IP address.
     */
    QHostAddress const &host() const;

    void setHost(QHostAddress const &host);

    /**
     * Determines if the address is on the local host.
     */
    bool isLocal() const;

    duint16 port() const;

    void setPort(duint16 p);

    /**
     * Checks if two IP address match. Port numbers are ignored.
     *
     * @param other  Address to check against.
     * @param mask   Net mask. Use to check if subnets match. The default
     *               checks if two IP addresses match.
     */
    bool matches(Address const &other, duint32 mask = 0xffffffff);

    /**
     * Converts the address to text.
     */
    String asText() const;

    static Address parse(String const &addressWithOptionalPort, duint16 defaultPort = 0);

    /**
     * Determines whether a host address refers to the local host.
     */
    static bool isHostLocal(QHostAddress const &host);

    // Implements LogEntry::Arg::Base.
    LogEntry::Arg::Type logEntryArgType() const { return LogEntry::Arg::StringArgument; }

private:
    DENG2_PRIVATE(d)
};

DENG2_PUBLIC QTextStream &operator << (QTextStream &os, Address const &address);

} // namespace de

#endif // LIBDENG2_ADDRESS_H
