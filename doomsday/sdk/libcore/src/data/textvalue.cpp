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

#include "de/TextValue"
#include "de/NumberValue"
#include "de/ArrayValue"
#include "de/String"
#include "de/Writer"
#include "de/Reader"
#include "de/ScriptSystem"

#include <QTextStream>
#include <list>
#include <cmath>

using namespace de;
using std::list;

TextValue::TextValue(String const &initialValue)
    : _value(initialValue)
{}

TextValue::operator String const &() const
{
    return _value;
}

Value *TextValue::duplicate() const
{
    return new TextValue(_value);
}

Value::Number TextValue::asNumber() const
{
    return _value.toDouble();
}

Value::Text TextValue::asText() const
{
    return _value;
}

Record *TextValue::memberScope() const
{
    return &ScriptSystem::builtInClass("String");
}

dsize TextValue::size() const
{
    return _value.size();
}

bool TextValue::isTrue() const
{
    // If there is at least one nonwhite character, this is considered a truth.
    for(Text::const_iterator i = _value.begin(); i != _value.end(); ++i)
    {
        if(!(*i).isSpace())
            return true;
    }
    return false;
}

dint TextValue::compare(Value const &value) const
{
    TextValue const *other = dynamic_cast<TextValue const *>(&value);
    if(other)
    {
        return _value.compare(other->_value);
    }
    return Value::compare(value);
}

void TextValue::sum(Value const &value)
{
    TextValue const *other = dynamic_cast<TextValue const *>(&value);
    if(!other)
    {
        throw ArithmeticError("TextValue::sum", "Value cannot be summed");
    }
    
    _value += other->_value;
}

void TextValue::multiply(Value const &value)
{
    NumberValue const *other = dynamic_cast<NumberValue const *>(&value);
    if(!other)
    {
        throw ArithmeticError("Value::multiply", "Value cannot be multiplied");
    }
    
    ddouble factor = other->asNumber();
    
    if(factor <= 0)
    {
        _value.clear();
    }
    else
    {
        QString str;
        QTextStream os(&str);
        while(factor-- > 1)
        {
            os << _value;
        }
        // The remainder.
        dint remain = dint(std::floor((factor + 1) * _value.size() + .5));
        os << _value.substr(0, remain);
        _value = str;
    }
}

void TextValue::divide(Value const &value)
{
    TextValue const *other = dynamic_cast<TextValue const *>(&value);
    if(!other)
    {
        throw ArithmeticError("TextValue::divide", "Text cannot be divided");
    }
    _value = _value / other->_value;
}

void TextValue::modulo(Value const &value)
{
    list<Value const *> args;
    
    ArrayValue const *array = dynamic_cast<ArrayValue const *>(&value);
    if(array)
    {
        for(ArrayValue::Elements::const_iterator i = array->elements().begin();
            i != array->elements().end(); ++i)
        {
            args.push_back(*i);
        }
    }
    else
    {
        // Just one.
        args.push_back(&value);
    }
    
    _value = substitutePlaceholders(_value, args);
}

String TextValue::substitutePlaceholders(String const &pattern, const std::list<Value const *> &args)
{
    QString result;
    QTextStream out(&result);
    list<Value const *>::const_iterator arg = args.begin();
    
    for(String::const_iterator i = pattern.begin(); i != pattern.end(); ++i)
    {
        QChar ch = *i;
        
        if(ch == '%')
        {
            if(arg == args.end())
            {
                throw IllegalPatternError("TextValue::replacePlaceholders",
                    "Too few substitution values");
            }
            
            out << String::patternFormat(i, pattern.end(), **arg);
            ++arg;
        }
        else
        {
            out << ch;
        }
    }
    
    return result;
}

void TextValue::operator >> (Writer &to) const
{
    to << SerialId(TEXT) << _value;
}

void TextValue::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if(id != TEXT)
    {
        throw DeserializationError("TextValue::operator <<", "Invalid ID");
    }
    from >> _value;
}

void TextValue::setValue(String const &text)
{
    _value = text;
}
