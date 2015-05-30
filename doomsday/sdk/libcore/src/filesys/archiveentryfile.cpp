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

#include "de/ArchiveEntryFile"
#include "de/ArchiveFeed"
#include "de/Archive"
#include "de/Block"
#include "de/Guard"

namespace de {

ArchiveEntryFile::ArchiveEntryFile(String const &name, Archive &archive, String const &entryPath)
    : ByteArrayFile(name), _archive(archive), _entryPath(entryPath)
{}

ArchiveEntryFile::~ArchiveEntryFile()
{
    DENG2_GUARD(this);

    DENG2_FOR_AUDIENCE2(Deletion, i) i->fileBeingDeleted(*this);
    audienceForDeletion().clear();
    
    deindex();
}

String ArchiveEntryFile::entryPath() const
{
    return _entryPath;
}

String ArchiveEntryFile::describe() const
{
    DENG2_GUARD(this);

    return String("archive entry \"%1\"").arg(_entryPath);
}

void ArchiveEntryFile::clear()
{
    DENG2_GUARD(this);

    verifyWriteAccess();

    File::clear();
    
    archive().entryBlock(_entryPath).clear();
    
    // Update status.
    Status st = status();
    st.size = 0;
    st.modifiedAt = Time();
    setStatus(st);
}

void ArchiveEntryFile::flush()
{
    ByteArrayFile::flush();
    if(ArchiveFeed *feed = originFeed()->maybeAs<ArchiveFeed>())
    {
        feed->rewriteFile();
    }
}

IByteArray::Size ArchiveEntryFile::size() const
{
    DENG2_GUARD(this);

    return archive().entryBlock(_entryPath).size();
}

void ArchiveEntryFile::get(Offset at, Byte *values, Size count) const
{
    DENG2_GUARD(this);

    archive().entryBlock(_entryPath).get(at, values, count);
}

void ArchiveEntryFile::set(Offset at, Byte const *values, Size count)
{
    DENG2_GUARD(this);

    verifyWriteAccess();
    
    // The entry will be marked for recompression (due to non-const access).
    Block &entryBlock = archive().entryBlock(_entryPath);
    entryBlock.set(at, values, count);
    
    // Update status.
    Status st = status();
    st.size = entryBlock.size();
    // Timestamps must match, otherwise would be pruned needlessly.
    st.modifiedAt = archive().entryStatus(_entryPath).modifiedAt;
    setStatus(st);
}

} // namespace de
