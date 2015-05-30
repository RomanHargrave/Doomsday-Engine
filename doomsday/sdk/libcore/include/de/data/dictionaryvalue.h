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

#ifndef LIBDENG2_DICTIONARYVALUE_H
#define LIBDENG2_DICTIONARYVALUE_H

#include "../Value"

#include <map>

namespace de {

class ArrayValue;

/**
 * Subclass of Value that contains an array of values, indexed by any value.
 *
 * @ingroup data
 */
class DENG2_PUBLIC DictionaryValue : public Value
{
public:
    /// An invalid key was used. @ingroup errors
    DENG2_ERROR(KeyError);

    struct ValueRef {
        ValueRef(Value const *v) : value(v) {}
        ValueRef(ValueRef const &other) : value(other.value) {}

        // Operators for the map.
        bool operator < (ValueRef const &other) const {
            return value->compare(*other.value) < 0;
        }

        Value const *value;
    };

    typedef std::map<ValueRef, Value *> Elements;

public:
    DictionaryValue();
    DictionaryValue(DictionaryValue const &other);
    ~DictionaryValue();

    /// Returns a direct reference to the elements map.
    Elements const &elements() const { return _elements; }

    /// Returns a direct reference to the elements map.
    Elements &elements() { return _elements; }

    /**
     * Clears the dictionary of all values.
     */
    void clear();

    /**
     * Adds a key-value pair to the dictionary. If the key already exists, its
     * old value will be replaced by the new one.
     *
     * @param key    Key. Ownership given to DictionaryValue.
     * @param value  Value. Ownership given to DictionaryValue.
     */
    void add(Value *key, Value *value);

    /**
     * Removes a key-value pair from the dictionary.
     *
     * @param key  Key that will be removed.
     */
    void remove(Value const &key);

    void remove(Elements::iterator const &pos);

    enum ContentSelection { Keys, Values };

    /**
     * Creates an array with the keys or the values of the dictionary.
     *
     * @param selection  Keys or values.
     *
     * @return Caller gets ownership.
     */
    ArrayValue *contentsAsArray(ContentSelection selection) const;

    // Implementations of pure virtual methods.
    Value *duplicate() const;
    Text asText() const;
    Record *memberScope() const;
    dsize size() const;
    Value const &element(Value const &index) const;
    Value &element(Value const &index);
    void setElement(Value const &index, Value *value);
    bool contains(Value const &value) const;
    Value *begin();
    Value *next();
    bool isTrue() const;
    dint compare(Value const &value) const;
    void sum(Value const &value);
    void subtract(Value const &subtrahend);

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    Elements _elements;
    Elements::iterator _iteration;
    bool _validIteration;
};

} // namespace de

#endif /* LIBDENG2_DICTIONARYVALUE_H */
