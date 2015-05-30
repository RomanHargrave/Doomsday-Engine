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

#include "de/NumberValue"
#include "de/Writer"
#include "de/Reader"
#include "de/math.h"

#include <QTextStream>

namespace de {

NumberValue::NumberValue(Number initialValue, SemanticHints semantic)
    : _value(initialValue), _semantic(semantic)
{}

NumberValue::NumberValue(dsize initialSize)
    : _value(initialSize), _semantic(Generic)
{}

NumberValue::NumberValue(dint initialInteger, SemanticHints semantic)
    : _value(initialInteger), _semantic(semantic)
{}

NumberValue::NumberValue(duint initialUnsignedInteger, SemanticHints semantic)
    : _value(initialUnsignedInteger), _semantic(semantic)
{}

NumberValue::NumberValue(bool initialBoolean)
    : _value(initialBoolean? True : False), _semantic(Boolean)
{}

Value *NumberValue::duplicate() const
{
    return new NumberValue(_value, _semantic);
}

Value::Number NumberValue::asNumber() const
{
    return _value;
}

Value::Text NumberValue::asText() const
{
    Text result;
    QTextStream s(&result);
    if(_semantic.testFlag(Boolean) && (_value == True || _value == False))
    {
        s << (isTrue()? "True" : "False");
    }
    else if(_semantic.testFlag(Hex))
    {
        s << "0x" << QString::number(int(_value), 16);
    }
    else
    {
        s << _value;
    }
    return result;
}

bool NumberValue::isTrue() const
{
    return (_value != 0);
}

dint NumberValue::compare(Value const &value) const
{
    NumberValue const *other = dynamic_cast<NumberValue const *>(&value);
    if(other)
    {
        if(fequal(_value, other->_value))
        {
            return 0;
        }
        return cmp(_value, other->_value);
    }    
    return Value::compare(value);
}

void NumberValue::negate()
{
    _value = -_value;
}

void NumberValue::sum(Value const &value)
{
    NumberValue const *other = dynamic_cast<NumberValue const *>(&value);
    if(!other)
    {
        throw ArithmeticError("NumberValue::sum", "Values cannot be summed");
    }
    
    _value += other->_value;
}

void NumberValue::subtract(Value const &value)
{
    NumberValue const *other = dynamic_cast<NumberValue const *>(&value);
    if(!other)
    {
        throw ArithmeticError("Value::subtract", "Value cannot be subtracted from");    
    }
    
    _value -= other->_value;
}

void NumberValue::divide(Value const &divisor)
{
    NumberValue const *other = dynamic_cast<NumberValue const *>(&divisor);
    if(!other)
    {
        throw ArithmeticError("NumberValue::divide", "Value cannot be divided");
    }
    
    _value /= other->_value;
}
 
void NumberValue::multiply(Value const &value)
{
    NumberValue const *other = dynamic_cast<NumberValue const *>(&value);
    if(!other)
    {
        throw ArithmeticError("NumberValue::multiply", "Value cannot be multiplied");
    }
    
    _value *= other->_value;
}

void NumberValue::modulo(Value const &divisor)
{
    NumberValue const *other = dynamic_cast<NumberValue const *>(&divisor);
    if(!other)
    {
        throw ArithmeticError("Value::modulo", "Modulo not defined");
    }

    // Modulo is done with integers.
    _value = int(_value) % int(other->_value);
}

// Flags for serialization:
static duint8 const SEMANTIC_BOOLEAN = 0x01;
static duint8 const SEMANTIC_HEX     = 0x02;

void NumberValue::operator >> (Writer &to) const
{
    duint8 flags = (_semantic.testFlag(Boolean)? SEMANTIC_BOOLEAN : 0) |
                   (_semantic.testFlag(Hex)?     SEMANTIC_HEX     : 0);

    to << SerialId(NUMBER) << flags << _value;
}

void NumberValue::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if(id != NUMBER)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized value was invalid.
        throw DeserializationError("NumberValue::operator <<", "Invalid ID");
    }
    duint8 flags;
    from >> flags >> _value;

    _semantic = SemanticHints((flags & SEMANTIC_BOOLEAN? Boolean : 0) |
                              (flags & SEMANTIC_HEX?     Hex : 0));
}

} // namespace de
