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

#include "de/LibraryFile"
#include "de/NativeFile"
#include "de/Library"

#include <QLibrary>

using namespace de;

LibraryFile::LibraryFile(File *source)
    : File(source->name()), _library(0)
{
    DENG2_ASSERT(source != 0);
    setSource(source); // takes ownership
}

LibraryFile::~LibraryFile()
{
    DENG2_FOR_AUDIENCE2(Deletion, i) i->fileBeingDeleted(*this);
    audienceForDeletion().clear();
    
    deindex();    
    delete _library;
}

String LibraryFile::describe() const
{
    String desc = "shared library";
    if(loaded()) desc += " [" + library().type() + "]";
    return desc;
}

Library &LibraryFile::library()
{
    if(_library)
    {
        return *_library;
    }
    
    /// @todo A method for File for making a NativeFile out of any File.
    NativeFile *native = source()->maybeAs<NativeFile>();
    if(!native)
    {
        /// @throw UnsupportedSourceError Currently shared libraries are only loaded directly
        /// from native files. Other kinds of files would require a temporary native file.
        throw UnsupportedSourceError("LibraryFile::library", source()->description() +
            ": can only load from NativeFile");
    }
    
    _library = new Library(native->nativePath());
    return *_library;
}

Library const &LibraryFile::library() const
{
    if(_library) return *_library;

    /// @throw NotLoadedError Library is presently not loaded.
    throw NotLoadedError("LibraryFile::library", "Library is not loaded: " + description());
}

void LibraryFile::clear()
{
    if(_library)
    {
        delete _library;
        _library = 0;
    }
}

bool LibraryFile::hasUnderscoreName(String const &nameAfterUnderscore) const
{
    return name().contains("_" + nameAfterUnderscore + ".") ||
           name().endsWith("_" + nameAfterUnderscore);
}

bool LibraryFile::recognize(File const &file)
{
#ifdef MACOSX
    // On Mac OS X, plugins are in the .bundle format. The LibraryFile will point
    // to the actual binary inside the bundle. Libraries must be loaded from
    // native files.
    if(NativeFile const *native = file.maybeAs<NativeFile>())
    {
        // Check if this in the executable folder with a matching bundle name.
        if(native->nativePath().fileNamePath().toString().endsWith(
                    file.name() + ".bundle/Contents/MacOS"))
        {
            // (name).bundle/Contents/MacOS/(name)
            return true;
        }
    }
#else
    // Check the extension first.
    if(QLibrary::isLibrary(file.name()))
    {
#if defined(UNIX)
        // Only actual .so files should be considered.
        if(!file.name().endsWith(".so")) // just checks the file name
        {
            return false;
        }
#endif
        // Looks like a library.
        return true;
    }
#endif
    return false;
}
