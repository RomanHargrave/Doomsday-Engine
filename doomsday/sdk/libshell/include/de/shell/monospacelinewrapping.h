/** @file monospacelinewrapping.h
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBSHELL_MONOSPACELINEWRAPPING_H
#define LIBSHELL_MONOSPACELINEWRAPPING_H

#include "libshell.h"
#include <QList>

namespace de {
namespace shell {

/**
 * Line wrapper that assumes that all characters are the same width. Width is
 * defined in characters, height in lines.
 */
class LIBSHELL_PUBLIC MonospaceLineWrapping : public ILineWrapping
{
public:
    MonospaceLineWrapping();

    bool isEmpty() const;

    void clear();

    /**
     * Determines word wrapping for a line of text given a maximum line width.
     *
     * @param text      Text to wrap.
     * @param maxWidth  Maximum width for each text line.
     *
     * @return List of positions in @a text where to break the lines. Total number
     * of word-wrapped lines is equal to the size of the returned list.
     */
    void wrapTextToWidth(String const &text, int maxWidth);

    WrappedLine line(int index) const { return _lines[index]; }
    int width() const;
    int height() const;
    int rangeWidth(Rangei const &range) const;
    int indexAtWidth(Rangei const &range, int width) const;

private:
    QList<WrappedLine> _lines;
};

} // namespace shell
} // namespace de

#endif // LIBSHELL_MONOSPACELINEWRAPPING_H
