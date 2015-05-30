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

#include "de/Id"
#include "de/String"
#include "de/Writer"
#include "de/Reader"

namespace de {

// The Id generator starts from one.
Id::Type Id::_generator = 1;

Id::Id() : _id(_generator++)
{
    if(_id == None) 
    {
        ++_id;
    }
}

Id::Id(String const &text) : _id(None)
{
    if(text.beginsWith("{") && text.endsWith("}"))
    {
        _id = text.substr(1, text.size() - 2).toUInt();
    }
}

Id::~Id()
{}

Id::operator String () const
{
    return asText();
}
    
Id::operator Value::Number () const
{
    return static_cast<Value::Number>(_id);
}
    
String Id::asText() const
{
    return QString("{%1}").arg(_id);
}

ddouble Id::asDouble() const
{
    return _id;
}

dint64 Id::asInt64() const
{
    return _id;
}

QTextStream &operator << (QTextStream &os, Id const &id)
{
    os << id.asText();
    return os;
}

void Id::operator >> (Writer &to) const
{
    to << _id;
}

void Id::operator << (Reader &from)
{
    from >> _id;
}

} // namespace de
