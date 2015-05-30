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

#ifndef LIBDENG2_WAITABLEFIFO_H
#define LIBDENG2_WAITABLEFIFO_H

#include "../FIFO"
#include "../Waitable"

namespace de {

/**
 * FIFO with a semaphore that allows threads to wait until there are objects in
 * the buffer.
 *
 * @ingroup data
 */
template <typename Type>
class WaitableFIFO : public FIFO<Type>, public Waitable
{
public:
    WaitableFIFO() {}

    void put(Type *object, typename FIFO<Type>::PutMode mode = FIFO<Type>::PutHead) {
        FIFO<Type>::put(object, mode);
        post();
    }

    Type *take() {
        wait();
        return FIFO<Type>::take();
    }
};

} // namespace de

#endif // LIBDENG2_WAITABLEFIFO_H
