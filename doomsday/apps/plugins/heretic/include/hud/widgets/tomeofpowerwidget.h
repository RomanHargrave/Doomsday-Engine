/** @file tomeofpowerwidget.h  GUI widget for -.
 *
 * @authors Copyright © 2005-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBHERETIC_UI_TOMEOFPOWERWIDGET_H
#define LIBHERETIC_UI_TOMEOFPOWERWIDGET_H

#include "hud/hudwidget.h"

/**
 * @ingroup ui
 */
class guidata_tomeofpower_t : public HudWidget
{
public:
    guidata_tomeofpower_t(de::dint player);
    virtual ~guidata_tomeofpower_t();

    void reset();

    void tick(timespan_t elapsed);
    void updateGeometry();
    void draw(de::Vector2i const &offset = de::Vector2i()) const;

public:
    static void prepareAssets();

private:
    DENG2_PRIVATE(d)
};

#endif  // LIBHERETIC_UI_TOMEOFPOWERWIDGET_H
