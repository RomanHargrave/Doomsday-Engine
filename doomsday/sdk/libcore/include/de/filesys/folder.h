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

#ifndef LIBDENG2_FOLDER_H
#define LIBDENG2_FOLDER_H

#include "../libcore.h"
#include "../File"

#include <map>
#include <list>

namespace de {

class Feed;

/**
 * A folder contains a set of files. It is used for building a tree of files
 * in the file system (de::FS). This is the base class for all types of folders.
 * @ingroup fs
 *
 * The first Feed attached to a Folder is the primary feed.
 *
 * @par Deriving from Folder
 *
 * See the requirements that apply to deriving from File; the same apply for Folder,
 * as it is itself derived from File.
 *
 * @see Feed
 */
class DENG2_PUBLIC Folder : public File
{
public:
    /// A folder cannot contain two or more files with the same name. @ingroup errors
    DENG2_ERROR(DuplicateNameError);

    /// File path did not point to a file. @ingroup errors
    DENG2_ERROR(NotFoundError);

    /// Creating a new file was unsuccessful. @ingroup errors
    DENG2_ERROR(NewFileError);

    /**
     * Accesses the properties of a Folder. Allows using properties of a
     * folder (like how many items it contains) as a Value, for instance in
     * script expressions.
     *
     * @ingroup fs
     */
    class Accessor : public AccessorValue
    {
    public:
        /// Property of the file to access.
        enum Property {
            CONTENT_SIZE
        };

    public:
        Accessor(Folder &owner, Property prop);
        void update() const;
        Value *duplicateContent() const;

    private:
        Folder &_owner;
        Property _prop;
    };

    typedef std::list<Feed *> Feeds;
    typedef std::map<String, File *> Contents;

    enum PopulationBehavior {
        PopulateFullTree       = 0,     ///< The full tree is populated.
        PopulateOnlyThisFolder = 0x1    ///< Do not descend into subfolders while populating.
    };

    /// Behavior for creating new files.
    enum FileCreationBehavior {
        KeepExisting,       ///< Existing file will be kept unchanged (safe).
        ReplaceExisting     ///< Existing file will be replaced.
    };

public:
    Folder(String const &name = "");

    virtual ~Folder();

    String describe() const;
    
    String describeFeeds() const;

    /**
     * Populates the folder with a set of File instances. Each feed attached to
     * the folder will contribute File instances constructed based on the
     * feeds' source data. Every populated file will also be added to the file
     * system's main index.
     *
     * Repopulation is nondestructive as long as the source data has not
     * changed. Population may be performed more than once during the lifetime
     * of the folder, for example when it's necessary to synchronize it with
     * the contents of a native hard drive directory.
     *
     * This should never be called "just in case"; only populate when you know
     * that the source data has changed and the file tree corresponding that
     * data needs to be updated to reflect the changes. Operations done using
     * File and Folder instances automatically keep the tree up to date even
     * when they apply changes to the source data, so population should not be
     * performed in that case.
     *
     * @param behavior  Behavior of the population operation, see
     *                  Folder::PopulationBehavior.
     */
    virtual void populate(PopulationBehavior behavior = PopulateFullTree);

    /**
     * Provides direct read-only access to the content of the folder.
     */
    Contents const &contents() const;

    /**
     * Empties the contents of the folder: all contained file instances are
     * deleted. Attached feeds are not notified, which means the source data
     * they translate into the folder remains untouched.
     */
    void clear();

    /**
     * Creates a new file in the folder. The feeds attached to the folder will
     * decide what kind of file is actually created, and perform the
     * construction of the new File instance. The new file is added to the file
     * system's index.
     *
     * @param name      Name or path of the new file, relative to this folder.
     * @param behavior  How to treat existing files.
     *
     * @return  The created file (write mode enabled).
     */
    File &newFile(String const &name, FileCreationBehavior behavior = KeepExisting);

    /**
     * Creates a new file in the folder, replacing an existing file with the
     * same name. Same as calling <code>newFile(name, true)</code>.
     *
     * @param name  Name or path of the new file, relative to this folder.
     *
     * @return  The created file (write mode enabled).
     */
    File &replaceFile(String const &name);

    /**
     * Removes a file from a folder. The file will be deleted. If it has an
     * origin feed, the feed will be asked to remove the file as well, which
     * means it will be removed in the source data as well as the file tree.
     *
     * @param name  Name or path of file to remove, relative to this folder.
     */
    void removeFile(String const &name);

    /**
     * Checks whether the folder contains a file.
     *
     * @param name  File to check for. The name is not case sensitive.
     */
    bool has(String const &name) const;

    /**
     * Adds an object to the folder. The object must be an instance of a class
     * derived from File.
     *
     * @param fileObject  Object to add to the folder. The folder takes
     *      ownership of this object. Cannot be NULL.
     *
     * @return  Reference to @a fileObject, for convenience.
     */
    template <typename Type>
    Type &add(Type *fileObject) {
        DENG2_ASSERT(fileObject != 0);
        add(static_cast<File *>(fileObject));
        return *fileObject;
    }

    /**
     * Adds a file instance to the contents of the folder.
     *
     * @param file  File to add. The folder takes ownership of this instance.
     *
     * @return  Reference to the file, for convenience.
     */
    virtual File &add(File *file);

    /**
     * Removes a file from the folder, by name. The file is not deleted. The
     * ownership of the file is given to the caller.
     *
     * @return  The removed file object. Ownership of the object is given to
     * the caller.
     */
    File *remove(String const &name);

    File *remove(char const *nameUtf8);

    template <typename Type>
    Type *remove(Type *fileObject) {
        DENG2_ASSERT(fileObject != 0);
        remove(*static_cast<File *>(fileObject));
        return fileObject;
    }

    /**
     * Removes a file from the folder. The file is not deleted. The ownership
     * of the file is given to the caller.
     *
     * @return  The removed file object. Ownership of the object is given to
     * the caller.
     */
    virtual File *remove(File &file);

    File *tryLocateFile(String const &path) const;

    template <typename Type>
    Type *tryLocate(String const &path) const {
        return dynamic_cast<Type *>(tryLocateFile(path));
    }

    /**
     * Locates a file in this folder or in one of its subfolders. Looks recusively
     * through subfolders.
     *
     * @param path  Path to look for. Relative to this folder.
     *
     * @return  The found file.
     */
    template <typename Type>
    Type &locate(String const &path) const {
        Type *found = tryLocate<Type>(path);
        if(!found) {
            /// @throw NotFoundError  Path didn't exist, or the located file had
            /// an incompatible type.
            throw NotFoundError("Folder::locate", "\"" + path +"\" was not found or had incompatible type "
                                "(in " + description() + ")");
        }
        return *found;
    }

    /**
     * Attach a feed to the folder. The feed will provide content for the
     * folder. The first feed attached to a folder is the primary feed.
     *
     * @param feed  Feed to attach to the folder. The folder gets ownership of
     *              the feed.
     */
    void attach(Feed *feed);

    /**
     * Detaches a feed from the folder. The feed object is not deleted.
     *
     * @param feed  Feed to detach from the folder.
     *
     * @return  Feed instance. Ownership is returned to the caller.
     */
    Feed *detach(Feed &feed);

    /**
     * Makes the specified feed the primary one.
     *
     * @param feed  Feed instance to make the primary feed. Must already be
     *              attached to the Folder.
     */
    void setPrimaryFeed(Feed &feed);

    /**
     * Detaches all feeds and deletes the Feed instances. Existing files in the
     * folder are unaffected.
     */
    void clearFeeds();

    /**
     * Provides access to the list of Feeds for this folder. The feeds are responsible
     * for creating File and Folder instances in the folder.
     */
    Feeds const &feeds() const;

    String contentsAsText() const;

    // filesys::Node overrides:
    Node const *tryFollowPath(PathRef const &path) const;
    Node const *tryGetChild(String const &name) const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif /* LIBDENG2_FOLDER_H */
