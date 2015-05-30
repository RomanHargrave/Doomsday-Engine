/** @file dedregister.h  Register of definitions.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/defs/dedregister.h"

#include <de/ArrayValue>
#include <de/DictionaryValue>
#include <de/TextValue>
#include <de/RecordValue>
#include <QSet>
#include <QMap>

using namespace de;

DENG2_PIMPL(DEDRegister)
, DENG2_OBSERVES(Record, Deletion)
, DENG2_OBSERVES(Record, Addition)
, DENG2_OBSERVES(Record, Removal)
, DENG2_OBSERVES(Variable, ChangeFrom)
{
    Record *names;
    struct Key {
        LookupFlags flags;
        Key(LookupFlags const &f = DefaultLookup) : flags(f) {}
    };
    typedef QMap<String, Key> Keys;
    Keys keys;
    QMap<Variable *, Record *> parents;

    Instance(Public *i, Record &rec) : Base(i), names(&rec)
    {
        names->audienceForDeletion() += this;

        // The definitions will be stored here in the original order.
        names->addArray("order");
    }

    ~Instance()
    {
        if(names) names->audienceForDeletion() -= this;
    }

    void recordBeingDeleted(Record &DENG2_DEBUG_ONLY(record))
    {
        DENG2_ASSERT(names == &record);
        names = 0;
    }

    void clear()
    {
        // As a side-effect, the lookups will be cleared, too, as the members of
        // each definition record are deleted.
        (*names)["order"].value<ArrayValue>().clear();

#ifdef DENG2_DEBUG
        DENG2_ASSERT(parents.isEmpty());
        foreach(String const &key, keys.keys())
        {
            DENG2_ASSERT(lookup(key).size() == 0);
        }
#endif
    }

    void addKey(String const &name, LookupFlags const &flags)
    {
        keys.insert(name, Key(flags));
        names->addDictionary(name + "Lookup");
    }

    ArrayValue &order()
    {
        return (*names)["order"].value<ArrayValue>();
    }

    ArrayValue const &order() const
    {
        return (*names)["order"].value<ArrayValue>();
    }

    DictionaryValue &lookup(String const &keyName)
    {
        return (*names)[keyName + "Lookup"].value<DictionaryValue>();
    }

    DictionaryValue const &lookup(String const &keyName) const
    {
        return (*names)[keyName + "Lookup"].value<DictionaryValue>();
    }

    template <typename Type>
    Type lookupOperation(String const &key, String value,
                         std::function<Type (DictionaryValue const &, String)> operation) const
    {
        auto foundKey = keys.constFind(key);
        if(foundKey == keys.constEnd()) return 0;

        if(!foundKey.value().flags.testFlag(CaseSensitive))
        {
            // Case insensitive lookup is done in lower case.
            value = value.lower();
        }

        return operation(lookup(key), value);
    }

    Record const *tryFind(String const &key, String const &value) const
    {
        return lookupOperation<Record const *>(key, value,
            [] (DictionaryValue const &lut, String v) -> Record const * {
                TextValue const val(v);
                auto i = lut.elements().find(DictionaryValue::ValueRef(&val));
                if(i == lut.elements().end()) return 0; // Value not in dictionary.
                return i->second->as<RecordValue>().record();
            });
    }

    bool has(String const &key, String const &value) const
    {
        return lookupOperation<bool>(key, value, [] (DictionaryValue const &lut, String v) {
            return lut.contains(TextValue(v));
        });
    }

    Record &append()
    {
        Record *sub = new Record;

        // Let each subrecord know their ordinal.
        sub->set("__order__", int(order().size())).setReadOnly();

        // Observe what goes into this record.
        //sub->audienceForDeletion() += this;
        sub->audienceForAddition() += this;
        sub->audienceForRemoval()  += this;

        order().add(new RecordValue(sub, RecordValue::OwnsRecord));
        return *sub;
    }

    bool isEmptyKeyValue(Value const &value) const
    {
        return value.is<TextValue>() && value.asText().isEmpty();
    }

    bool isValidKeyValue(Value const &value) const
    {
        // Empty strings are not indexable.
        if(isEmptyKeyValue(value)) return false;
        return true;
    }

    /// Returns @c true if the value was added.
    bool addToLookup(String const &key, Value const &value, Record &def)
    {
        if(!isValidKeyValue(value))
            return false;

        String valText = value.asText();
        DENG2_ASSERT(!valText.isEmpty());
        DENG2_ASSERT(keys.contains(key));

        if(!keys[key].flags.testFlag(CaseSensitive))
        {
            valText = valText.lower();
        }

        DictionaryValue &dict = lookup(key);

        if(keys[key].flags.testFlag(OnlyFirst))
        {
            // Only index the first one that is found.
            if(dict.contains(TextValue(valText))) return false;
        }

        // Index definition using its current value.
        dict.add(new TextValue(valText), new RecordValue(&def));
        return true;
    }

    bool removeFromLookup(String const &key, Value const &value, Record &def)
    {
        //qDebug() << "removeFromLookup" << key << value.asText() << &def;

        if(!isValidKeyValue(value))
            return false;

        String valText = value.asText();
        DENG2_ASSERT(!valText.isEmpty());
        DENG2_ASSERT(keys.contains(key));

        if(!keys[key].flags.testFlag(CaseSensitive))
        {
            valText = valText.lower();
        }

        DictionaryValue &dict = lookup(key);

        // Remove from the index.
        if(dict.contains(TextValue(valText)))
        {
            Value const &indexed = dict.element(TextValue(valText));
            //qDebug() << " -" << indexed.as<RecordValue>().record() << &def;
            Record const *indexedDef = indexed.as<RecordValue>().record();
            if(!indexedDef || indexedDef == &def)
            {
                // This is the definition that was indexed using the key value.
                // Let's remove it.
                dict.remove(TextValue(valText));

                /// @todo Should now index any other definitions with this key value;
                /// needs to add a lookup of which other definitions have this value.
                return true;
            }
        }

        // There was some other definition indexed using this key.
        return false;
    }

    void recordMemberAdded(Record &def, Variable &key)
    {
        // Keys must be observed so that they are indexed in the lookup table.
        if(keys.contains(key.name()))
        {
            // Index definition using its current value.
            // Observe empty keys so we'll get the key's value when it's set.
            if(addToLookup(key.name(), key.value(), def) ||
               isEmptyKeyValue(key.value()))
            {
                parents.insert(&key, &def);
                key.audienceForChangeFrom() += this;
            }
        }
    }

    void recordMemberRemoved(Record &def, Variable &key)
    {
        if(keys.contains(key.name()))
        {
            key.audienceForChangeFrom() -= this;
            parents.remove(&key);
            removeFromLookup(key.name(), key.value(), def);
        }
    }

    void variableValueChangedFrom(Variable &key, Value const &oldValue, Value const &newValue)
    {
        //qDebug() << "changed" << key.name() << "from" << oldValue.asText() << "to"
        //         << newValue.asText();

        DENG2_ASSERT(parents.contains(&key));

        // The value of a key has changed, so it needs to be reindexed.
        removeFromLookup(key.name(), oldValue, *parents[&key]);
        addToLookup(key.name(), newValue, *parents[&key]);
    }
};

DEDRegister::DEDRegister(Record &names) : d(new Instance(this, names))
{}

void DEDRegister::addLookupKey(String const &variableName, LookupFlags const &flags)
{
    d->addKey(variableName, flags);
}

void DEDRegister::clear()
{
    d->clear();
}

Record &DEDRegister::append()
{
    return d->append();
}

Record &DEDRegister::appendCopy(int index)
{
    return copy(index, append());
}

Record &DEDRegister::copy(int fromIndex, Record &to)
{
    QStringList omitted;
    omitted << "__.*"; // double-underscore

    // By default lookup keys are not copied, as they are used as identifiers and
    // therefore duplicates should not occur.
    DENG2_FOR_EACH_CONST(Instance::Keys, i, d->keys)
    {
        if(i.value().flags.testFlag(AllowCopy)) continue;
        omitted << i.key();
    }

    return to.assign((*this)[fromIndex], QRegExp(omitted.join("|")));
}

int DEDRegister::size() const
{
    return d->order().size();
}

bool DEDRegister::has(String const &key, String const &value) const
{
    return d->has(key, value);
}

Record &DEDRegister::operator [] (int index)
{
    return *d->order().at(index).as<RecordValue>().record();
}

Record const &DEDRegister::operator [] (int index) const
{
    return *d->order().at(index).as<RecordValue>().record();
}

Record *DEDRegister::tryFind(String const &key, String const &value)
{
    return const_cast<Record *>(d->tryFind(key, value));
}

Record const *DEDRegister::tryFind(String const &key, String value) const
{
    return d->tryFind(key, value);
}

Record &DEDRegister::find(String const &key, String const &value)
{
    return const_cast<Record &>(const_cast<DEDRegister const *>(this)->find(key, value));
}

Record const &DEDRegister::find(String const &key, String const &value) const
{
    if(!d->keys.contains(key))
    {
        throw UndefinedKeyError("DEDRegister::find", "Key '" + key + "' not defined");
    }
    Record const *rec = tryFind(key, value);
    if(!rec)
    {
        throw NotFoundError("DEDRegister::find", key + " '" + value + "' not found");
    }
    return *rec;
}

DictionaryValue const &DEDRegister::lookup(String const &key) const
{
    if(!d->keys.contains(key))
    {
        throw UndefinedKeyError("DEDRegister::lookup", "Key '" + key + "' not defined");
    }
    return d->lookup(key);
}
