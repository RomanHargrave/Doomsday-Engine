/** @file bitfield.h  Array of integers packed together.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_BITFIELD_H
#define LIBDENG2_BITFIELD_H

#include <de/Block>
#include <de/Error>
#include <de/String>

#include <QSet>

namespace de {

/**
 * Array of integer values packed tightly together.
 *
 * Before a BitField can be used, its elements must be defined with
 * BitField::setElements().
 *
 * @ingroup data
 */
class DENG2_PUBLIC BitField
{
public:
    typedef dint Id;
    struct Spec {
        Id id;          ///< User-provided identifier for the element.
        int numBits;    ///< 32 bits at most.
    };
    typedef QSet<Id> Ids;

    /**
     * Metadata about the elements of a bit field.
     */
    class DENG2_PUBLIC Elements
    {
    public:
        Elements();

        Elements(Spec const *elements, dsize count);

        /**
         * Adds a new element into the field.
         *
         * @param id       Identifier of the element.
         * @param numBits  Number of bits for the element.
         *
         * @return Reference to this pack.
         */
        Elements &add(Id id, dsize numBits);

        void add(Spec const *elements, dsize count);

        void add(QList<Spec> const &elements);

        void clear();

        /**
         * Returns the number of elements.
         */
        int size() const;

        Spec at(int index) const;

        void elementLayout(Id const &id, int &firstBit, int &numBits) const;

        /**
         * Total number of bits in the packed elements.
         */
        int bitCount() const;

        /**
         * Returns the identifiers of all elements.
         */
        Ids ids() const;

        /**
         * Returns the ids of all elements that are entirely or partially laid out on
         * byte # @a index.
         *
         * @param index  Index of the byte.
         *
         * @return Element ids.
         */
        Ids idsLaidOutOnByte(int index) const;

    private:
        DENG2_PRIVATE(d)
    };

    /// Failure to compare two fields with each other. @ingroup errors
    DENG2_ERROR(ComparisonError);

public:
    BitField();
    BitField(Elements const &elements);
    BitField(BitField const &other);
    BitField(Block const &data);

    BitField &operator = (BitField const &other);

    /**
     * Sets the elements of the bit field.
     *
     * @param elements  Elements metadata. BitField does not take ownership, so this must
     *                  exist as long as the bit field is in use.
     */
    void setElements(Elements const &elements);

    Elements const &elements() const;

    /**
     * Removes all the elements and the data contained in the bit field. Elements must be
     * redefined with setElements() after calling this.
     */
    void clear();

    bool isEmpty() const;

    /**
     * Returns the number of elements in the bit field.
     */
    //int size() const;

    //Spec element(int index) const;

    /**
     * Total number of bits in the packed elements.
     */
    //int bitCount() const;

    /**
     * Returns the identifiers of all elements.
     */
    //Ids elementIds() const;

    /**
     * Returns the packed data as an array of bytes. Only bitCount() bits are
     * valid; the highest bits of the last byte may be unused (and zero).
     */
    Block data() const;

    bool operator == (BitField const &other) const;
    bool operator != (BitField const &other) const;

    /**
     * Determines which elements in this pack are different when compared to @a
     * other. The fields must use the same number of elements with the same
     * specs.
     *
     * @param other  Other bit field to compare against.
     *
     * @return List of element Ids whose values are different than the ones
     * in @a other.
     */
    Ids delta(BitField const &other) const;

    /**
     * Returns the plain (unsigned integer) value of an element.
     *
     * @param id  Element id.
     *
     * @return Value of the element as unsigned integer.
     */
    duint operator [] (Id id) const { return asUInt(id); }

    void set(Id id, bool value);
    void set(Id id, duint value);

    bool asBool(Id id) const;
    duint asUInt(Id id) const;

    template <typename Type>
    Type valueAs(Id id) const {
        return Type(asUInt(id));
    }

    String asText() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_BITFIELD_H
