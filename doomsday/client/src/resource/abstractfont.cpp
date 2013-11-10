/** @file abstractfont.cpp Abstract font.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "resource/abstractfont.h"

using namespace de;

AbstractFont::AbstractFont(fonttype_t type, fontid_t bindId)
{
    DENG_ASSERT(VALID_FONTTYPE(type));

    _type = type;
    _marginWidth  = 0;
    _marginHeight = 0;
    _leading = 0;
    _ascent = 0;
    _descent = 0;
    _noCharSize.width  = 0;
    _noCharSize.height = 0;
    _primaryBind = bindId;
    _isDirty = true;
}

void AbstractFont::glInit()
{}

void AbstractFont::glDeinit()
{}

void AbstractFont::charSize(Size2Raw *size, unsigned char ch)
{
    if(!size) return;
    size->width  = charWidth(ch);
    size->height = charHeight(ch);
}

fonttype_t AbstractFont::type() const
{
    return _type;
}

fontid_t AbstractFont::primaryBind() const
{
    return _primaryBind;
}

void AbstractFont::setPrimaryBind(fontid_t bindId)
{
    _primaryBind = bindId;
}

int AbstractFont::flags() const
{
    return _flags;
}

int AbstractFont::ascent()
{
    glInit();
    return _ascent;
}

int AbstractFont::descent()
{
    glInit();
    return _descent;
}

int AbstractFont::lineSpacing()
{
    glInit();
    return _leading;
}
