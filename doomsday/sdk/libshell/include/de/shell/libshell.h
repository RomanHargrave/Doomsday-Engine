/** @file libshell.h  Common definitions for libshell.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBSHELL_MAIN_H
#define LIBSHELL_MAIN_H

#include <de/String>
#include <de/Range>

/*
 * The LIBSHELL_PUBLIC macro is used for declaring exported symbols. It must be
 * applied in all exported classes and functions. DEF files are not used for
 * exporting symbols out of libshell.
 */
#if defined(_WIN32) && defined(_MSC_VER)
#  ifdef __LIBSHELL__
// This is defined when compiling the library.
#    define LIBSHELL_PUBLIC __declspec(dllexport)
#  else
#    define LIBSHELL_PUBLIC __declspec(dllimport)
#  endif
#else
// No need to use any special declarators.
#  define LIBSHELL_PUBLIC
#endif

namespace de {
namespace shell {

// Default TCP/UDP port for servers to listen on.
static duint16 const DEFAULT_PORT = 13209;

/**
 * Line of word-wrapped text.
 */
struct LIBSHELL_PUBLIC WrappedLine
{
    Rangei range;
    bool isFinal;

    WrappedLine(Rangei const &range_, bool final = false)
        : range(range_), isFinal(final) {}
};

class LIBSHELL_PUBLIC ILineWrapping
{
public:
    virtual ~ILineWrapping() {}

    virtual bool isEmpty() const = 0;
    virtual void clear() = 0;
    virtual void wrapTextToWidth(String const &text, int maxWidth) = 0;
    virtual WrappedLine line(int index) const = 0;

    /// Determines the visible maximum width of the wrapped content.
    virtual int width() const = 0;

    /// Determines the number of lines in the wrapped content.
    virtual int height() const = 0;

    /// Returns the advance width of the range.
    virtual int rangeWidth(Rangei const &range) const = 0;

    /**
     * Calculates which index in the provided content range occupies a
     * character at a given width.
     *
     * @param range  Range within the content.
     * @param width  Advance width to check.
     */
    virtual int indexAtWidth(Rangei const &range, int width) const = 0;
};

} // namespace shell
} // namespace de

#endif // LIBSHELL_MAIN_H
