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

#ifndef LIBDENG2_CATCHSTATEMENT_H
#define LIBDENG2_CATCHSTATEMENT_H

#include "../Statement"
#include "../ArrayExpression"
#include "../Compound"

#include <QFlags>

namespace de {

/**
 * Catches an exception that occurs within a try compound.
 */
class CatchStatement : public Statement
{
public:
    enum Flag
    {
        /// The final catch compound in a sequence of catch compounds.
        FinalCompound = 0x1
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    /// Flags.
    Flags flags;

public:
    CatchStatement(ArrayExpression *args = 0);
    ~CatchStatement();

    Compound &compound() { return _compound; }

    /// Skips the catch compound (called only during normal execution).
    void execute(Context &context) const;

    bool isFinal() const;

    /**
     * Determines whether the catch statement will catch an error.
     *
     * @param err  Error to check.
     *
     * @return  @c true, if the error is caught and catch compound should be executed.
     */
    bool matches(Error const &err) const;

    /**
     * Assigns the exception to the specified variable and begins the catch compound.
     *
     * @param context  Execution context.
     * @param err      Error.
     */
    void executeCatch(Context &context, Error const &err) const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    ArrayExpression *_args;
    Compound _compound;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(CatchStatement::Flags)

} // namespace de

#endif /* LIBDENG2_CATCHSTATEMENT_H */
