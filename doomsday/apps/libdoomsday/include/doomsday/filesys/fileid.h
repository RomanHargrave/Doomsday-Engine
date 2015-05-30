/**
 * @file fileid.h
 *
 * Implements a file identifier in terms of a MD5 hash of its absolute path.
 *
 * @ingroup types
 *
 * @deprecated Should use FS2 instead for file access.
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILEID_H
#define LIBDENG_FILEID_H

#include "../libdoomsday.h"
#include <QByteArray>
#include <de/Log>
#include <de/String>

namespace de {

/**
 * File identifier (an MD5 hash).
 */
class LIBDOOMSDAY_PUBLIC FileId : public LogEntry::Arg::Base
{
public:
    typedef QByteArray Md5Hash;

public:
    explicit FileId(Md5Hash _md5);

    FileId(FileId const& other);

    FileId& operator = (FileId other);

    /// @return @c true= this FileId is lexically less than @a other.
    bool operator < (FileId const& other) const;

    /// @return @c true= this FileId is equal to @a other (identical hashes).
    bool operator == (FileId const& other) const;

    /// @return @c true= this FileId is not equal @a other (differing hashes).
    bool operator != (FileId const& other) const;

    friend void swap(FileId& first, FileId& second) // nothrow
    {
#ifdef DENG2_QT_4_8_OR_NEWER
        first.md5_.swap(second.md5_);
#  ifdef DENG_DEBUG
        first.path_.swap(second.path_);
#  endif
#else
        std::swap(first.md5_, second.md5_);
#  ifdef DENG_DEBUG
        std::swap(first.path_, second.path_);
#  endif
#endif
    }

    /// Converts this FileId to a text string.
    operator String () const { return asText(); }

    /// Converts this FileId to a text string.
    String asText() const;

    // Implements LogEntry::Arg::Base.
    LogEntry::Arg::Type logEntryArgType() const { return LogEntry::Arg::StringArgument; }

    /// @return Md5hash for this FileId.
    Md5Hash const& md5() const { return md5_; }

#ifdef DENG_DEBUG
    /// @return Path attributed to this FileId.
    String const& path() const { return path_; }

    /// Set the path attributed to this FileId.
    FileId& setPath(String path)
    {
        path_ = path;
        return *this;
    }
#endif

    /**
     * Constructs a new FileId instance by hashing the absolute @a path.
     * @param path  Path to be hashed.
     * @return Newly construced FileId.
     */
    static FileId fromPath(String path);

    /**
     * Calculate an MD5 identifier for the absolute @a path.
     * @return MD5 hash of the path.
     */
    static Md5Hash hash(String path);

private:
    Md5Hash md5_;

#ifdef DENG_DEBUG
    String path_;
#endif
};

} // namespace de

#endif /* LIBDENG_FILEID_H */
