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

#include "de/NoneValue"
#include "de/Writer"
#include "de/Reader"

using namespace de;

NoneValue::NoneValue()
{}

Value *NoneValue::duplicate() const
{
    return new NoneValue();
}

Value::Text NoneValue::asText() const
{
    return "(none)";
}

bool NoneValue::isTrue() const
{
    return false;
}

dint NoneValue::compare(Value const &value) const
{
    NoneValue const *other = dynamic_cast<NoneValue const *>(&value);
    if(other)
    {
        // All nones are equal.
        return 0;
    }    
    // None is less than everything else.
    return 1;
}

void NoneValue::operator >> (Writer &to) const
{
    to << SerialId(NONE);
}

void NoneValue::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if(id != NONE)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized value was invalid.
        throw DeserializationError("NoneValue::operator <<", "Invalid ID");
    }
}
