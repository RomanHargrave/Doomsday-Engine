/** @file bluemanaiconwidget.h  GUI widget for -.
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

#ifndef LIBHEXEN_UI_BLUEMANAICONWIDGET_H
#define LIBHEXEN_UI_BLUEMANAICONWIDGET_H

#include "hud/hudwidget.h"

/**
 * @ingroup ui
 */
class guidata_bluemanaicon_t : public HudWidget
{
public:
    guidata_bluemanaicon_t(void (*updateGeometry) (HudWidget *wi),
                           void (*drawer) (HudWidget *wi, Point2Raw const *offset),
                           de::dint player);
    virtual ~guidata_bluemanaicon_t();

    void reset();

    void tick(timespan_t elapsed);
    //void updateGeometry();
    //void draw(de::Vector2i const &offset = de::Vector2i()) const;

public:
    static void prepareAssets();

//private:
    de::dint _iconIdx = 0;
};

void BlueManaIconWidget_Draw    (guidata_bluemanaicon_t *icon, Point2Raw const *offset);
void SBarBlueManaIconWidget_Draw(guidata_bluemanaicon_t *icon, Point2Raw const *offset);

void BlueManaIconWidget_UpdateGeometry    (guidata_bluemanaicon_t *icon);
void SBarBlueManaIconWidget_UpdateGeometry(guidata_bluemanaicon_t *icon);

#endif  // LIBHEXEN_UI_BLUEMANAICONWIDGET_H
