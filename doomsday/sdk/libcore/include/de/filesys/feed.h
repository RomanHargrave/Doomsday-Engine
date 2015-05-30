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

#ifndef LIBDENG2_FEED_H
#define LIBDENG2_FEED_H

#include "../libcore.h"

#include <list>

namespace de {

class File;
class Folder;
class String;

/**
 * Base class for feeds that generate File and Folder instances.
 *
 * While File and Folder instances are responsible for the organization of
 * the data, and provide access to the content bytes, it is up to the Feed
 * instances to interpret the contents of files and folders and generate
 * the appropriate File/Folder instances.
 *
 * When it comes time to repopulate the file system, feeds are responsible
 * for determining whether a given File or Folder needs to be destroyed
 * (pruned). For instance, pruning a NativeFile is necessary if the
 * corresponding native file has been deleted from the hard drive since the
 * latest population was done.
 *
 * The first Feed attached to a Folder is the primary feed.
 *
 * @ingroup fs
 */
class DENG2_PUBLIC Feed
{
public:
    /// Tried to create a new file that already exists in the native file
    /// system. @ingroup errors
    DENG2_ERROR(AlreadyExistsError);

    /// Failed to remove a file. @ingroup errors
    DENG2_ERROR(RemoveError);

public:
    Feed();

    virtual ~Feed();

    /**
     * Returns a textual description of the feed, intended only for humans.
     *
     * @return Human-friendly description of the feed.
     */
    virtual String description() const = 0;

    /**
     * Populates a folder with File instances. Subclasses implement this to
     * produce the appropriate instances for the data they handle.
     *
     * @param folder  Folder where the File instances are placed.
     *
     * @see Folder::populate()
     */
    virtual void populate(Folder &folder) = 0;

    /**
     * Determines whether a file has become obsolete and needs to be pruned.
     * The file should be deleted if it needs to be pruned. If the Feed cannot
     * make a decision on whether pruning is needed, @c false should be returned.
     *
     * @param file  File to check for pruning.
     *
     * @return  @c true, if the file should be pruned and deleted, otherwise
     *      @c false.
     */
    virtual bool prune(File &file) const = 0;

    /**
     * Creates a new file with a given name and sets the new file's origin feed
     * to this feed.
     *
     * @param name  Name of the new file.
     *
     * @return  The new file. Caller gets ownership.
     */
    virtual File *newFile(String const &name);

    /**
     * Removes a file with the given name.
     *
     * @param name  Name of the removed file.
     */
    virtual void removeFile(String const &name);

    /**
     * Creates a new feed suitable for attaching to a subfolder of whatever
     * source this feed is based on.
     *
     * @param name  Name of the subfolder.
     *
     * @return New feed instance. Caller gets ownership.
     */
    virtual Feed *newSubFeed(String const &name);

    DENG2_AS_IS_METHODS()
};

} // namespace de

#endif /* LIBDENG2_FEED_H */
