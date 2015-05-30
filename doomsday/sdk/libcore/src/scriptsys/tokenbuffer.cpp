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

#include "de/TokenBuffer"
#include "de/String"
#include "de/math.h"

#include <cstring>

using namespace de;

// Default size of one allocation pool.
static duint const POOL_SIZE = 1024;

String const Token::PARENTHESIS_OPEN  = "(";
String const Token::PARENTHESIS_CLOSE = ")";
String const Token::BRACKET_OPEN      = "[";
String const Token::BRACKET_CLOSE     = "]";
String const Token::CURLY_OPEN        = "{";
String const Token::CURLY_CLOSE       = "}";
String const Token::COLON             = ":";
String const Token::COMMA             = ",";
String const Token::SEMICOLON         = ";";

bool Token::equals(QChar const *str) const
{
    dsize length = qchar_strlen(str);
    if(length != dsize(size())) return false;
    return String::equals(str, _begin, size());
}

bool Token::beginsWith(QChar const *str) const
{
    dsize length = qchar_strlen(str);
    if(length > dsize(size()))
    {
        // We are shorter than the required beginning string.
        return false;
    }
    return String::equals(str, _begin, length);
}

String Token::asText() const
{
    return String(typeToText(_type)) + " '" + QString(_begin, _end - _begin) +
           "' (on line " + QString::number(_line) + ")";
}

String Token::str() const
{
    return String(_begin, _end - _begin);
}

TokenBuffer::TokenBuffer() : _forming(0), _formPool(0)
{}
 
TokenBuffer::~TokenBuffer()
{}

void TokenBuffer::clear()
{
    _tokens.clear();

    // Empty the allocated pools.
    for(Pools::iterator i = _pools.begin(); i != _pools.end(); ++i)
    {
        i->rover = 0;
    }

    // Reuse allocated pools.
    _formPool = 0;
}

QChar *TokenBuffer::advanceToPoolWithSpace(duint minimum)
{
    for(;; ++_formPool)
    {
        if(_pools.size() == _formPool)
        {
            // Need a new pool.
            _pools.push_back(Pool());
            Pool &newFp = _pools[_formPool];
            newFp.size = POOL_SIZE + minimum;
            newFp.chars.resize(newFp.size);
            return newFp.chars.data();
        }

        Pool &fp = _pools[_formPool];
        if(fp.rover + minimum < fp.size)
        {
            return &fp.chars.data()[fp.rover];
        }
        
        // Can we resize this pool?
        if(!fp.rover)
        {
            fp.size = max(POOL_SIZE + minimum, 2 * minimum);
            fp.chars.resize(fp.size);
            return fp.chars.data();
        }
    }
}

void TokenBuffer::newToken(duint line)
{
    if(_forming)
    {
        // Discard the currently formed token. Use the old start address.
        *_forming = Token(_forming->begin(), _forming->begin(), line);
        return;
    }

    // Determine which pool to use and the starting address.
    QChar *begin = advanceToPoolWithSpace(0);
    
    _tokens.push_back(Token(begin, begin, line));
    _forming = &_tokens.back();
}

void TokenBuffer::appendChar(QChar c)
{
    DENG2_ASSERT(_forming != 0);
        
    // There is at least one character available in the pool.
    _forming->appendChar(c);

    // If we run out of space in the pool, we'll need to relocate the
    // token to a new pool. If the pool is new, or there are no tokens
    // in it yet, we can resize the pool in place.
    Pool &fp = _pools[_formPool];
    if(_forming->end() - fp.chars.data() >= dint(fp.size))
    {
        // The pool is full. Find a new pool and move the token.
        String tok = _forming->str();
        QChar *newBegin = advanceToPoolWithSpace(tok.size());
        memmove(newBegin, tok.data(), tok.size() * sizeof(QChar));
        *_forming = Token(newBegin, newBegin + tok.size(), _forming->line());
    }
}

void TokenBuffer::setType(Token::Type type)
{
    DENG2_ASSERT(_forming != 0);
    _forming->setType(type);
}

void TokenBuffer::endToken()
{
    if(_forming)
    {
        // Update the pool.
        _pools[_formPool].rover += _forming->size();
        
        _forming = 0;
    }
}

dsize TokenBuffer::size() const
{
    return _tokens.size();
}

Token const &TokenBuffer::at(duint i) const
{
    if(i >= _tokens.size())
    {
        /// @throw OutOfRangeError  Index @a i is out of range.
        throw OutOfRangeError("TokenBuffer::at", "Index out of range");
    }
    return _tokens[i];
}

Token const &TokenBuffer::latest() const
{
    return _tokens.back();
}
