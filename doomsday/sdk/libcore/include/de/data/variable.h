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

#ifndef LIBDENG2_VARIABLE_H
#define LIBDENG2_VARIABLE_H

#include "../ISerializable"
#include "../String"
#include "../Audience"

#include <QFlags>

namespace de {

class Value;
class Record;
    
/**
 * Stores a value and name identifier. Variables are typically stored in a Record.
 * A variable's behavior is defined by its mode flags.
 *
 * @ingroup data
 */
class DENG2_PUBLIC Variable : public ISerializable
{
public:
    /// There was an attempt to change the value of a read-only variable. @ingroup errors
    DENG2_ERROR(ReadOnlyError);

    /// An invalid value type was used. The mode flags denied using a value of the
    /// given type with the variable. @ingroup errors
    DENG2_ERROR(InvalidError);

    /// Variable name contains invalid characters. @ingroup errors
    DENG2_ERROR(NameError);

    /// Value could not be converted to the attempted type. @ingroup errors
    DENG2_ERROR(TypeError);

    /** @name Mode Flags */
    //@{
    enum Flag
    {
        /// Variable's value cannot change.
        ReadOnly = 0x1,

        /// Variable cannot be serialized.
        NoSerialize = 0x2,

        /// NoneValue allowed as value.
        AllowNone = 0x4,

        /// NumberValue allowed as value.
        AllowNumber = 0x8,

        /// TextValue allowed as value.
        AllowText = 0x10,

        /// ArrayValue allowed as value.
        AllowArray = 0x20,

        /// DictionaryValue allowed as value.
        AllowDictionary = 0x40,

        /// BlockValue allowed as value.
        AllowBlock = 0x80,

        /// FunctionValue allowed as value.
        AllowFunction = 0x100,

        /// RecordValue allowed as value.
        AllowRecord = 0x200,

        /// RefValue allowed as value.
        AllowRef = 0x400,

        /// TimeValue allowed as value.
        AllowTime = 0x800,

        /// The default mode allows reading and writing all types of values,
        /// including NoneValue.
        DefaultMode = AllowNone | AllowNumber | AllowText | AllowArray |
            AllowDictionary | AllowBlock | AllowFunction | AllowRecord |
            AllowRef | AllowTime
    };
    //@}
    Q_DECLARE_FLAGS(Flags, Flag)

public:
    /**
     * Constructs a new variable.
     *
     * @param name  Name for the variable. Any periods (.) are not allowed.
     * @param initial  Initial value. Variable gets ownership. If no value is given here,
     *      a NoneValue will be created for the variable.
     * @param varMode  Mode flags.
     */
    Variable(String const &name = "", Value *initial = 0,
             Flags const &varMode = DefaultMode);

    /**
     * Constructs a copy of another variable.
     *
     * @param other  Variable to copy.
     */
    Variable(Variable const &other);

    virtual ~Variable();

    /**
     * Returns the name of the variable.
     */
    String const &name() const;

    /**
     * Sets the value of the variable.
     *
     * @param v  New value. Variable gets ownership. Cannot be NULL.
     */
    Variable &set(Value *v);

    /**
     * Sets the value of the variable.
     *
     * @param v  New value. Variable gets ownership. Cannot be NULL.
     */
    Variable &operator = (Value *v);

    /**
     * Sets the value of the variable.
     *
     * @param v  New value. Variable takes a copy of this.
     */
    Variable &set(Value const &v);

    /**
     * Returns the value of the variable (non-modifiable).
     */
    Value const &value() const;

    /**
     * Returns the value of the variable.
     */
    Value &value();

    Value *valuePtr();
    Value const *valuePtr() const;

    /**
     * Returns the value of the variable.
     */
    template <typename Type>
    Type &value() {
        Type *v = dynamic_cast<Type *>(valuePtr());
        if(!v) {
            /// @throw TypeError Casting to Type failed.
            throw TypeError("Variable::value",
                            QString("Illegal type conversion to ") + typeid(Type).name());
        }
        return *v;
    }

    /**
     * Returns the value of the variable.
     */
    template <typename Type>
    Type const &value() const {
        Type const *v = dynamic_cast<Type const *>(valuePtr());
        if(!v) {
            /// @throw TypeError Casting to Type failed.
            throw TypeError("Variable::value",
                            QString("Illegal type conversion to ") + typeid(Type).name());
        }
        return *v;
    }

    /**
     * Returns the Record that the variable references. If the variable does
     * not have a RecordValue, an exception is thrown.
     *
     * @return Referenced Record.
     */
    Record const &valueAsRecord() const;

    Record &valueAsRecord();

    operator Record & ();

    operator Record const & () const;

    // Automatic conversion to native primitive types.
    operator String () const;
    operator QString () const;
    operator ddouble () const;

    /**
     * Returns the current mode flags of the variable.
     */
    Flags mode() const;

    /**
     * Sets the mode flags of the variable.
     *
     * @param flags      New mode flags.
     * @param operation  What to do with @a flags.
     */
    void setMode(Flags const &flags, FlagOp operation = ReplaceFlags);

    /**
     * Makes the variable read-only.
     *
     * @return Reference to this variable.
     */
    Variable &setReadOnly();

    /**
     * Checks that a value is valid, checking what is allowed in the mode
     * flags.
     *
     * @param v  Value to check.
     *
     * @return @c true, if the value is valid. @c false otherwise.
     */
    bool isValid(Value const &v) const;

    /**
     * Verifies that a value is valid, checking against what is allowed in the
     * mode flags. If not, an exception is thrown.
     *
     * @param v  Value to test.
     */
    void verifyValid(Value const &v) const;

    /**
     * Verifies that the variable can be assigned a new value.
     *
     * @param attemptedNewValue  The new value that is being assigned.
     */
    void verifyWritable(Value const &attemptedNewValue);

    /**
     * Verifies that a string is a valid name for the variable. If not,
     * an exception is thrown.
     *
     * @param s  Name to test.
     */
    static void verifyName(String const &s);

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

public:
    /**
     * The variable is about to be deleted.
     *
     * @param variable  Variable.
     */
    DENG2_DEFINE_AUDIENCE2(Deletion, void variableBeingDeleted(Variable &variable))

    /**
     * The value of the variable has changed.
     *
     * @param variable  Variable.
     * @param newValue  New value of the variable.
     */
    DENG2_DEFINE_AUDIENCE2(Change, void variableValueChanged(Variable &variable, Value const &newValue))

    DENG2_DEFINE_AUDIENCE2(ChangeFrom, void variableValueChangedFrom(Variable &variable, Value const &oldValue,
                                                                     Value const &newValue))

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Variable::Flags)

} // namespace de

#endif /* LIBDENG2_VARIABLE_H */
