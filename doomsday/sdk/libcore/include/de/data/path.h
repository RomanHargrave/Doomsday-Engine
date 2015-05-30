/** @file path.h Textual path composed of segments.
 *
 * @author Copyright © 2010-2013 Daniel Swanson <danij@dengine.net>
 * @author Copyright © 2010-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_PATH_H
#define LIBDENG2_PATH_H

#include <algorithm> // std::swap

#include "../libcore.h"
#include "../Error"
#include "../Log"
#include "../ISerializable"
#include "../String"

namespace de {

/**
 * A textual path composed of segments. @ingroup data
 *
 * A path is a case insensitive text string that is broken down into segments.
 * Path is a generic class and as such does not provide any interpretation of
 * what the path refers to; it just parses the string and splits it into
 * segments. The user may choose any character to act as the segment separator.
 *
 * Paths are used when identifying and organizing structured data. One
 * practical example is file system paths.
 *
 * Internally, the class avoids duplicating the provided path String (i.e.,
 * string is not altered), instead relying on the implicit sharing of QString.
 */
class DENG2_PUBLIC Path : public ISerializable, public LogEntry::Arg::Base
{
    struct Instance; // needs to be friended by Path::Segment

public:
    /// Segment index was out of bounds. @ingroup errors
    DENG2_ERROR(OutOfBoundsError);

    /// Type used to represent a path segment hash key.
    typedef ushort hash_type;

    /// Range of a path segment hash key; [0..hash_range)
    static hash_type const hash_range;

    /**
     * Marks a segment in the path. Makes no copy of the segments in the path,
     * only stores the location within the path where they begin and end.
     *
     * Examples:
     * - Empty path (as produced by the default constructor) => one empty segment ""
     * - Unix-style root directory "/" => two empty segments "", ""
     * - Windows-style root directory "c:/" => "c:", ""
     * - relative path "some/dir/file.ext" => "some", "dir", file.ext"
     * - Unix-style absolute path "/some/file.ext" => "", "some", file.ext"
     *
     * @see URI paths are composed of "segments":
     * http://tools.ietf.org/html/rfc3986#section-3.3
     */
    struct DENG2_PUBLIC Segment
    {
        /**
         * Segments are implicitly converted to text strings.
         */
        operator String () const;

        /**
         * Converts the segment to a string.
         */
        String toString() const;

        inline QStringRef const &toStringRef() const { return range; }

        /**
         * Determines the length of the segment in characters.
         * Same as size().
         */
        int length() const;

        /**
         * Determines the length of the segment in characters.
         * Same as length().
         */
        dsize size() const;

        /**
         * Returns a somewhat-random number in the range [0..Path::hash_range)
         * generated from the segment.
         *
         * @return  The generated hash key.
         */
        hash_type hash() const;

        /**
         * Case insensitive equality test.
         *
         * @param other Other segment.
         *
         * @return @c true, iff the segments are equal.
         */
        bool operator == (Segment const &other) const;

        bool operator != (Segment const &other) const {
            return !(*this == other);
        }

        bool operator == (QString const &text) const {
            return !range.compare(text, Qt::CaseInsensitive);
        }

        bool operator != (QString const &text) const {
            return !(*this == text);
        }

        /**
         * Returns @c true if this segment is lexically less than @a other.
         * The test is case and separator insensitive.
         */
        bool operator < (Segment const &other) const;

        friend class Path;
        friend struct Path::Instance;

    private:
        mutable bool gotHashKey;
        mutable hash_type hashKey;
        QStringRef range;
    };

public:
    /**
     * Construct an empty Path instance.
     */
    Path();

    /**
     * Construct a path from @a path.
     *
     * @param path  Path to be parsed. The supplied string is used as-is
     *              (implicit sharing): all white space is included in the path.
     * @param sep   Character used to separate path segments in @a path.
     */
    Path(String const &path, QChar sep = '/');

    /**
     * Construct a path from @a str with '/' as the segment separator.
     *
     * @param str  String.
     */
    Path(QString const &str);

    /**
     * Construct a path from a UTF-8 C-style string.
     *
     * @param nullTerminatedCStr  Path to be parsed. All white space is included in the path.
     * @param sep   Character used to separate path segments.
     */
    Path(char const *nullTerminatedCStr, char sep);

    /**
     * Construct a path from a UTF-8 C-style string that uses '/' separators.
     *
     * @param nullTerminatedCStr  Path to be parsed. All white space is included in the path.
     */
    Path(char const *nullTerminatedCStr);

    /**
     * Construct a path by duplicating @a other.
     */
    Path(Path const &other);

    virtual ~Path();

    inline Path &operator = (Path other) {
        std::swap(d, other.d);
        return *this;
    }

    /**
     * Append a string.
     *
     * @param str  String.
     *
     * @return Path with @a str added to the end.
     *
     * @note This is a plain string append, not a path concatenation: use the /
     * operator for concatenating paths in a way that takes care of separators
     * and path relativity.
     */
    Path operator + (QString const &str) const;

    /**
     * @copydoc operator+
     */
    Path operator + (char const *str) const;

    /**
     * Swaps this Path with @a other.
     * @param other  Path.
     */
    inline void swap(Path &other) {
        std::swap(d, other.d);
    }

    /**
     * Determines if this path is equal to @a other. The test is case
     * and separator insensitive.
     *
     * Examples:
     * - "hello/world" (sep: /) == "HELLO/World" (sep: /)
     * - "hello/world" (sep: /) == "Hello|World" (sep: |)
     *
     * @param other  Path.
     *
     * @return @c true, iff the paths are equal.
     */
    bool operator == (Path const &other) const;

    /**
     * Determines if this path is not equal to @a other. The test is case
     * and separator insensitive.
     *
     * @param other  Path.
     *
     * @return @c true, iff the paths are not equal.
     */
    bool operator != (Path const &other) const {
        return !(*this == other);
    }

    /**
     * Returns @c true if this path is lexically less than @a other. The test
     * is case and separator insensitive.
     */
    bool operator < (Path const &other) const;

    /**
     * Concatenate paths together. This path's separator will be used for
     * the resulting path. @see String::concatenatePath()
     *
     * @param other  Path to concatenate after this one.
     *
     * @return Concatenated path.
     */
    Path operator / (Path const &other) const;

    /**
     * Concatenate paths together. This path's separator will be used for
     * the resulting path. @see String::concatenatePath()
     *
     * @param other  Path to concatenate after this one. '/' is used as
     *               the separator.
     *
     * @return Concatenated path.
     */
    Path operator / (QString other) const;

    Path operator / (char const *otherNullTerminatedUtf8) const;

    /**
     * Convert this path to a text string.
     */
    operator String() const {
        return toString();
    }

    /**
     * Convert this path to a text string.
     */
    String toString() const;

    /**
     * Returns a reference to the path as a string.
     */
    String const &toStringRef() const;

    /**
     * Returns @c true if the path is empty; otherwise @c false.
     */
    bool isEmpty() const;

    bool isAbsolute() const;

    /// Returns the length of the path.
    int length() const;

    /// Returns the length of the path.
    dsize size() const;

    /// Returns the first character of the path.
    QChar first() const;

    /// Returns the last character of the path.
    QChar last() const;

    /**
     * Clear the path.
     */
    Path &clear();

    /**
     * Assigns a new path with '/' separators.
     *
     * @param newPath  Path where '/' is the segment separator character.
     *
     * @return This Path.
     */
    Path &operator = (String const &newPath);

    /**
     * Changes the path.
     *
     * @param newPath  New path.
     * @param sep      Character used to separate path segments in @a path.
     */
    Path &set(String const &newPath, QChar sep = '/');

    /**
     * Returns a copy of the path where all segment separators have been
     * replaced with a new character.
     *
     * @param sep  Character used to replace segment separators.
     *
     * @return  Path with new separators.
     */
    Path withSeparators(QChar sep = '/') const;

    /**
     * Returns the character used as segment separator.
     */
    QChar separator() const;

    /**
     * Returns the file name portion of the path, i.e., the last segment.
     */
    String fileName() const;

    Block toUtf8() const;

    /**
     * Retrieve a reference to the segment at @a index. In this method the
     * segments are indexed left to right, in the same order as they appear in
     * the original textual path. There is always at least one segment (index
     * 0, the first segment).
     *
     * @note The zero-length name in UNIX-style absolute paths is also treated
     *       as a segment. For example, the path "/Users/username" has three
     *       segments ("", "Users", "username").
     *
     * @param index  Index of the segment to look up. All paths have
     *               at least one segment (even empty ones) thus index @c 0 will
     *               always be valid.
     *
     * @return  Referenced segment. Do not keep the returned reference after
     * making a change to the path.
     */
    Segment const &segment(int index) const;

    /**
     * Retrieve a reference to the segment at @a reverseIndex. In this method
     * the segments are indexed in reverse order (right to left). There is
     * always at least one segment (index 0, the last segment).
     *
     * For example, if the path is "c:/mystuff/myaddon.addon" the corresponding
     * segment map is arranged as follows:
     * <pre>
     *   [0:{myaddon.addon}, 1:{mystuff}, 2:{c:}].
     * </pre>
     *
     * @note The zero-length name in UNIX-style absolute paths is also treated
     *       as a segment. For example, the path "/Users/username" has three
     *       segments ("username", "Users", "").
     *
     * @param reverseIndex  Reverse-index of the segment to look up. All paths have
     *                      at least one segment (even empty ones) thus index @c 0 will
     *                      always be valid.
     *
     * @return  Referenced segment. Do not keep the returned reference after
     * making a change to the path.
     */
    Segment const &reverseSegment(int reverseIndex) const;

    /**
     * @return  Total number of segments in the segment map. There is always
     * at least one segment.
     */
    int segmentCount() const;

    /**
     * @return First (i.e., left-most) segment in the path. If the path is
     * empty, the returned segment is an empty, zero-length segment.
     */
    inline Segment const &firstSegment() const {
        return segment(0);
    }

    /**
     * @return  Last (i.e., right-most) segment in the path. If the path is empty,
     * the returned segment is an empty, zero-length segment.
     */
    inline Segment const &lastSegment() const {
        return segment(segmentCount() - 1);
    }

    // Implements LogEntry::Arg::Base.
    LogEntry::Arg::Type logEntryArgType() const { return LogEntry::Arg::StringArgument; }
    String asText() const {
        return toString();
    }

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

public:
    /**
     * Normalizes slashes in a string so that they are replaced with the given
     * character (defaults to forward slash).
     *
     * @param text          String where to replace separators.
     * @param replaceWith   New separator character.
     *
     * @return String with all slashes replaced with @a replaceWith.
     */
    static String normalizeString(String const &text, QChar replaceWith = '/');

    /**
     * Makes a path where the given input text is first normalized so that
     * slashes are replaced with the given character (defaults to forward
     * slash).
     *
     * @param text          String where to replace separators.
     * @param replaceWith   New separator character.
     *
     * @return Path with @a replaceWith used in place of slashes.
     */
    static Path normalize(String const &text, QChar replaceWith = '/');

private:
    Instance *d;
};

/**
 * Utility class for specifying paths that use a dot (.) as the path separator.
 * @ingroup data
 */
class DENG2_PUBLIC DotPath : public Path
{
public:
    DotPath(String const &path = "")        : Path(path, '.') {}
    DotPath(QString const &str)             : Path(str, '.') {}
    DotPath(char const *nullTerminatedCStr) : Path(nullTerminatedCStr, '.') {}
    DotPath(Path const &other)              : Path(other) {}
    DotPath(DotPath const &other)           : Path(other.toStringRef(), other.separator()) {}

    bool operator == (DotPath const &other) const {
        return Path::operator == (other);
    }

    bool operator != (DotPath const &other) const {
        return Path::operator != (other);
    }
};

/**
 * Utility class for referring to a portion of an existing (immutable) path.
 * @ingroup data
 */
class DENG2_PUBLIC PathRef
{
public:
    PathRef(Path const &path)
        : _path(path)
        , _range(0, path.segmentCount()) {}
    PathRef(Path const &path, Rangei const &segRange)
        : _path(path)
        , _range(segRange) {}

    Path const &path() const { return _path; }
    Rangei range() const { return _range; }

    bool isEmpty() const { return _range.isEmpty(); }
    bool isAbsolute() const {
        return !isEmpty() && !firstSegment().size();
    }
    Path::Segment const &segment(int index) const {
        return _path.segment(_range.start + index);
    }
    int segmentCount() const { return _range.size(); }
    inline Path::Segment const &firstSegment() const {
        return segment(0);
    }
    inline Path::Segment const &lastSegment() const {
        return segment(segmentCount() - 1);
    }
    PathRef subPath(Rangei const &sub) const {
        return PathRef(_path, sub + _range.start);
    }
    Path toPath() const;

private:
    Path const &_path;
    Rangei _range;
};

} // namespace de

namespace std {
    // std::swap specialization for de::Path
    template <>
    inline void swap<de::Path>(de::Path &a, de::Path &b) {
        a.swap(b);
    }
}

#endif // LIBDENG2_PATH_H
