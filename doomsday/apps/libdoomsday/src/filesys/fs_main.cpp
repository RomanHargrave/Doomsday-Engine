/** @file fs_main.cpp 
 * @ingroup fs
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @authors Copyright &copy; 1993-1996 by id Software, Inc.
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

#include "doomsday/filesys/fs_main.h"
#include "doomsday/filesys/fs_util.h"
#include "doomsday/console/exec.h"
#include "doomsday/console/cmd.h"
#include "doomsday/filesys/file.h"
#include "doomsday/filesys/fileid.h"
#include "doomsday/filesys/fileinfo.h"
#include "doomsday/filesys/lumpindex.h"
#include "doomsday/filesys/wad.h"
#include "doomsday/filesys/zip.h"

#include <ctime>
#include <QDir>
#include <QList>
#include <QtAlgorithms>
#include <de/App>
#include <de/Log>
#include <de/NativePath>
#include <de/LogBuffer>
#include <de/memory.h>
#include <de/findfile.h>

using namespace de;

extern uint F_GetLastModified(char const *path);

static FS1 *fileSystem;

typedef QList<FileId> FileIds;

/**
 * Virtual (file) path => Lump name mapping.
 *
 * @todo We can't presently use a Map or Hash for these. Although the paths are
 *       unique, several of the existing algorithms which match using patterns
 *       assume they are sorted in a quasi load ordering.
 */
typedef QPair<QString, QString> LumpMapping;
typedef QList<LumpMapping> LumpMappings;

/**
 * Virtual file-directory mapping.
 * Maps one (absolute) path in the virtual file system to another.
 *
 * @todo We can't presently use a Map or Hash for these. Although the paths are
 *       unique, several of the existing algorithms which match using patterns
 *       assume they are sorted in a quasi load ordering.
 */
typedef QPair<QString, QString> PathMapping;
typedef QList<PathMapping> PathMappings;

/**
 * @note Performance is O(n).
 * @return @c iterator pointing to list->end() if not found.
 */
static FS1::FileList::iterator findListFile(FS1::FileList &list, File1 &file)
{
    if(list.empty()) return list.end();
    // Perform the search.
    FS1::FileList::iterator i;
    for(i = list.begin(); i != list.end(); ++i)
    {
        if(&file == &(*i)->file())
        {
            break; // This is the node we are looking for.
        }
    }
    return i;
}

/**
 * @note Performance is O(n).
 * @return @c iterator pointing to list->end() if not found.
 */
static FS1::FileList::iterator findListFileByPath(FS1::FileList &list, String path)
{
    if(list.empty()) return list.end();
    if(path.isEmpty()) return list.end();

    // Perform the search.
    FS1::FileList::iterator i;
    for(i = list.begin(); i != list.end(); ++i)
    {
        File1 &file = (*i)->file();
        if(!file.composePath().compare(path, Qt::CaseInsensitive))
        {
            break; // This is the node we are looking for.
        }
    }
    return i;
}

static bool applyPathMapping(ddstring_t *path, PathMapping const &vdm);

/**
 * Performs a case-insensitive pattern match. The pattern can contain
 * wildcards.
 *
 * @param filePath  Path to match.
 * @param pattern   Pattern with * and ? as wildcards.
 *
 * @return  @c true, if @a filePath matches the pattern.
 */
static bool matchFileName(String const &string, String const &pattern)
{
    static QChar const ASTERISK('*');
    static QChar const QUESTION_MARK('?');

    QChar const *in = string.constData(), *st = pattern.constData();

    while(!in->isNull())
    {
        if(*st == ASTERISK)
        {
            st++;
            continue;
        }

        if(*st != QUESTION_MARK && st->toLower() != in->toLower())
        {
            // A mismatch. Hmm. Go back to a previous '*'.
            while(st >= pattern && *st != ASTERISK) { st--; }

            if(st < pattern)
                return false; // No match!
            // The asterisk lets us continue.
        }

        // This character of the pattern is OK.
        st++;
        in++;
    }

    // Match is good if the end of the pattern was reached.

    // Skip remaining asterisks.
    while(*st == ASTERISK) { st++; }

    return st->isNull();
}

DENG2_PIMPL(FS1)
{
    bool loadingForStartup;     ///< @c true= Flag newly opened files as "startup".

    FileList openFiles;         ///< List of currently opened files.
    FileList loadedFiles;       ///< List of all loaded files present in the system.
    uint loadedFilesCRC;
    FileIds fileIds;            ///< Database of unique identifiers for all loaded/opened files.

    LumpIndex primaryIndex;     ///< Primary index of all files in the system.
    LumpIndex zipFileIndex;     ///< Type-specific index for ZipFiles.

    LumpMappings lumpMappings;  ///< Virtual (file) path => Lump name mapping.
    PathMappings pathMappings;  ///< Virtual file-directory mapping.

    Schemes schemes;            ///< File subsets.

    Instance(Public *i)
        : Base(i)
        , loadingForStartup(true)
        , loadedFilesCRC   (0)
        , zipFileIndex     (true/*paths are unique*/)
    {}

    ~Instance()
    {
        clearLoadedFiles();
        clearOpenFiles();
        clearIndexes();

        fileIds.clear(); // Should be NOP, if bookkeeping is correct.

        pathMappings.clear();
        lumpMappings.clear();

        clearAllSchemes();
    }

    void clearAllSchemes()
    {
        DENG2_FOR_EACH(Schemes, i, schemes)
        {
            delete *i;
        }
        schemes.clear();
    }

    /// @return  @c true if the FileId associated with @a path was released.
    bool releaseFileId(String path)
    {
        if(!path.isEmpty())
        {
            FileId fileId = FileId::fromPath(path);
            FileIds::iterator place = qLowerBound(fileIds.begin(), fileIds.end(), fileId);
            if(place != fileIds.end() && *place == fileId)
            {
                LOGDEV_RES_XVERBOSE_DEBUGONLY("Released FileId %s - \"%s\"", *place << fileId.path());
                fileIds.erase(place);
                return true;
            }
        }
        return false;
    }

    void clearLoadedFiles(LumpIndex *index = 0)
    {
        loadedFilesCRC = 0;

        // Unload in reverse load order.
        for(int i = loadedFiles.size() - 1; i >= 0; i--)
        {
            File1 &file = loadedFiles[i]->file();
            if(!index || index->catalogues(file))
            {
                self.deindex(file);
                delete &file;
            }
        }
    }

    void clearOpenFiles()
    {
        while(!openFiles.isEmpty()){ delete openFiles.takeLast(); }
    }

    void clearIndexes()
    {
        primaryIndex.clear();
        zipFileIndex.clear();
    }

    String findPath(de::Uri const &search)
    {
        // Within a subspace scheme?
        try
        {
            Scheme &scheme = self.scheme(search.scheme());
            LOG_RES_XVERBOSE("Using scheme '%s'...") << scheme.name();

            // Ensure the scheme's index is up to date.
            scheme.rebuild();

            // The in-scheme name is the file name sans extension.
            String name = search.path().lastSegment().toString().fileNameWithoutExtension();

            // Perform the search.
            Scheme::FoundNodes foundNodes;
            if(scheme.findAll(name, foundNodes))
            {
                // At least one node name was matched (perhaps partially).
                DENG2_FOR_EACH_CONST(Scheme::FoundNodes, i, foundNodes)
                {
                    PathTree::Node &node = **i;
                    if(!node.comparePath(search.path(), PathTree::NoBranch))
                    {
                        // This is the file we are looking for.
                        return node.path();
                    }
                }
            }

            /// @todo Should return not-found here but some searches are still dependent
            ///       on falling back to a wider search. -ds
        }
        catch(UnknownSchemeError const &)
        {} // Ignore this error.

        // Try a wider search of the whole virtual file system.
        QScopedPointer<File1> file(openFile(search.path(), "rb", 0, true /* allow duplicates */));
        if(!file.isNull())
        {
            return file->composePath();
        }

        return ""; // Not found.
    }

    File1 *findLump(String path, String const & /*mode*/)
    {
        if(path.isEmpty()) return 0;

        // We must have an absolute path - prepend the base path if necessary.
        if(QDir::isRelativePath(path))
        {
            path = App_BasePath() / path;
        }

        // First check the Zip lump index.
        lumpnum_t lumpNum = zipFileIndex.findLast(path);
        if(lumpNum >= 0)
        {
            return &zipFileIndex[lumpNum];
        }

        // Nope. Any applicable dir/WAD redirects?
        if(!lumpMappings.empty())
        {
            DENG2_FOR_EACH_CONST(LumpMappings, i, lumpMappings)
            {
                LumpMapping const &mapping = *i;
                if(mapping.first.compare(path)) continue;

                lumpnum_t lumpNum = self.lumpNumForName(mapping.second);
                if(lumpNum < 0) continue;

                return &self.lump(lumpNum);
            }
        }

        return 0;
    }

    FILE *findAndOpenNativeFile(String path, String const &mymode, String &foundPath)
    {
        DENG2_ASSERT(!path.isEmpty());

        // We must have an absolute path - prepend the CWD if necessary.
        path = NativePath::workPath().withSeparators('/') / path;

        // Translate mymode to the C-lib's fopen() mode specifiers.
        char mode[8] = "";
        if(mymode.contains('r'))      strcat(mode, "r");
        else if(mymode.contains('w')) strcat(mode, "w");
        if(mymode.contains('b'))      strcat(mode, "b");
        else if(mymode.contains('t')) strcat(mode, "t");

        // First try a real native file at this absolute path.
        NativePath nativePath = NativePath(path);
        FILE *nativeFile = fopen(nativePath.toUtf8().constData(), mode);
        if(nativeFile)
        {
            foundPath = nativePath.expand().withSeparators('/');
            return nativeFile;
        }

        // Nope. Any applicable virtual directory mappings?
        if(!pathMappings.empty())
        {
            QByteArray pathUtf8 = path.toUtf8();
            AutoStr *mapped = AutoStr_NewStd();
            DENG2_FOR_EACH_CONST(PathMappings, i, pathMappings)
            {
                Str_Set(mapped, pathUtf8.constData());
                if(!applyPathMapping(mapped, *i)) continue;
                // The mapping was successful.

                nativePath = NativePath(Str_Text(mapped));
                nativeFile = fopen(nativePath.toUtf8().constData(), mode);
                if(nativeFile)
                {
                    foundPath = nativePath.expand().withSeparators('/');
                    return nativeFile;
                }
            }
        }

        return 0;
    }

    File1 *openFile(String path, String const& mode, size_t baseOffset,
                    bool allowDuplicate)
    {
        if(path.isEmpty()) return 0;

        LOG_AS("FS1::openFile");

        // We must have an absolute path.
        path = App_BasePath() / path;

        LOG_RES_XVERBOSE("Trying \"%s\"...") << NativePath(path).pretty();

        bool const reqNativeFile = mode.contains('f');

        FileHandle *hndl = 0;
        FileInfo info; // The temporary info descriptor.

        // First check for lumps?
        if(!reqNativeFile)
        {
            if(File1 *found = findLump(path, mode))
            {
                // Do not read files twice.
                if(!allowDuplicate && !self.checkFileId(found->composeUri())) return 0;

                // Get a handle to the lump we intend to open.
                /// @todo The way this buffering works is nonsensical it should not be done here
                ///        but should instead be deferred until the content of the lump is read.
                hndl = FileHandle::fromLump(*found);

                // Prepare a temporary info descriptor.
                info = found->info();
            }
        }

        // Not found? - try a native file.
        if(!hndl)
        {
            String foundPath;
            if(FILE *found = findAndOpenNativeFile(path, mode, foundPath))
            {
                // Do not read files twice.
                if(!allowDuplicate && !self.checkFileId(de::Uri(foundPath, RC_NULL)))
                {
                    fclose(found);
                    return 0;
                }

                // Acquire a handle on the file we intend to open.
                hndl = FileHandle::fromNativeFile(*found, baseOffset);

                // Prepare the temporary info descriptor.
                info = FileInfo(F_GetLastModified(foundPath.toUtf8().constData()));
            }
        }

        // Nothing?
        if(!hndl) return 0;

        // Search path is used here rather than found path as the latter may have
        // been mapped to another location. We want the file to be attributed with
        // the path it is to be known by throughout the virtual file system.

        File1 &file = self.interpret(*hndl, path, info);

        if(loadingForStartup)
        {
            file.setStartup(true);
        }

        return &file;
    }
};

FS1::FS1() : d(new Instance(this))
{}

FS1::Scheme &FS1::createScheme(String name, Scheme::Flags flags)
{
    DENG2_ASSERT(name.length() >= Scheme::min_name_length);

    // Ensure this is a unique name.
    if(knownScheme(name)) return scheme(name);

    // Create a new scheme.
    Scheme *newScheme = new Scheme(name, flags);
    d->schemes.insert(name.toLower(), newScheme);
    return *newScheme;
}

void FS1::index(File1 &file)
{
#ifdef DENG_DEBUG
    // Ensure this hasn't yet been indexed.
    FileList::const_iterator found = findListFile(d->loadedFiles, file);
    if(found != d->loadedFiles.end())
        throw Error("FS1::index", "File \"" + NativePath(file.composePath()).pretty() + "\" has already been indexed");
#endif

    // Publish lumps to one or more indexes?
    if(Zip *zip = file.maybeAs<Zip>())
    {
        if(!zip->isEmpty())
        {
            // Insert the lumps into their rightful places in the index.
            for(int i = 0; i < zip->lumpCount(); ++i)
            {
                File1 &lump = zip->lump(i);

                d->primaryIndex.catalogLump(lump);

                // Zip files go into a special ZipFile index as well.
                d->zipFileIndex.catalogLump(lump);
            }
        }
    }
    else if(Wad *wad = file.maybeAs<Wad>())
    {
        if(!wad->isEmpty())
        {
            // Insert the lumps into their rightful places in the index.
            for(int i = 0; i < wad->lumpCount(); ++i)
            {
                d->primaryIndex.catalogLump(wad->lump(i));
            }
        }
    }

    // Add a handle to the loaded files list.
    FileHandle *hndl = FileHandle::fromFile(file);
    d->loadedFiles.push_back(hndl); hndl->setList(reinterpret_cast<de::FileList *>(&d->loadedFiles));
    d->loadedFilesCRC = 0;
}

void FS1::deindex(File1 &file)
{
    FileList::iterator found = findListFile(d->loadedFiles, file);
    if(found == d->loadedFiles.end()) return; // Most peculiar..

    d->releaseFileId(file.composePath());

    d->zipFileIndex.pruneByFile(file);
    d->primaryIndex.pruneByFile(file);

    d->loadedFiles.erase(found);
    d->loadedFilesCRC = 0;
    delete *found;
}

File1 &FS1::find(de::Uri const &search)
{
    LOG_AS("FS1::find");
    if(!search.isEmpty())
    {
        try
        {
            String searchPath = search.resolved();

            // Convert to an absolute path.
            if(!QDir::isAbsolutePath(searchPath))
            {
                searchPath = App_BasePath() / searchPath;
            }

            FileList::iterator found = findListFileByPath(d->loadedFiles, searchPath);
            if(found != d->loadedFiles.end())
            {
                DENG_ASSERT((*found)->hasFile());
                return (*found)->file();
            }
        }
        catch(de::Uri::ResolveError const &er)
        {
            // Log but otherwise ignore unresolved paths.
            LOGDEV_RES_VERBOSE(er.asText());
        }
    }

    /// @throw NotFoundError  No files found matching the search term.
    throw NotFoundError("FS1::find", "No files found matching '" + search.compose() + "'");
}

String FS1::findPath(de::Uri const &search, int flags, ResourceClass &rclass)
{
    LOG_AS("FS1::findPath");
    if(!search.isEmpty())
    {
        try
        {
            String searchPath = search.resolved();

            // If an extension was specified, first look for files of the same type.
            String ext = searchPath.fileNameExtension();
            if(!ext.isEmpty() && ext.compare(".*"))
            {
                String found = d->findPath(de::Uri(search.scheme(), searchPath));
                if(!found.isEmpty()) return found;

                // If we are looking for a particular file type, get out of here.
                if(flags & RLF_MATCH_EXTENSION) return "";
            }

            if(isNullResourceClass(rclass) || !rclass.fileTypeCount()) return "";

            /*
             * Try each expected file type name extension for this resource class.
             */
            String searchPathWithoutFileNameExtension = searchPath.fileNamePath() / searchPath.fileNameWithoutExtension();

            DENG2_FOR_EACH_CONST(ResourceClass::FileTypes, typeIt, rclass.fileTypes())
            {
                DENG2_FOR_EACH_CONST(QStringList, i, (*typeIt)->knownFileNameExtensions())
                {
                    String const &ext = *i;
                    String found = d->findPath(de::Uri(search.scheme(), searchPathWithoutFileNameExtension + ext));
                    if(!found.isEmpty()) return found;
                }
            };
        }
        catch(de::Uri::ResolveError const &er)
        {
            // Log but otherwise ignore unresolved paths.
            LOGDEV_RES_VERBOSE(er.asText());
        }
    }

    /// @throw NotFoundError  No files found matching the search term.
    throw NotFoundError("FS1::findPath", "No paths found matching '" + search.compose() + "'");
}

String FS1::findPath(de::Uri const &search, int flags)
{
    return findPath(search, flags, ResourceClass::classForId(RC_NULL));
}

#ifdef DENG_DEBUG
static void printFileIds(FileIds const &fileIds)
{
    uint idx = 0;
    DENG2_FOR_EACH_CONST(FileIds, i, fileIds)
    {
        LOGDEV_RES_MSG("  %u - %s : \"%s\"") << idx << *i << i->path();
        ++idx;
    }
}
#endif

#ifdef DENG_DEBUG
static void printFileList(FS1::FileList &list)
{
    uint idx = 0;
    DENG2_FOR_EACH_CONST(FS1::FileList, i, list)
    {
        FileHandle &hndl = **i;
        File1 &file   = hndl.file();
        FileId fileId = FileId::fromPath(file.composePath());

        LOGDEV_RES_VERBOSE(" %c%d: %s - \"%s\" (handle: %p)")
                << (file.hasStartup()? '*' : ' ') << idx
                << fileId << fileId.path() << &hndl;
        ++idx;
    }
}
#endif

int FS1::unloadAllNonStartupFiles()
{
#ifdef DENG_DEBUG
    // List all open files with their identifiers.
    if(LogBuffer::get().isEnabled(LogEntry::Generic | LogEntry::Verbose))
    {
        LOGDEV_RES_VERBOSE("Open files at reset:");
        printFileList(d->openFiles);
        LOGDEV_RES_VERBOSE("End\n");
    }
#endif

    // Perform non-startup file unloading (in reverse load order).
    int numUnloadedFiles = 0;
    for(int i = d->loadedFiles.size() - 1; i >= 0; i--)
    {
        File1 &file = d->loadedFiles[i]->file();
        if(file.hasStartup()) continue;

        deindex(file);
        delete &file;
        numUnloadedFiles += 1;
    }

#ifdef DENG_DEBUG
    // Sanity check: look for orphaned identifiers.
    if(!d->fileIds.empty())
    {
        LOGDEV_RES_MSG("Orphan FileIds:");
        printFileIds(d->fileIds);
    }
#endif

    return numUnloadedFiles;
}

bool FS1::checkFileId(de::Uri const &path)
{
    if(!accessFile(path)) return false;

    // Calculate the identifier.
    FileId fileId = FileId::fromPath(path.compose());
    FileIds::iterator place = qLowerBound(d->fileIds.begin(), d->fileIds.end(), fileId);
    if(place != d->fileIds.end() && *place == fileId) return false;

    LOGDEV_RES_XVERBOSE_DEBUGONLY("checkFileId \"%s\" => %s", fileId.path() << fileId); /* path() is debug-only */

    d->fileIds.insert(place, fileId);
    return true;
}

void FS1::resetFileIds()
{
    d->fileIds.clear();
}

void FS1::endStartup()
{
    d->loadingForStartup = false;
}

LumpIndex const &FS1::nameIndex() const
{
    return d->primaryIndex;
}

lumpnum_t FS1::lumpNumForName(String name)
{
    LOG_AS("FS1::lumpNumForName");

    if(name.isEmpty()) return -1;

    // Append a .lmp extension if none is specified.
    if(name.fileNameExtension().isEmpty())
    {
        name += ".lmp";
    }

    // Perform the search.
    return d->primaryIndex.findLast(Path(name));
}

void FS1::releaseFile(File1 &file)
{
    for(int i = d->openFiles.size() - 1; i >= 0; i--)
    {
        FileHandle &hndl = *(d->openFiles[i]);
        if(&hndl.file() == &file)
        {
            d->openFiles.removeAt(i);
        }
    }
}

/// @return @c NULL= Not found.
static Wad *findFirstWadFile(FS1::FileList &list, bool custom)
{
    if(list.empty()) return 0;
    DENG2_FOR_EACH(FS1::FileList, i, list)
    {
        File1 &file = (*i)->file();
        if(custom != file.hasCustom()) continue;

        if(Wad *wad = file.maybeAs<Wad>())
        {
            return wad;
        }
    }
    return 0;
}

uint FS1::loadedFilesCRC()
{
    if(!d->loadedFilesCRC)
    {
        /**
         * We define the CRC as that of the lump directory of the first loaded IWAD.
         * @todo Really kludgy...
         */
        // CRC not calculated yet, let's do it now.
        Wad *iwad = findFirstWadFile(d->loadedFiles, false/*not-custom*/);
        if(!iwad) return 0;
        d->loadedFilesCRC = iwad->calculateCRC();
    }
    return d->loadedFilesCRC;
}

FS1::FileList const &FS1::loadedFiles() const
{
    return d->loadedFiles;
}

int FS1::findAll(bool (*predicate)(File1 &file, void *parameters), void *parameters,
    FS1::FileList &found) const
{
    int numFound = 0;
    DENG2_FOR_EACH_CONST(FS1::FileList, i, d->loadedFiles)
    {
        // Interested in this file?
        if(predicate && !predicate((*i)->file(), parameters)) continue; // Nope.

        found.push_back(*i);
        numFound += 1;
    }
    return numFound;
}

int FS1::findAllPaths(Path searchPattern, int flags, FS1::PathList &found)
{
    int const numFoundSoFar = found.count();

    // We must have an absolute path - prepend the base path if necessary.
    if(!QDir::isAbsolutePath(searchPattern))
    {
        searchPattern = App_BasePath() / searchPattern;
    }

    /*
     * Check the Zip directory.
     */
    DENG2_FOR_EACH_CONST(LumpIndex::Lumps, i, d->zipFileIndex.allLumps())
    {
        File1 const &lump = **i;
        PathTree::Node const &node = lump.directoryNode();

        String filePath;
        bool patternMatched;
        if(!(flags & SearchPath::NoDescend))
        {
            filePath = lump.composePath();
            patternMatched = matchFileName(filePath, searchPattern);
        }
        else
        {
            patternMatched = !node.comparePath(searchPattern, PathTree::MatchFull);
        }

        if(!patternMatched) continue;

        // Not yet composed the path?
        if(filePath.isEmpty())
        {
            filePath = lump.composePath();
        }

        found.push_back(PathListItem(filePath, !node.isLeaf()? A_SUBDIR : 0));
    }

    /*
     * Check the dir/WAD direcs.
     */
    if(!d->lumpMappings.empty())
    {
        DENG2_FOR_EACH_CONST(LumpMappings, i, d->lumpMappings)
        {
            if(!matchFileName(i->first, searchPattern)) continue;

            found.push_back(PathListItem(i->first, 0 /*only filepaths (i.e., leaves) can be mapped to lumps*/));
        }

        /// @todo Shouldn't these be sorted? -ds
    }

    /*
     * Check native paths.
     */
    String searchDirectory = searchPattern.toString().fileNamePath();
    if(!searchDirectory.isEmpty())
    {
        QByteArray searchDirectoryUtf8 = searchDirectory.toUtf8();
        PathList nativeFilePaths;
        AutoStr *wildPath = AutoStr_NewStd();
        Str_Reserve(wildPath, searchDirectory.length() + 2 + 16); // Conservative estimate.

        for(int i = -1; i < (int)d->pathMappings.count(); ++i)
        {
            Str_Clear(wildPath);
            Str_Appendf(wildPath, "%s/", searchDirectoryUtf8.constData());

            if(i > -1)
            {
                // Possible mapping?
                if(!applyPathMapping(wildPath, d->pathMappings[i])) continue;
            }
            Str_AppendChar(wildPath, '*');

            FindData fd;
            if(!FindFile_FindFirst(&fd, Str_Text(wildPath)))
            {
                // First path found.
                do
                {
                    // Ignore relative directory symbolics.
                    if(Str_Compare(&fd.name, ".") && Str_Compare(&fd.name, ".."))
                    {
                        String foundPath = searchDirectory / NativePath(Str_Text(&fd.name)).withSeparators('/');
                        if(!matchFileName(foundPath, searchPattern)) continue;

                        nativeFilePaths.push_back(PathListItem(foundPath, fd.attrib));
                    }
                } while(!FindFile_FindNext(&fd));
            }
            FindFile_Finish(&fd);
        }

        // Sort the native file paths.
        qSort(nativeFilePaths.begin(), nativeFilePaths.end());

        // Add the native file paths to the found results.
        found.append(nativeFilePaths);
    }

    return found.count() - numFoundSoFar;
}

File1 &FS1::interpret(FileHandle &hndl, String filePath, FileInfo const &info)
{
    DENG2_ASSERT(!filePath.isEmpty());

    File1 *interpretedFile = 0;

    // Firstly try the interpreter for the guessed resource types.
    FileType const &ftypeGuess = DD_GuessFileTypeFromFileName(filePath);
    if(NativeFileType const* fileType = dynamic_cast<NativeFileType const *>(&ftypeGuess))
    {
        interpretedFile = fileType->interpret(hndl, filePath, info);
    }

    // If not yet interpreted - try each recognisable format in order.
    if(!interpretedFile)
    {
        FileTypes const &fileTypes = DD_FileTypes();
        DENG2_FOR_EACH_CONST(FileTypes, i, fileTypes)
        {
            if(NativeFileType const *fileType = dynamic_cast<NativeFileType const *>(*i))
            {
                // Already tried this?
                if(fileType == &ftypeGuess) continue;

                interpretedFile = fileType->interpret(hndl, filePath, info);
                if(interpretedFile) break;
            }
        }
    }

    // Still not interpreted?
    if(!interpretedFile)
    {
        // Use a generic file.
        File1 *container = (hndl.hasFile() && hndl.file().isContained())? &hndl.file().container() : 0;
        interpretedFile = new File1(hndl, filePath, info, container);
    }

    DENG2_ASSERT(interpretedFile);
    return *interpretedFile;
}

FileHandle &FS1::openFile(String const &path, String const &mode, size_t baseOffset, bool allowDuplicate)
{
#ifdef DENG_DEBUG
    for(int i = 0; i < mode.length(); ++i)
    {
        if(mode[i] != 'r' && mode[i] != 't' && mode[i] != 'b' && mode[i] != 'f')
            throw Error("FS1::openFile", "Unknown argument in mode string '" + mode + "'");
    }
#endif

    File1 *file = d->openFile(path, mode, baseOffset, allowDuplicate);
    if(!file) throw NotFoundError("FS1::openFile", "No files found matching '" + path + "'");

    // Add a handle to the opened files list.
    FileHandle &hndl = *FileHandle::fromFile(*file);
    d->openFiles.push_back(&hndl); hndl.setList(reinterpret_cast<de::FileList *>(&d->openFiles));
    return hndl;
}

FileHandle &FS1::openLump(File1 &lump)
{
    // Add a handle to the opened files list.
    FileHandle &hndl = *FileHandle::fromLump(lump);
    d->openFiles.push_back(&hndl); hndl.setList(reinterpret_cast<de::FileList *>(&d->openFiles));
    return hndl;
}

bool FS1::accessFile(de::Uri const &search)
{
    try
    {
        QScopedPointer<File1> file(d->openFile(search.resolved(), "rb", 0, true /* allow duplicates */));
        return !file.isNull();
    }
    catch(de::Uri::ResolveError const &er)
    {
        // Log but otherwise ignore unresolved paths.
        LOGDEV_RES_VERBOSE(er.asText());
    }
    return false;
}

void FS1::addPathLumpMapping(String lumpName, String destination)
{
    if(lumpName.isEmpty() || destination.isEmpty()) return;

    // We require an absolute path - prepend the CWD if necessary.
    if(QDir::isRelativePath(destination))
    {
        String workPath = DENG2_APP->currentWorkPath().withSeparators('/');
        destination = workPath / destination;
    }

    // Have already mapped this path?
    LumpMappings::iterator found = d->lumpMappings.begin();
    for(; found != d->lumpMappings.end(); ++found)
    {
        LumpMapping const &ldm = *found;
        if(!ldm.first.compare(destination, Qt::CaseInsensitive))
            break;
    }

    LumpMapping *ldm;
    if(found == d->lumpMappings.end())
    {
        // No. Acquire another mapping.
        d->lumpMappings.push_back(LumpMapping(destination, lumpName));
        ldm = &d->lumpMappings.back();
    }
    else
    {
        // Remap to another lump.
        ldm = &*found;
        ldm->second = lumpName;
    }

    LOG_RES_MSG("Path \"%s\" now mapped to lump \"%s\"") << NativePath(ldm->first).pretty() << ldm->second;
}

void FS1::clearPathLumpMappings()
{
    d->lumpMappings.clear();
}

/// @return  @c true iff the mapping matched the path.
static bool applyPathMapping(ddstring_t *path, PathMapping const &pm)
{
    if(!path) return false;
    QByteArray destUtf8 = pm.first.toUtf8();
    AutoStr *dest = AutoStr_FromTextStd(destUtf8.constData());
    if(qstrnicmp(Str_Text(path), Str_Text(dest), Str_Length(dest))) return false;

    // Replace the beginning with the source path.
    QByteArray sourceUtf8 = pm.second.toUtf8();
    AutoStr *temp = AutoStr_FromTextStd(sourceUtf8.constData());
    Str_PartAppend(temp, Str_Text(path), pm.first.length(), Str_Length(path) - pm.first.length());
    Str_Copy(path, temp);
    return true;
}

void FS1::addPathMapping(String source, String destination)
{
    if(source.isEmpty() || destination.isEmpty()) return;

    // Have already mapped this source path?
    PathMappings::iterator found = d->pathMappings.begin();
    for(; found != d->pathMappings.end(); ++found)
    {
        PathMapping const &pm = *found;
        if(!pm.second.compare(source, Qt::CaseInsensitive))
            break;
    }

    PathMapping* pm;
    if(found == d->pathMappings.end())
    {
        // No. Acquire another mapping.
        d->pathMappings.push_back(PathMapping(destination, source));
        pm = &d->pathMappings.back();
    }
    else
    {
        // Remap to another destination.
        pm = &*found;
        pm->first = destination;
    }

    LOG_RES_MSG("Path \"%s\" now mapped to \"%s\"")
            << NativePath(pm->second).pretty() << NativePath(pm->first).pretty();
}

void FS1::clearPathMappings()
{
    d->pathMappings.clear();
}

void FS1::printDirectory(Path path)
{
    LOG_RES_MSG(_E(b) "Directory: %s") << NativePath(path).pretty();

    // We are interested in *everything*.
    path = path / "*";

    PathList found;
    if(findAllPaths(path, 0, found))
    {
        qSort(found.begin(), found.end());

        DENG2_FOR_EACH_CONST(PathList, i, found)
        {
            LOG_RES_MSG("  %s") << NativePath(i->path).pretty();
        }
    }
}

bool FS1::knownScheme(String name)
{
    if(!name.isEmpty())
    {
        Schemes::iterator found = d->schemes.find(name.toLower());
        if(found != d->schemes.end()) return true;
    }
    return false;
}

FS1::Scheme &FS1::scheme(String name)
{
    if(!name.isEmpty())
    {
        Schemes::iterator found = d->schemes.find(name.toLower());
        if(found != d->schemes.end()) return **found;
    }
    /// @throw UnknownSchemeError An unknown scheme was referenced.
    throw UnknownSchemeError("FS1::scheme", "No scheme found matching '" + name + "'");
}

FS1::Schemes const &FS1::allSchemes()
{
    return d->schemes;
}

/// Print contents of directories as Doomsday sees them.
D_CMD(Dir)
{
    DENG2_UNUSED(src);
    if(argc > 1)
    {
        for(int i = 1; i < argc; ++i)
        {
            String path = NativePath(argv[i]).expand().withSeparators('/');
            App_FileSystem().printDirectory(path);
        }
    }
    else
    {
        App_FileSystem().printDirectory(String("/"));
    }
    return true;
}

/// Dump a copy of a virtual file to the runtime directory.
D_CMD(DumpLump)
{
    DENG2_UNUSED2(src, argc);

    if(fileSystem)
    {
        lumpnum_t lumpNum = App_FileSystem().lumpNumForName(argv[1]);
        if(lumpNum >= 0)
        {
            return F_DumpFile(App_FileSystem().lump(lumpNum), 0);
        }
        LOG_RES_ERROR("No such lump");
        return false;
    }
    return false;
}

/// List virtual files inside containers.
D_CMD(ListLumps)
{
    DENG2_UNUSED3(src, argc, argv);

    if(!fileSystem) return false;

    LumpIndex const &lumpIndex = App_FileSystem().nameIndex();
    int const numRecords       = lumpIndex.size();
    int const numIndexDigits   = de::max(3, M_NumDigits(numRecords));

    LOG_RES_MSG("LumpIndex %p (%i records):") << &lumpIndex << numRecords;

    int idx = 0;
    DENG2_FOR_EACH_CONST(LumpIndex::Lumps, i, lumpIndex.allLumps())
    {
        File1 const &lump = **i;
        String containerPath  = NativePath(lump.container().composePath()).pretty();
        String lumpPath       = NativePath(lump.composePath()).pretty();

        LOG_RES_MSG(String("%1 - \"%2:%3\" (size: %4 bytes%5)")
                        .arg(idx++, numIndexDigits, 10, QChar('0'))
                        .arg(containerPath)
                        .arg(lumpPath)
                        .arg(lump.info().size)
                        .arg(lump.info().isCompressed()? " compressed" : ""));
    }
    LOG_RES_MSG("---End of lumps---");

    return true;
}

/// List presently loaded files in original load order.
D_CMD(ListFiles)
{
    DENG2_UNUSED3(src, argc, argv);

    LOG_RES_MSG(_E(b) "Loaded Files " _E(l) "(in load order)" _E(w) ":");

    int totalFiles = 0;
    int totalPackages = 0;
    if(fileSystem)
    {
        FS1::FileList const &allLoadedFiles = App_FileSystem().loadedFiles();
        DENG2_FOR_EACH_CONST(FS1::FileList, i, allLoadedFiles)
        {
            File1 &file = (*i)->file();
            uint crc = 0;

            int fileCount = 1;
            if(de::Zip *zip = file.maybeAs<de::Zip>())
            {
                fileCount = zip->lumpCount();
            }
            else if(de::Wad *wad = file.maybeAs<de::Wad>())
            {
                fileCount = wad->lumpCount();
                crc = (!file.hasCustom()? wad->calculateCRC() : 0);
            }

            LOG_RES_MSG(" %s " _E(2)_E(>) "(%i %s%s)%s")
                    << NativePath(file.composePath()).pretty()
                    << fileCount << (fileCount != 1 ? "files" : "file")
                    << (file.hasStartup()? ", startup" : "")
                    << (crc? QString(" [%1]").arg(crc, 0, 16) : "");

            totalFiles += fileCount;
            ++totalPackages;
        }
    }

    LOG_RES_MSG(_E(b)"Total: " _E(.) "%i files in %i packages")
            << totalFiles << totalPackages;

    return true;
}

void FS1::consoleRegister()
{
    C_CMD("dir", "",   Dir);
    C_CMD("ls",  "",   Dir); // Alias
    C_CMD("dir", "s*", Dir);
    C_CMD("ls",  "s*", Dir); // Alias

    C_CMD("dump",      "s", DumpLump);
    C_CMD("listfiles", "",  ListFiles);
    C_CMD("listlumps", "",  ListLumps);
}

FS1 &App_FileSystem()
{
    if(!fileSystem) throw Error("App_FileSystem", "File system not yet initialized");
    return *fileSystem;
}

String App_BasePath()
{
    return App::app().nativeBasePath().withSeparators('/');
}

void F_Init()
{
    DENG2_ASSERT(!fileSystem);
    fileSystem = new FS1();
}

void F_Shutdown()
{
    if(!fileSystem) return;
    delete fileSystem; fileSystem = 0;
}

void const *F_LumpIndex()
{
    return &App_FileSystem().nameIndex();
}
