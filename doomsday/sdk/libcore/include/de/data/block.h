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

#ifndef LIBDENG2_BLOCK_H
#define LIBDENG2_BLOCK_H

#include "../IByteArray"
#include "../IBlock"

#include <QByteArray>

namespace de {

class IIStream;

/**
 * Data buffer that implements the byte array interface.
 *
 * Note that Block is based on QByteArray, and thus automatically always ensures
 * that the data is followed by a terminating \0 character (even if one is not
 * part of the actual Block contents). Therefore it is safe to use it in functions
 * that assume zero-terminated strings.
 *
 * @ingroup data
 */
class DENG2_PUBLIC Block : public QByteArray, public IByteArray, public IBlock
{
public:
    Block(Size initialSize = 0);
    Block(IByteArray const &array);
    Block(Block const &other);
    Block(QByteArray const &byteArray);
    Block(char const *nullTerminatedCStr);
    Block(void const *data, Size length);

    /**
     * Constructs a block by reading the contents of an input stream. The block
     * will contain all the data that is available immediately; will not wait
     * for additional data to become available later.
     *
     * @param stream  Stream to read from.
     */
    Block(IIStream &stream);

    /**
     * Constructs a block by reading the contents of an input stream in const
     * mode. The block will contain all the data that is available immediately;
     * will not wait for additional data to become available later.
     *
     * @param stream  Stream to read from.
     */
    Block(IIStream const &stream);

    /**
     * Construct a new block and copy its contents from the specified
     * location at another array.
     *
     * @param array  Source data.
     * @param at     Offset within source data.
     * @param count  Number of bytes to copy. Also the size of the new block.
     */
    Block(IByteArray const &array, Offset at, Size count);

    // Implements IByteArray.
    Size size() const;
    void get(Offset at, Byte *values, Size count) const;
    void set(Offset at, Byte const *values, Size count);

    // Implements IBlock.
    void clear();
    void copyFrom(IByteArray const &array, Offset at, Size count);
    void resize(Size size);
    Byte const *data() const;

    Byte *data();

    /// Appends a block after this one.
    Block &operator += (Block const &other);

    /// Appends a byte array after this one.
    Block &operator += (IByteArray const &byteArray);

    /// Copies the contents of another block.
    Block &operator = (Block const &other);

    Block &operator = (IByteArray const &byteArray);
};

} // namespace de
    
#endif // LIBDENG2_BLOCK_H
