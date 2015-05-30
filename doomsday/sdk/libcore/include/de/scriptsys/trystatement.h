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

#ifndef LIBDENG2_TRYSTATEMENT_H
#define LIBDENG2_TRYSTATEMENT_H

#include "../Statement"
#include "../Compound"

namespace de {

/**
 * Begins a try/catch compound. Always followed by one or more catch statements.
 */
class TryStatement : public Statement
{
public:
    void execute(Context &context) const;

    Compound &compound() { return _compound; }

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    Compound _compound;
};

} // namespace de

#endif /* LIBDENG2_TRYSTATEMENT_H */
