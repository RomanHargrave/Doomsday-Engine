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

#ifndef LIBDENG2_SCRIPT_H
#define LIBDENG2_SCRIPT_H

/**
 * @defgroup script Script Engine
 *
 * The script engine parses and executes scripts.
 */

#include "../Compound"
#include "../String"

#include <vector>

namespace de {

class File;

/**
 * Contains statements and expressions which are ready to be executed.
 * A Script instance is built from source code text, which is parsed and
 * converted to statements and expressions.
 *
 * @see Process (used to execute scripts)
 *
 * @ingroup script
 */
class DENG2_PUBLIC Script
{
public:
    /// Constructs an empty script with no statements.
    Script();

    /**
     * Parses the source into statements.
     *
     * @param source  Script source.
     */
    Script(String const &source);

    /**
     * Parses the source file info statements. The path of the source file
     * is saved and used when importing modules.
     *
     * @param file  Source file.
     */
    Script(File const &file);

    /**
     * Parses a source into statements, replacing any statements currently
     * in the Script. The user must ensure that the script is not currently
     * being executed by a Process.
     *
     * @param source  Script source.
     */
    void parse(String const &source);

    /**
     * Sets the path of the source. Used as the value of __file__ in the
     * executing process's global namespace.
     *
     * @param path  Path.
     */
    void setPath(String const &path);

    String const &path() const;

    /// Returns the statement that begins the script. This is where
    /// a process begins the execution of a script.
    Statement const *firstStatement() const;

    /// Returns a modifiable reference to the main statement compound
    /// of the script.
    Compound &compound();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif
