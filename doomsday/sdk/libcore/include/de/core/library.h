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

#ifndef LIBDENG2_LIBRARY_H
#define LIBDENG2_LIBRARY_H

#include "../libcore.h"
#include "../NativePath"

#include <QLibrary>
#include <QMap>

/**
 * Convenience macro for accessing symbols that have a type defined in de::Library
 * with the type name matching the symbol name.
 */
#define DENG2_SYMBOL(Name) symbol<de::Library::Name>(#Name)

namespace de {

class Audio;
class Map;
class Object;
class User;
class WorldSystem;

/**
 * The Library class allows loading shared library files
 * (DLL/so/bundle/dylib) and looks up exported symbols in the libraries.
 *
 * Library type identifiers;
 * - <code>library/generic</code>: A shared library with no special function.
 * - <code>deng-plugin/generic</code>: Generic plugin. Loaded always.
 * - <code>deng-plugin/game</code>: The game plugin. Only one of these can be loaded.
 * - <code>deng-plugin/audio</code>: Audio driver. Optional. Loaded on demand by
 *   the audio subsystem.
 *
 * @ingroup core
 */
class DENG2_PUBLIC Library
{
public:
    /// Loading the shared library failed. @ingroup errors
    DENG2_ERROR(LoadError);

    /// A symbol was not found. @ingroup errors
    DENG2_ERROR(SymbolMissingError);

    /// Default type identifier.
    static char const *DEFAULT_TYPE;

    // Common function profiles.
    /**
     * Queries the plugin for a type identifier string. If this function is not
     * defined, the identifier defaults to DEFAULT_TYPE.
     *
     * @return  Type identifier string.
     */
    typedef char const *(*deng_LibraryType)(void);

    /**
     * Passes Doomsda'ys public APIs to the library. Called automatically by
     * the engine when loading libraries.
     */
    typedef void (*deng_API)(int, void *);

    /**
     * Performs any one-time initialization necessary for the usage of the plugin.
     * If this symbol is exported from a shared library, it gets called automatically
     * when the library is loaded. Note that this is called before deng_API(), so it
     * should be used exclusively for setting up the plugin's internal state.
     */
    typedef void (*deng_InitializePlugin)(void);

    /**
     * Frees resources reserved by the plugin. If this symbol is exported from a
     * shared library, it gets called automatically when the library is unloaded.
     */
    typedef void (*deng_ShutdownPlugin)(void);

    /**
     * Constructs a new instance of an audio subsystem.
     *
     * @return  Audio subsystem.
     */
    typedef Audio *(*deng_NewAudio)(void);

    /**
     * Constructs a new game world.
     */
    typedef WorldSystem *(*deng_NewWorld)(void);

    /**
     * Constructs a new game map.
     */
    typedef Map *(*deng_NewMap)();

    /**
     * Constructs a new object.
     */
    typedef Object *(*deng_NewObject)(void);

    /**
     * Constructs a new user.
     */
    typedef User *(*deng_NewUser)(void);

    typedef dint (*deng_GetInteger)(dint id);
    typedef char const *(*deng_GetString)(dint id);
    typedef void *(*deng_GetAddress)(dint id);
    typedef void (*deng_Ticker)(ddouble tickLength);

public:
    /**
     * Constructs a new Library by loading a native shared library.
     *
     * @param nativePath  Path of the shared library to load.
     */
    Library(NativePath const &nativePath);

    /**
     * Unloads the shared library.
     */
    virtual ~Library();

    /**
     * Returns the type identifier of the library. This affects how libcore will treat
     * the library. The type is determined automatically when the library is first
     * loaded, and can then be queried at any time even after the library has been
     * unloaded.
     */
    String const &type() const;

    enum SymbolLookupMode {
        RequiredSymbol, ///< Symbol must be exported.
        OptionalSymbol  ///< Symbol can be missing.
    };

    /**
     * Gets the address of an exported symbol. Throws an exception if a required
     * symbol is not found.
     *
     * @param name    Name of the exported symbol.
     * @param lookup  Lookup mode (required or optional).
     *
     * @return  A pointer to the symbol. Returns @c NULL if an optional symbol is
     * not found.
     */
    void *address(String const &name, SymbolLookupMode lookup = RequiredSymbol);

    /**
     * Checks if the library exports a specific symbol.
     * @param name  Name of the exported symbol.
     * @return @c true if the symbol is exported, @c false if not.
     */
    bool hasSymbol(String const &name) const;

    /**
     * Gets the address of a symbol. Throws an exception if a required symbol
     * is not found.
     *
     * @param name    Name of the symbol.
     * @param lookup  Symbol lookup mode (required or optional).
     *
     * @return Pointer to the symbol as type @a Type.
     */
    template <typename Type>
    Type symbol(String const &name, SymbolLookupMode lookup = RequiredSymbol) {
        return function_cast<Type>(address(name, lookup));
    }

    /**
     * Utility template for acquiring pointers to symbols. Throws an exception
     * if a required symbol is not found.
     *
     * @param ptr     Pointer that will be set to point to the symbol's address.
     * @param name    Name of the symbol whose address to get.
     * @param lookup  Symbol lookup mode (required or optional).
     *
     * @return @c true, if the symbol was found. Otherwise @c false.
     */
    template <typename Type>
    bool setSymbolPtr(Type &ptr, String const &name, SymbolLookupMode lookup = RequiredSymbol) {
        ptr = symbol<Type>(name, lookup);
        return ptr != 0;
    }

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif /* LIBDENG2_LIBRARY_H */
