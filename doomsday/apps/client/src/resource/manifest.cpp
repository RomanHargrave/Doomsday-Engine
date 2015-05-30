/** @file manifest.cpp  Game resource manifest.
 *
 * @authors Copyright © 2010-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2010-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "resource/manifest.h"
#include "dd_main.h"

#include <doomsday/filesys/fs_main.h>
#include <doomsday/filesys/lumpindex.h>
#include <doomsday/filesys/wad.h>
#include <doomsday/filesys/zip.h>

#include <de/App>
#include <de/NativeFile>
#include <de/Path>

using namespace de;

DENG2_PIMPL(ResourceManifest)
{
    resourceclassid_t classId;

    int flags;         ///< @ref fileFlags.
    QStringList names; ///< Known names in precedence order.

    /// Vector of resource identifier keys (e.g., file or lump names).
    /// Used for identification purposes.
    QStringList identityKeys;

    /// Index (in Manifest::Instance::names) of the name used to locate
    /// this resource if found. Set during resource location.
    int foundNameIndex;

    /// Fully resolved absolute path to the located resource if found.
    /// Set during resource location.
    String foundPath;

    Instance(Public *i, resourceclassid_t rclass, int rflags)
        : Base(i)
        , classId(rclass)
        , flags(rflags & ~FF_FOUND)
        , names()
        , identityKeys()
        , foundNameIndex(-1)
        , foundPath()
    {}
};

ResourceManifest::ResourceManifest(resourceclassid_t resClass, int fFlags, String *name)
    : d(new Instance(this, resClass, fFlags))
{
    if(name) addName(*name);
}

void ResourceManifest::addName(String newName)
{
    if(newName.isEmpty()) return;

    // Is this name unique? We don't want duplicates.
    if(!d->names.contains(newName, Qt::CaseInsensitive))
    {
        d->names.prepend(newName);
    }
}

void ResourceManifest::addIdentityKey(String newIdKey)
{
    if(newIdKey.isEmpty()) return;

    // Is this key unique? We don't want duplicates.
    if(!d->identityKeys.contains(newIdKey, Qt::CaseInsensitive))
    {
        d->identityKeys.append(newIdKey);
    }
}

enum lumpsizecondition_t
{
    LSCOND_NONE,
    LSCOND_EQUAL,
    LSCOND_GREATER_OR_EQUAL,
    LSCOND_LESS_OR_EQUAL
};

/**
 * Modifies the idKey so that the size condition is removed.
 */
static void checkSizeConditionInIdentityKey(String &idKey, lumpsizecondition_t *pCond, size_t *pSize)
{
    DENG_ASSERT(pCond != 0);
    DENG_ASSERT(pSize != 0);

    *pCond = LSCOND_NONE;
    *pSize = 0;

    int condPos = -1;
    int argPos  = -1;
    if((condPos = idKey.indexOf("==")) >= 0)
    {
        *pCond = LSCOND_EQUAL;
        argPos = condPos + 2;
    }
    else if((condPos = idKey.indexOf(">=")) >= 0)
    {
        *pCond = LSCOND_GREATER_OR_EQUAL;
        argPos = condPos + 2;
    }
    else if((condPos = idKey.indexOf("<=")) >= 0)
    {
        *pCond = LSCOND_LESS_OR_EQUAL;
        argPos = condPos + 2;
    }

    if(condPos < 0) return;

    // Get the argument.
    *pSize = idKey.mid(argPos).toULong();

    // Remove it from the name.
    idKey.truncate(condPos);
}

static lumpnum_t lumpNumForIdentityKey(LumpIndex const &lumpIndex, String idKey)
{
    if(idKey.isEmpty()) return -1;

    // The key may contain a size condition (==, >=, <=).
    lumpsizecondition_t sizeCond;
    size_t refSize;
    checkSizeConditionInIdentityKey(idKey, &sizeCond, &refSize);

    // We should now be left with just the name.
    String name = idKey;

    // Append a .lmp extension if none is specified.
    if(idKey.fileNameExtension().isEmpty())
    {
        name += ".lmp";
    }

    lumpnum_t lumpNum = lumpIndex.findLast(Path(name));
    if(lumpNum < 0) return -1;

    // Check the condition.
    size_t lumpSize = lumpIndex[lumpNum].info().size;
    switch(sizeCond)
    {
    case LSCOND_EQUAL:
        if(lumpSize != refSize) return -1;
        break;

    case LSCOND_GREATER_OR_EQUAL:
        if(lumpSize < refSize) return -1;
        break;

    case LSCOND_LESS_OR_EQUAL:
        if(lumpSize > refSize) return -1;
        break;

    default: break;
    }

    return lumpNum;
}

/// @return  @c true, iff the resource appears to be what we think it is.
static bool validateWad(String const &filePath, QStringList const &identityKeys)
{
    bool validated = true;
    try
    {
        FileHandle &hndl = App_FileSystem().openFile(filePath, "rb", 0/*baseOffset*/, true /*allow duplicates*/);

        if(Wad *wad = hndl.file().maybeAs<Wad>())
        {
            // Ensure all identity lumps are present.
            if(identityKeys.count())
            {
                if(wad->isEmpty())
                {
                    // Clear not what we are looking for.
                    validated = false;
                }
                else
                {
                    // Publish lumps to a temporary index.
                    LumpIndex lumpIndex;
                    for(int i = 0; i < wad->lumpCount(); ++i)
                    {
                        lumpIndex.catalogLump(wad->lump(i));
                    }

                    // Check each lump.
                    DENG2_FOR_EACH_CONST(QStringList, i, identityKeys)
                    {
                        if(lumpNumForIdentityKey(lumpIndex, *i) < 0)
                        {
                            validated = false;
                            break;
                        }
                    }
                }
            }
        }
        else
        {
            validated = false;
        }

        // We're done with the file.
        App_FileSystem().releaseFile(hndl.file());
        delete &hndl;
    }
    catch(FS1::NotFoundError const &)
    {} // Ignore this error.

    return validated;
}

/// @return  @c true, iff the resource appears to be what we think it is.
static bool validateZip(String const &filePath, QStringList const & /*identityKeys*/)
{
    try
    {
        FileHandle &hndl = App_FileSystem().openFile(filePath, "rbf");
        bool result = Zip::recognise(hndl);
        /// @todo Check files. We should implement an auxiliary zip lump index...
        App_FileSystem().releaseFile(hndl.file());
        delete &hndl;
        return result;
    }
    catch(FS1::NotFoundError const &)
    {} // Ignore error.
    return false;
}

void ResourceManifest::locateFile()
{
    // Already found?
    if(d->flags & FF_FOUND) return;

    // Perform the search.
    int nameIndex = 0;
    for(QStringList::const_iterator i = d->names.constBegin(); i != d->names.constEnd(); ++i, ++nameIndex)
    {
        StringList candidates;
        
        // Attempt to resolve a path to the named resource using FS1.
        try
        {
            String foundPath = App_FileSystem().findPath(de::Uri(*i, d->classId),
                                                         RLF_DEFAULT, App_ResourceClass(d->classId));
            foundPath = App_BasePath() / foundPath; // Ensure the path is absolute.
            candidates << foundPath;
        }
        catch(FS1::NotFoundError const &)
        {} // Ignore this error.

        // Also check what FS2 has to offer. FS1 can't access FS2's files, so we'll
        // restrict this to native files.
        App::fileSystem().forAll(*i, [&candidates] (File &f)
        {
            // We ignore interpretations and go straight to the source.
            if(NativeFile const *native = f.source()->maybeAs<NativeFile>())
            {
                candidates << native->nativePath();
            }
            return LoopContinue;
        });
        
        for(String foundPath : candidates)
        {
            // Perform identity validation.
            bool validated = false;
            if(d->classId == RC_PACKAGE)
            {
                /// @todo The identity configuration should declare the type of resource...
                validated = validateWad(foundPath, d->identityKeys);
                if(!validated)
                    validated = validateZip(foundPath, d->identityKeys);
            }
            else
            {
                // Other resource types are not validated.
                validated = true;
            }

            if(validated)
            {
                // This is the resource we've been looking for.
                d->flags |= FF_FOUND;
                d->foundPath = foundPath;
                d->foundNameIndex = nameIndex;
                return;
            }
        }
    }
}

void ResourceManifest::forgetFile()
{
    if(d->flags & FF_FOUND)
    {
        d->foundPath.clear();
        d->foundNameIndex = -1;
        d->flags &= ~FF_FOUND;
    }
}

String const &ResourceManifest::resolvedPath(bool tryLocate)
{
    if(tryLocate)
    {
        locateFile();
    }
    return d->foundPath;
}

resourceclassid_t ResourceManifest::resourceClass() const
{
    return d->classId;
}

int ResourceManifest::fileFlags() const
{
    return d->flags;
}

QStringList const &ResourceManifest::identityKeys() const
{
    return d->identityKeys;
}

QStringList const &ResourceManifest::names() const
{
    return d->names;
}
