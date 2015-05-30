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

#ifndef LIBDENG2_PACKET_H
#define LIBDENG2_PACKET_H

#include "../ISerializable"
#include "../Address"
#include "../String"
#include "../Reader"

namespace de {

class String;
    
/**
 * Base class for all network packets in the libcore network communications
 * protocol. All packets are based on this class.
 *
 * @ingroup protocol
 */
class DENG2_PUBLIC Packet : public ISerializable
{
public:
    /// While deserializing, an invalid type identifier was encountered. @ingroup errors
    DENG2_SUB_ERROR(DeserializationError, InvalidTypeError);

    /// Length of a type identifier.
    static dint const TYPE_SIZE = 4;

    typedef String Type;

public:
    /**
     * Constructs an empty packet.
     *
     * @param type  Type identifier of the packet.
     */
    Packet(Type const &type);

    virtual ~Packet() {}

    /**
     * Returns the type identifier of the packet.
     */
    Type const &type() const { return _type; }

    /**
     * Determines where the packet was received from.
     */
    Address const &from() const { return _from; }

    /**
     * Sets the address where the packet was received from.
     *
     * @param from  Address of the sender.
     */
    void setFrom(Address const &from) { _from = from; }

    /**
     * Execute whatever action the packet defines. This is called for all
     * packets once received and interpreted by the protocol. A packet defined
     * outside libcore may use this to add functionality to the packet.
     */
    virtual void execute() const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

protected:
    /**
     * Sets the type identifier.
     *
     * @param t  Type identifier. Must be exactly TYPE_SIZE characters long.
     */
    void setType(Type const &t);

public:
    /**
     * Checks if the packet starting at the current offset in the reader
     * has the given type identifier.
     *
     * @param from  Reader.
     * @param type  Packet identifier.
     */
    static bool checkType(Reader &from, String const &type);

    template <typename PacketType>
    static PacketType *constructFromBlock(Block const &block, char const *packetTypeIdentifier)
    {
        Reader from(block);
        if(checkType(from, packetTypeIdentifier))
        {
            std::auto_ptr<PacketType> p(new PacketType);
            from >> *static_cast<IReadable *>(p.get());
            return p.release();
        }
        return 0;
    }

private:
    /// The type is identified with a four-character string.
    Type _type;

    /// Address where the packet was received from.
    Address _from;
};

} // namespace de

#endif /* LIBDENG2_PACKET_H */
