/** @file directoryfeed.cpp Directory Feed.
 *
 * @author Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include "de/DirectoryFeed"
#include "de/Folder"
#include "de/NativeFile"
#include "de/FS"
#include "de/Date"
#include "de/App"

#include <QDir>
#include <QFileInfo>

using namespace de;

DirectoryFeed::DirectoryFeed(NativePath const &nativePath, Flags const &mode)
    : _nativePath(nativePath), _mode(mode) {}

DirectoryFeed::~DirectoryFeed()
{}

String DirectoryFeed::description() const
{
    return "directory \"" + _nativePath.pretty() + "\"";
}

NativePath const &DirectoryFeed::nativePath() const
{
    return _nativePath;
}

void DirectoryFeed::populate(Folder &folder)
{
    if(_mode & AllowWrite)
    {
        // Automatically enable modifying the Folder.
        folder.setMode(File::Write);
    }
    if(_mode.testFlag(CreateIfMissing) && !exists(_nativePath))
    {
        createDir(_nativePath);
    }

    QDir dir(_nativePath);
    if(!dir.isReadable())
    {
        /// @throw NotFoundError The native directory was not accessible.
        throw NotFoundError("DirectoryFeed::populate", "Path '" + _nativePath + "' inaccessible");
    }
    QStringList nameFilters;
    nameFilters << "*";
    foreach(QFileInfo entry,
            dir.entryInfoList(nameFilters, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot))
    {
        if(entry.isDir())
        {
            populateSubFolder(folder, entry.fileName());
        }
        else
        {
            populateFile(folder, entry.fileName());
        }
    }
}

void DirectoryFeed::populateSubFolder(Folder &folder, String const &entryName)
{
    LOG_AS("DirectoryFeed::populateSubFolder");

    if(entryName != "." && entryName != "..")
    {
        Folder &subFolder = folder.fileSystem()
                .makeFolder(folder.path() / entryName, FS::InheritPrimaryFeed);

        if(_mode & AllowWrite)
        {
            subFolder.setMode(File::Write);
        }
        else
        {
            subFolder.setMode(File::ReadOnly);
        }
    }
}

void DirectoryFeed::populateFile(Folder &folder, String const &entryName)
{
    if(folder.has(entryName))
    {
        // Already has an entry for this, skip it (wasn't pruned so it's OK).
        return;
    }

    NativePath entryPath = _nativePath / entryName;

    // Open the native file.
    std::auto_ptr<NativeFile> nativeFile(new NativeFile(entryName, entryPath));
    nativeFile->setStatus(fileStatus(entryPath));
    if(_mode & AllowWrite)
    {
        nativeFile->setMode(File::Write);
    }

    File *file = folder.fileSystem().interpret(nativeFile.release());
    folder.add(file);

    // We will decide on pruning this.
    file->setOriginFeed(this);

    // Include files in the main index.
    folder.fileSystem().index(*file);
}

bool DirectoryFeed::prune(File &file) const
{
    LOG_AS("DirectoryFeed::prune");

    /// Rules for pruning:
    /// - A file sourced by NativeFile will be pruned if it's out of sync with the hard
    ///   drive version (size, time of last modification).
    if(NativeFile *nativeFile = file.maybeAs<NativeFile>())
    {
        try
        {
            if(fileStatus(nativeFile->nativePath()) != nativeFile->status())
            {
                // It's not up to date.
                LOG_RES_MSG("Pruning \"%s\": status has changed") << nativeFile->nativePath();
                return true;
            }
        }
        catch(StatusError const &)
        {
            // Get rid of it.
            return true;
        }
    }

    /// - A Folder will be pruned if the corresponding directory does not exist (providing
    ///   a DirectoryFeed is the sole feed in the folder).
    if(Folder *subFolder = file.maybeAs<Folder>())
    {
        if(subFolder->feeds().size() == 1)
        {
            DirectoryFeed *dirFeed = subFolder->feeds().front()->maybeAs<DirectoryFeed>();
            if(dirFeed && !exists(dirFeed->_nativePath))
            {
                LOG_RES_NOTE("Pruning \"%s\": no longer exists") << _nativePath;
                return true;
            }
        }
    }

    /// - Other types of Files will not be pruned.
    return false;
}

File *DirectoryFeed::newFile(String const &name)
{
    NativePath newPath = _nativePath / name;
    if(exists(newPath))
    {
        /// @throw AlreadyExistsError  The file @a name already exists in the native directory.
        throw AlreadyExistsError("DirectoryFeed::newFile", name + ": already exists");
    }
    File *file = new NativeFile(name, newPath);
    file->setOriginFeed(this);
    return file;
}

void DirectoryFeed::removeFile(String const &name)
{
    NativePath path = _nativePath / name;

    if(!exists(path))
    {
        // The file doesn't exist in the native file system, we can ignore this.
        return;
    }

    if(!QDir::current().remove(path))
    {
        /// @throw RemoveError  The file @a name exists but could not be removed.
        throw RemoveError("DirectoryFeed::removeFile", "Cannot remove \"" + name +
                          "\" in " + description());
    }
}

Feed *DirectoryFeed::newSubFeed(String const &name)
{
    NativePath subPath = _nativePath / name;
    if(_mode.testFlag(CreateIfMissing) || (subPath.exists() && subPath.isReadable()))
    {
        return new DirectoryFeed(subPath, _mode);
    }
    return 0;
}

void DirectoryFeed::changeWorkingDir(NativePath const &nativePath)
{
    if(!App::setCurrentWorkPath(nativePath))
    {
        /// @throw WorkingDirError Changing to @a nativePath failed.
        throw WorkingDirError("DirectoryFeed::changeWorkingDir",
                              "Failed to change to " + nativePath);
    }
}

void DirectoryFeed::createDir(NativePath const &nativePath)
{
    NativePath parentPath = nativePath.fileNamePath();
    if(!parentPath.isEmpty() && !exists(parentPath))
    {
        createDir(parentPath);
    }

    if(!QDir::current().mkdir(nativePath))
    {
        /// @throw CreateDirError Failed to create directory @a nativePath.
        throw CreateDirError("DirectoryFeed::createDir", "Could not create: " + nativePath);
    }
}

bool DirectoryFeed::exists(NativePath const &nativePath)
{
    return QDir::current().exists(nativePath);
}

File::Status DirectoryFeed::fileStatus(NativePath const &nativePath)
{
    QFileInfo info(nativePath);

    if(!info.exists())
    {
        /// @throw StatusError Determining the file status was not possible.
        throw StatusError("DirectoryFeed::fileStatus", nativePath + " inaccessible");
    }

    // Get file status information.
    return File::Status(info.size(), info.lastModified());
}
