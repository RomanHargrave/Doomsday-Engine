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
 
#include "de/Module"
#include "de/File"
#include "de/Process"
#include "de/App"
#include "de/Folder"
#include "de/Script"

using namespace de;

Module::Module(String const &sourcePath) : _sourcePath(sourcePath), _process(0)
{
    // Load the script.
    initialize(Script(App::rootFolder().locate<File>(sourcePath)));
}

Module::Module(File const &sourceFile) : _sourcePath(sourceFile.path()), _process(0)
{
    initialize(Script(sourceFile));
}

void Module::initialize(Script const &script)
{
    // Execute the script.
    std::auto_ptr<Process> proc(new Process(script));
    proc->execute();
    _process = proc.release();
}

Module::~Module()
{
    delete _process;
}

Record &Module::names()
{
    return _process->context().names();
}
