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

#ifndef LIBDENG2_STATEMENT_H
#define LIBDENG2_STATEMENT_H

#include "../ISerializable"

namespace de {

class Context;

/**
 * The abstract base class for all statements.
 *
 * @ingroup script
 */
class Statement : public ISerializable
{
public:
    /// Deserialization of a statement failed. @ingroup errors
    DENG2_ERROR(DeserializationError);

public:
    Statement();

    virtual ~Statement();

    virtual void execute(Context &context) const = 0;

    Statement *next() const { return _next; }

    void setNext(Statement *statement) { _next = statement; }

public:
    /**
     * Constructs a statement by deserializing one from a reader.
     *
     * @param from  Reader.
     *
     * @return  The deserialized statement. Caller gets ownership.
     */
    static Statement *constructFrom(Reader &from);

protected:
    typedef dbyte SerialId;

    enum SerialIds {
        ASSIGN,
        CATCH,
        EXPRESSION,
        FLOW,
        FOR,
        FUNCTION,
        IF,
        PRINT,
        TRY,
        WHILE,
        DELETE,
        SCOPE
    };

private:
    /// Pointer to the statement that follows this one, or NULL if
    /// this is the final statement.
    Statement *_next;
};

} // namespace de

#endif /* LIBDENG2_STATEMENT_H */
