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

#ifndef LIBDENG2_EXPRESSION_H
#define LIBDENG2_EXPRESSION_H

#include "../ISerializable"

#include <QFlags>

namespace de {

class Evaluator;
class Value;
class Record;

/**
 * Base class for expressions.
 *
 * @note  All expression classes must call the serialization methods of this class
 *        so that the expression flags are properly serialized.
 *
 * @ingroup script
 */
class DENG2_PUBLIC Expression : public ISerializable
{
public:
    /// Deserialization of an expression failed. @ingroup errors
    DENG2_ERROR(DeserializationError);

    // Flags for evaluating expressions.
    // Note: these are serialized as is, so don't change the existing values.
    enum Flag
    {
        /// Evaluates to a value. In conjunction with IMPORT, causes the imported
        /// record to be copied to the local namespace.
        ByValue = 0x1,

        /// Evaluates to a reference.
        ByReference = 0x2,

        /// If missing, create a new variable.
        NewVariable = 0x4,

        /// If missing, create a new subrecord (i.e., Variable that owns a Record).
        NewSubrecord = 0x8,

        /// Identifier must exist and will be deleted.
        //Delete = 0x10,

        /// Imports an external namespace into the local namespace (as a reference).
        Import = 0x20,

        /// Look for object in local namespace only.
        LocalOnly = 0x40,

        /// If the identifier is in scope, returns a reference to the process's
        /// throwaway variable.
        ThrowawayIfInScope = 0x80,

        /// Identifier must not already exist in scope.
        NotInScope = 0x100,

        /// Variable will be set to read-only mode.
        ReadOnly = 0x200,

        /// Variable will be raised into a higher namespace.
        Export = 0x400,

        /// If missing, create a new subrecord. Otherwise, reuse the existing record.
        NewSubrecordIfNotInScope = 0x800
    };
    Q_DECLARE_FLAGS(Flags, Flag)

public:
    Expression();
    
    virtual ~Expression();

    virtual void push(Evaluator &evaluator, Value *scope = 0) const;

    virtual Value *evaluate(Evaluator &evaluator) const = 0;

    /**
     * Returns the flags of the expression.
     */
    Flags const &flags () const;

    /**
     * Sets the flags of the expression.
     *
     * @param f          Flags to set.
     * @param operation  Operation to perform on the flags.
     */
    void setFlags(Flags f, FlagOp operation = ReplaceFlags);

    /**
     * Subclasses must call this in their serialization method.
     */
    void operator >> (Writer &to) const;

    /**
     * Subclasses must call this in their deserialization method.
     */
    void operator << (Reader &from);

public:
    /**
     * Constructs an expression by deserializing one from a reader.
     *
     * @param reader  Reader.
     *
     * @return  The deserialized expression. Caller gets ownership.
     */
    static Expression *constructFrom(Reader &reader);

protected:
    typedef dbyte SerialId;

    enum SerialIds {
        ARRAY,
        BUILT_IN,
        CONSTANT,
        DICTIONARY,
        NAME,
        OPERATOR
    };

private:
    Flags _flags;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Expression::Flags)

} // namespace de

#endif /* LIBDENG2_EXPRESSION_H */
