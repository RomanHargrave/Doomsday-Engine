/** @file archive.cpp Collection of named memory blocks stored inside a byte array.
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

#include "de/Archive"

namespace de {

DENG2_PIMPL(Archive)
{
    /// Source data provided at construction.
    IByteArray const *source;

    /// Index maps entry paths to their metadata. Created by concrete subclasses
    /// but we have the ownership.
    PathTree *index;

    /// Contents of the archive have been modified.
    bool modified;

    Instance(Public &a, IByteArray const *src) : Base(a), source(src), index(0), modified(false)
    {}

    ~Instance()
    {
        delete index;
    }

    void readEntry(Path const &path, IBlock &deserializedData) const
    {
        Entry const &entry = static_cast<Entry const &>(
                    index->find(path, PathTree::MatchFull | PathTree::NoBranch));
        if(!entry.size)
        {
            // Empty entry; nothing to do.
            deserializedData.clear();
            return;
        }

        // Do we already have a deserialized copy of this entry?
        if(entry.data)
        {
            deserializedData.copyFrom(*entry.data, 0, entry.data->size());
            return;
        }

        self.readFromSource(entry, path, deserializedData);
    }
};

Archive::Archive() : d(new Instance(*this, 0))
{}

Archive::Archive(IByteArray const &archive) : d(new Instance(*this, &archive))
{}

Archive::~Archive()
{
    clear();
}

IByteArray const *Archive::source() const
{
    return d->source;
}

void Archive::cache(CacheAttachment attach)
{
    if(!d->source)
    {
        // Nothing to read from.
        return;
    }
    PathTreeIterator<PathTree> iter(d->index->leafNodes());
    while(iter.hasNext())
    {
        Entry &entry = static_cast<Entry &>(iter.next());
        if(!entry.data && !entry.dataInArchive)
        {
            entry.dataInArchive = new Block(*d->source, entry.offset, entry.sizeInArchive);
        }
    }
    if(attach == DetachFromSource)
    {
        d->source = 0;
    }
}

bool Archive::hasEntry(Path const &path) const
{
    DENG2_ASSERT(d->index != 0);

    return d->index->has(path, PathTree::MatchFull | PathTree::NoBranch);
}

dint Archive::listFiles(Archive::Names &names, Path const &folder) const
{
    DENG2_ASSERT(d->index != 0);

    names.clear();

    // Find the folder in the index.
    if(PathTree::Node const *parent = d->index->tryFind(folder, PathTree::MatchFull | PathTree::NoLeaf))
    {
        // Traverse the parent's nodes.
        for(PathTreeIterator<PathTree> iter(parent->children().leaves); iter.hasNext(); )
        {
            names.insert(iter.next().name());
        }
    }
    return dint(names.size());
}

dint Archive::listFolders(Archive::Names &names, Path const &folder) const
{
    DENG2_ASSERT(d->index != 0);

    names.clear();

    // Find the folder in the index.
    if(PathTree::Node const *parent = d->index->tryFind(folder, PathTree::MatchFull | PathTree::NoLeaf))
    {
        // Traverse the parent's nodes.
        for(PathTreeIterator<PathTree> iter(parent->children().branches); iter.hasNext(); )
        {
            names.insert(iter.next().name());
        }
    }
    return dint(names.size());
}

File::Status Archive::entryStatus(Path const &path) const
{
    DENG2_ASSERT(d->index != 0);

    Entry const &found = static_cast<Entry const &>(d->index->find(path, PathTree::MatchFull));

    return File::Status(
        found.isLeaf()? File::Status::FILE : File::Status::FOLDER,
        found.size,
        found.modifiedAt);
}

Block const &Archive::entryBlock(Path const &path) const
{
    DENG2_ASSERT(d->index != 0);

    try
    {
        // We'll need to modify the entry.
        Entry &entry = static_cast<Entry &>(d->index->find(path, PathTree::MatchFull | PathTree::NoBranch));
        if(entry.data)
        {
            // Got it.
            return *entry.data;
        }
        std::auto_ptr<Block> cached(new Block);
        d->readEntry(path, *cached.get());
        entry.data = cached.release();
        return *entry.data;
    }
    catch(PathTree::NotFoundError const &)
    {
        /// @throw NotFoundError Entry with @a path was not found.
        throw NotFoundError("Archive::entryBlock", String("'%1' not found").arg(path));
    }
}

Block &Archive::entryBlock(Path const &path)
{
    if(!hasEntry(path))
    {
        add(path, Block());
    }

    Block const &block = const_cast<Archive const *>(this)->entryBlock(path);
    
    // Mark for recompression.
    Entry &entry = static_cast<Entry &>(d->index->find(path, PathTree::MatchFull | PathTree::NoBranch));
    entry.maybeChanged = true;
    entry.modifiedAt   = Time();

    d->modified = true;
    
    return const_cast<Block &>(block);
}

void Archive::add(Path const &path, IByteArray const &data)
{
    if(path.isEmpty())
    {
        /// @throws InvalidPathError  Provided path was not a valid path.
        throw InvalidPathError("Archive::add",
                               QString("'%1' is an invalid path for an entry").arg(path));
    }

    // Get rid of the earlier entry with this path.
    remove(path);

    DENG2_ASSERT(d->index != 0);

    Entry &entry = static_cast<Entry &>(d->index->insert(path));
    entry.data         = new Block(data);
    entry.modifiedAt   = Time();
    entry.maybeChanged = true;

    // The rest of the data gets updated when the archive is written.

    d->modified = true;
}

void Archive::remove(Path const &path)
{
    DENG2_ASSERT(d->index != 0);

    if(d->index->remove(path, PathTree::MatchFull | PathTree::NoBranch))
    {
        d->modified = true;
    }
}

void Archive::clear()
{
    DENG2_ASSERT(d->index != 0);

    d->index->clear();
    d->modified = true;
}

bool Archive::modified() const
{
    return d->modified;
}

void Archive::setIndex(PathTree *tree)
{
    d->index = tree;
}

Archive::Entry &Archive::insertEntry(Path const &path)
{
    LOG_AS("Archive");
    DENG2_ASSERT(d->index != 0);

    // Remove any existing node at this path.
    d->index->remove(path, PathTree::MatchFull | PathTree::NoBranch);

    return static_cast<Entry &>(d->index->insert(path));
}

PathTree const &Archive::index() const
{
    DENG2_ASSERT(d->index != 0);

    return *d->index;
}

} // namespace de
