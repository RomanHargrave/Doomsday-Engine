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

#include "de/Block"
#include "de/File"

using namespace de;

Block::Block(Size initialSize)
{
    resize(initialSize);
}

Block::Block(IByteArray const &other)
{
    // Read the other's data directly into our data buffer.
    resize(other.size());
    other.get(0, (dbyte *) data(), other.size());
}

Block::Block(Block const &other)
    : QByteArray(other), IByteArray(), IBlock()
{}

Block::Block(QByteArray const &byteArray)
    : QByteArray(byteArray)
{}

Block::Block(char const *nullTerminatedCStr)
    : QByteArray(nullTerminatedCStr)
{}

Block::Block(void const *data, Size length)
    : QByteArray(reinterpret_cast<char const *>(data), int(length)), IByteArray(), IBlock()
{}

Block::Block(IIStream &stream)
{
    stream >> *this;
}

Block::Block(IIStream const &stream)
{
    stream >> *this;
}

Block::Block(IByteArray const &other, Offset at, Size count) : IByteArray()
{
    copyFrom(other, at, count);
}

Block::Size Block::size() const
{
    return QByteArray::size();
}

void Block::get(Offset atPos, Byte *values, Size count) const
{
    if(atPos + count > size())
    {
        /// @throw OffsetError The accessed region of the block was out of range.
        throw OffsetError("Block::get", "Out of range " +
                          String("(%1[+%2] > %3)").arg(atPos).arg(count).arg(size()));
    }

    for(Offset i = atPos; count > 0; ++i, --count)
    {
        *values++ = Byte(at(i));
    }
}

void Block::set(Offset at, Byte const *values, Size count)
{
    if(at > size())
    {
        /// @throw OffsetError The accessed region of the block was out of range.
        throw OffsetError("Block::set", "Out of range");
    }
    replace(at, count, QByteArray((char const *) values, count));
}

void Block::copyFrom(IByteArray const &array, Offset at, Size count)
{
    // Read the other's data directly into our data buffer.
    resize(count);
    array.get(at, data(), count);
}

void Block::resize(Size size)
{
    QByteArray::resize(size);
}

Block::Byte *Block::data()
{
    return reinterpret_cast<Byte *>(QByteArray::data());
}

Block::Byte const *Block::data() const
{
    return reinterpret_cast<Byte const *>(QByteArray::data());
}

Block &Block::operator += (Block const &other)
{
    append(other);
    return *this;
}

Block &Block::operator += (IByteArray const &byteArray)
{
    Offset pos = size();
    resize(size() + byteArray.size());
    byteArray.get(0, data() + pos, byteArray.size());
    return *this;
}

Block &Block::operator = (Block const &other)
{
    *static_cast<QByteArray *>(this) = static_cast<QByteArray const &>(other);
    return *this;
}

Block &Block::operator = (IByteArray const &byteArray)
{
    copyFrom(byteArray, 0, byteArray.size());
    return *this;
}

void Block::clear()
{
    QByteArray::clear();
}
