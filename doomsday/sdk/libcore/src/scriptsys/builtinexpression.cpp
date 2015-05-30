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

#include "de/BuiltInExpression"
#include "de/Evaluator"
#include "de/NumberValue"
#include "de/TextValue"
#include "de/ArrayValue"
#include "de/DictionaryValue"
#include "de/RefValue"
#include "de/RecordValue"
#include "de/BlockValue"
#include "de/TimeValue"
#include "de/Writer"
#include "de/Reader"
#include "de/Script"
#include "de/Process"
#include "de/File"
#include "de/App"
#include "de/math.h"

using namespace de;

BuiltInExpression::BuiltInExpression() : _type(NONE), _arg(0)
{}

BuiltInExpression::BuiltInExpression(Type type, Expression *argument)
    : _type(type), _arg(argument)
{}

BuiltInExpression::~BuiltInExpression()
{
    delete _arg;
}

void BuiltInExpression::push(Evaluator &evaluator, Value *scope) const
{
    Expression::push(evaluator, scope);
    _arg->push(evaluator);
}

Value *BuiltInExpression::evaluate(Evaluator &evaluator) const
{
    std::auto_ptr<Value> value(evaluator.popResult());
    ArrayValue const &args = value.get()->as<ArrayValue>();
    
    switch(_type)
    {
    case LENGTH:
        if(args.size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for LENGTH");
        }
        return new NumberValue(args.at(1).size());
        
    case DICTIONARY_KEYS:
    case DICTIONARY_VALUES:
    {
        if(args.size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for " +
                String(_type == DICTIONARY_KEYS?
                       "DICTIONARY_KEYS" : "DICTIONARY_VALUES"));
        }
        
        DictionaryValue const *dict = dynamic_cast<DictionaryValue const *>(&args.at(1));
        if(!dict)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Argument must be a dictionary");
        }

        return dict->contentsAsArray(_type == DICTIONARY_KEYS ? DictionaryValue::Keys
                                                              : DictionaryValue::Values);
    }

    case DIR:
    {
        if(args.size() > 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                                      "Expected exactly at most one arguments for DIR");
        }

        Record const *ns;
        if(args.size() == 1)
        {
            ns = evaluator.localNamespace();
        }
        else
        {
            ns = &args.at(1).as<RecordValue>().dereference();
        }

        ArrayValue *keys = new ArrayValue;
        DENG2_FOR_EACH_CONST(Record::Members, i, ns->members())
        {
            *keys << new TextValue(i.key());
        }
        return keys;
    }
    
    case RECORD_MEMBERS:    
    case RECORD_SUBRECORDS:
    {
        if(args.size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for " +
                String(_type == RECORD_MEMBERS? "RECORD_MEMBERS" : "RECORD_SUBRECORDS"));
        }
        
        RecordValue const *rec = dynamic_cast<RecordValue const *>(&args.at(1));
        if(!rec)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Argument must be a record");
        }
        DictionaryValue *dict = new DictionaryValue();
        if(_type == RECORD_MEMBERS)
        {
            for(Record::Members::const_iterator i = rec->dereference().members().begin();
                i != rec->dereference().members().end(); ++i)
            {
                dict->add(new TextValue(i.key()), new RefValue(i.value()));
            }
        }
        else
        {
            Record::Subrecords subs = rec->dereference().subrecords();
            DENG2_FOR_EACH(Record::Subrecords, i, subs)
            {
                dict->add(new TextValue(i.key()), new RecordValue(i.value()));
            }
        }
        return dict;
    }

    case AS_RECORD:
    {
        if(args.size() == 1)
        {
            // No arguments: produce an owned, empty Record.
            return new RecordValue(new Record, RecordValue::OwnsRecord);
        }
        if(args.size() == 2)
        {
            // One argument: make an owned copy of a referenced record.
            RecordValue const *rec = dynamic_cast<RecordValue const *>(&args.at(1));
            if(!rec)
            {
                throw WrongArgumentsError("BuiltInExpression::evaluate",
                                          "Argument 1 of AS_RECORD must be a record");
            }
            return new RecordValue(new Record(*rec->record()), RecordValue::OwnsRecord);
        }
        throw WrongArgumentsError("BuiltInExpression::evaluate",
                                  "Expected less than two arguments for AS_RECORD");
    }

    case AS_FILE:
    {
        if(args.size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                                      "Expected exactly one argument for AS_FILE");
        }
        // The only argument is an absolute path of the file.
        return new RecordValue(App::rootFolder().locate<File>(args.at(1).asText()).info());
    }
    
    case AS_NUMBER:
        if(args.size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                                      "Expected exactly one argument for AS_NUMBER");
        }
        return new NumberValue(args.at(1).asNumber());

    case AS_TEXT:
        if(args.size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                                      "Expected exactly one argument for AS_TEXT");
        }
        return new TextValue(args.at(1).asText());

    case AS_TIME:
        if(args.size() == 1)
        {
            // Current time.
            return new TimeValue;
        }
        if(args.size() == 2)
        {
            Time t = Time::fromText(args.at(1).asText());
            if(!t.isValid())
            {
                // Maybe just a date?
                t = Time::fromText(args.at(1).asText(), Time::ISODateOnly);
            }
            return new TimeValue(t);
        }
        throw WrongArgumentsError("BuiltInExpression::evaluate",
                                  "Expected less than two arguments for AS_TIME");

    case TIME_DELTA:
    {
        if(args.size() != 3)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                                      "Expected exactly two arguments for TIME_DELTA");
        }
        TimeValue const *fromTime = dynamic_cast<TimeValue const *>(&args.at(1));
        if(!fromTime)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate", "Argument 1 of TIME_DELTA must be a time");
        }
        TimeValue const *toTime = dynamic_cast<TimeValue const *>(&args.at(2));
        if(!toTime)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate", "Argument 2 of TIME_DELTA must be a time");
        }
        return new NumberValue(toTime->time() - fromTime->time());
    }

    case LOCAL_NAMESPACE:
    {
        if(args.size() != 1)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "No arguments expected for LOCAL_NAMESPACE");
        }
        return new RecordValue(evaluator.localNamespace());
    }

    case GLOBAL_NAMESPACE:
    {
        if(args.size() != 1)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "No arguments expected for GLOBAL_NAMESPACE");
        }
        return new RecordValue(evaluator.process().globals());
    }

    case SERIALIZE:
    {
        if(args.size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for SERIALIZE");
        }
        std::auto_ptr<BlockValue> data(new BlockValue);
        Writer(*data.get()) << args.at(1);
        return data.release();
    }
    
    case DESERIALIZE:
    {
        if(args.size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for DESERIALIZE");
        }
        BlockValue const *block = dynamic_cast<BlockValue const *>(&args.at(1));
        if(block)
        {
            Reader reader(*block);
            return Value::constructFrom(reader);
        }
        /*
        // Alternatively allow deserializing from a text value.
        TextValue const *text = dynamic_cast<TextValue const *>(&args.at(1));
        if(text)
        {
            return Value::constructFrom(Reader(Block(text->asText().toUtf8())));
        }
        */
        throw WrongArgumentsError("BuiltInExpression::evaluate",
            "deserialize() can operate only on block values");
    }
        
    case FLOOR:
        if(args.size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                                      "Expected exactly one argument for FLOOR");
        }
        return new NumberValue(std::floor(args.at(1).asNumber()));

    case EVALUATE:
    {
        if(args.size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                                      "Expected exactly one argument for EVALUATE");
        }
        // Set up a subprocess in the local namespace.
        Process subProcess(evaluator.localNamespace());
        // Parse the argument as a script.
        Script subScript(args.at(1).asText());
        subProcess.run(subScript);
        subProcess.execute();
        // A copy of the result value is returned.
        return subProcess.context().evaluator().result().duplicate();
    }

    default:
        DENG2_ASSERT(false);
    }
    return NULL;
}

void BuiltInExpression::operator >> (Writer &to) const
{
    to << SerialId(BUILT_IN);

    Expression::operator >> (to);

    to << duint8(_type) << *_arg;
}

void BuiltInExpression::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if(id != BUILT_IN)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized expression was invalid.
        throw DeserializationError("BuiltInExpression::operator <<", "Invalid ID");
    }

    Expression::operator << (from);

    duint8 t;
    from >> t;
    _type = Type(t);
    delete _arg;
    _arg = 0;
    _arg = Expression::constructFrom(from);
}

static struct {
    char const *str;
    BuiltInExpression::Type type;
} types[] = {
    { "File",        BuiltInExpression::AS_FILE },
    { "Number",      BuiltInExpression::AS_NUMBER },
    { "Record",      BuiltInExpression::AS_RECORD },
    { "Text",        BuiltInExpression::AS_TEXT },
    { "Time",        BuiltInExpression::AS_TIME },
    { "deserialize", BuiltInExpression::DESERIALIZE },
    { "dictkeys",    BuiltInExpression::DICTIONARY_KEYS },
    { "dictvalues",  BuiltInExpression::DICTIONARY_VALUES },
    { "dir",         BuiltInExpression::DIR },
    { "eval",        BuiltInExpression::EVALUATE },
    { "floor",       BuiltInExpression::FLOOR },
    { "globals",     BuiltInExpression::GLOBAL_NAMESPACE },
    { "len",         BuiltInExpression::LENGTH },
    { "locals",      BuiltInExpression::LOCAL_NAMESPACE },
    { "members",     BuiltInExpression::RECORD_MEMBERS },
    { "serialize",   BuiltInExpression::SERIALIZE },
    { "subrecords",  BuiltInExpression::RECORD_SUBRECORDS },
    { "timedelta",   BuiltInExpression::TIME_DELTA },
    { NULL,          BuiltInExpression::NONE }
};

BuiltInExpression::Type BuiltInExpression::findType(String const &identifier)
{    
    for(duint i = 0; types[i].str; ++i)
    {
        if(identifier == types[i].str)
        {
            return types[i].type;
        }
    }
    return NONE;
}

StringList BuiltInExpression::identifiers()
{
    StringList names;
    for(int i = 0; types[i].str; ++i)
    {
        names << types[i].str;
    }
    return names;
}
