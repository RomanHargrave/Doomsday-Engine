/** @file weaponslotwidget.h  GUI widget for visualizing player weapon ownership.
 *
 * @authors Copyright © 2005-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOM_UI_WEAPONSLOTWIDGET_H
#define LIBDOOM_UI_WEAPONSLOTWIDGET_H

#include "hud/hudwidget.h"

/**
 * @ingroup ui
 */
class guidata_weaponslot_t : public HudWidget
{
public:
    guidata_weaponslot_t(de::dint player);
    virtual ~guidata_weaponslot_t();

    void reset();

    guidata_weaponslot_t &setSlot(de::dint newSlotNum);

    void tick(timespan_t elapsed);
    void updateGeometry();
    void draw(de::Vector2i const &offset = de::Vector2i()) const;

public:
    static void prepareAssets();

private:
    de::dint _slot = 0;
    patchid_t _patchId = 0;
};

#endif  // LIBDOOM_UI_WEAPONSLOTWIDGET_H
