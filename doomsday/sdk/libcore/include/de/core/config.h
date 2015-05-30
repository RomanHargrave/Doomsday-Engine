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
 
#ifndef LIBDENG2_CONFIG_H
#define LIBDENG2_CONFIG_H

#include "../Process"
#include "../String"
#include "../Path"
#include "../Version"
#include "../Record"

namespace de {

class ArrayValue;

/**
 * Stores the configuration of everything.
 *
 * The application owns a Config. The default configuration is produced by executing the
 * .de scripts in the config directories. The resulting namespace is serialized for
 * storage, and is restored from the serialized version directly before the config
 * scripts are run.
 *
 * The version of the engine is stored in the serialized config namespace. This is for
 * actions needed when upgrading: the config script can check the previous version and
 * apply changes accordingly.
 *
 * In practice, Config is a specialized script namespace stored in a Record. It gets
 * written to the application's persistent data store (persist.pack) using a Refuge. The
 * Config is automatically written persistently before being destroyed.
 *
 * @ingroup core
 */
class DENG2_PUBLIC Config : public RecordAccessor
{
public:
    /**
     * Constructs a new configuration.
     *
     * @param path  Name of the configuration file to read.
     */
    Config(Path const &path);

    /// Read configuration from files.
    void read();

    /// Writes the configuration to /home.
    void write() const;

    /**
     * Sets the value of a variable, creating the variable if needed.
     *
     * @param name   Name of the variable. May contain subrecords using the dot notation.
     * @param value  Value for the variable.
     *
     * @return Variable whose value was set.
     */
    Variable &set(String const &name, bool value);

    /// @copydoc set()
    Variable &set(String const &name, Value::Text const &value);

    /// @copydoc set()
    Variable &set(String const &name, Value::Number const &value);

    /// @copydoc set()
    Variable &set(String const &name, dint value);

    /// @copydoc set()
    Variable &set(String const &name, duint value);

    /**
     * Sets the value of a variable, creating the variable if it doesn't exist.
     *
     * @param name   Name of the variable. May contain subrecords using the dot notation.
     * @param value  Array to use as the value of the variable. Ownership taken.
     */
    Variable &set(String const &name, ArrayValue *value);

    /**
     * Returns the configuration namespace.
     */
    Record &names();

    /**
     * Returns the configuration namespace (immutable).
     */
    Record const &names() const;

    /**
     * Looks up a variable in the Config. Variables in subrecords can be accessed
     * using the member notation: <code>subrecord-name.variable-name</code>
     *
     * If the variable does not exist, a Record::NotFoundError is thrown.
     *
     * @param name  Variable name.
     *
     * @return  Variable.
     */
    Variable &operator [] (String const &name);

    /**
     * Looks up a variable in the record. Variables in subrecords can be accessed
     * using the member notation: <code>subrecord-name.variable-name</code>
     *
     * If the variable does not exist, an Record::NotFoundError is thrown.
     *
     * @param name  Variable name.
     *
     * @return  Variable (non-modifiable).
     */
    Variable const &operator [] (String const &name) const;

    /**
     * Returns the old version, when a new installed version has been detected.
     * If no upgrade has occurred, returns the current version.
     */
    Version upgradedFromVersion() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif /* LIBDENG2_CONFIG_H */
