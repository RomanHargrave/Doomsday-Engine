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

#ifndef LIBDENG2_FIXEDBYTEARRAY_H
#define LIBDENG2_FIXEDBYTEARRAY_H

#include "../ByteSubArray"

namespace de {

/**
 * Byte array of fixed size. This is a utility that points to a fixed-length
 * region of another byte array.
 *
 * @ingroup data
 */
class DENG2_PUBLIC FixedByteArray : public ByteSubArray
{
public:
    /**
     * Constructs a modifiable fixed-length byte array out of an entire
     * byte array.
     *
     * @param mainArray  Array that holds the data.
     */
    FixedByteArray(IByteArray &mainArray);

    /**
     * Constructs a modifiable fixed-length byte array out of a portion
     * of another byte array.
     *
     * @param mainArray  Array that holds the data.
     * @param at         Start of fixed-length region.
     * @param size       Size of fixed-length region.
     */
    FixedByteArray(IByteArray &mainArray, Offset at, Size size);

    /**
     * Constructs a non-modifiable fixed-length byte array out of an entire
     * byte array.
     *
     * @param mainArray  Array that holds the data.
     */
    FixedByteArray(IByteArray const &mainArray);

    /**
     * Constructs a non-modifiable fixed-length byte array out of a portion
     * of another byte array.
     *
     * @param mainArray  Array that holds the data.
     * @param at         Start of fixed-length region.
     * @param size       Size of fixed-length region.
     */
    FixedByteArray(IByteArray const &mainArray, Offset at, Size size);

    void set(Offset at, Byte const *values, Size count);
};

} // namespace de

#endif /* LIBDENG2_FIXEDBYTEARRAY_H */
