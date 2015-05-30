/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Packet"
#include "de/Writer"
#include "de/Reader"
#include "de/String"

#include <QByteArray>

using namespace de;

Packet::Packet(Type const &t)
{
    setType(t);
}

void Packet::setType(Type const &t)
{
    DENG2_ASSERT(t.size() == TYPE_SIZE);
    _type = t;
}

void Packet::operator >> (Writer &to) const
{
    QByteArray bytes = _type.toLatin1();
    to << bytes[0] << bytes[1] << bytes[2] << bytes[3];
}

void Packet::operator << (Reader &from)
{
    char ident[5];
    from >> ident[0] >> ident[1] >> ident[2] >> ident[3];
    ident[4] = 0;
    
    // Having been constructed as a specific type, the identifier is already
    // set and cannot change. Let's check if it's the correct one.
    if(_type.compareWithCase(ident))
    {
        throw InvalidTypeError("Packet::operator <<", "Invalid ID");
    }
}

void Packet::execute() const
{}

bool Packet::checkType(Reader &from, String const &type)
{
    char ident[5];
    from.mark();
    from >> ident[0] >> ident[1] >> ident[2] >> ident[3];
    ident[4] = 0;
    from.rewind();
    return !type.compareWithCase(ident);
}
