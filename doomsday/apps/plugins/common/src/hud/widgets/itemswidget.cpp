/** @file itemswidget.h  GUI widget for -.
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

#include "hud/widgets/itemswidget.h"

#include "common.h"
#include "p_actor.h"

using namespace de;

static void ItemsWidget_Draw(guidata_items_t *items, Point2Raw const *offset)
{
    DENG2_ASSERT(items);
    items->draw(offset? Vector2i(offset->xy) : Vector2i());
}

static void ItemsWidget_UpdateGeometry(guidata_items_t *items)
{
    DENG2_ASSERT(items);
    items->updateGeometry();
}

guidata_items_t::guidata_items_t(dint player)
    : HudWidget(function_cast<UpdateGeometryFunc>(ItemsWidget_UpdateGeometry),
                function_cast<DrawFunc>(ItemsWidget_Draw),
                player)
{}

guidata_items_t::~guidata_items_t()
{}

void guidata_items_t::reset()
{
    _value = 1994;
}

void guidata_items_t::tick(timespan_t /*elapsed*/)
{
    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    player_t const *plr = &players[player()];
    _value = plr->itemCount;
}

void guidata_items_t::draw(Vector2i const &offset) const
{
#if !__JHEXEN__

    dfloat const textOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudColor[3];

    if(!(::cfg.common.hudShownCheatCounters & (CCH_ITEMS | CCH_ITEMS_PRCNT))) return;
    if(ST_AutomapIsOpen(player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;
    if(::cfg.common.hudCheatCounterShowWithAutomap && !ST_AutomapIsOpen(player())) return;

    if(_value == 1994) return;

    String valueAsText("Items:");
    if(cfg.common.hudShownCheatCounters & CCH_ITEMS)
    {
        valueAsText += String(" %1/%2").arg(_value).arg(totalItems);
    }
    if(cfg.common.hudShownCheatCounters & CCH_ITEMS_PRCNT)
    {
        valueAsText += String(" %1%2%%3")
                           .arg((::cfg.common.hudShownCheatCounters & CCH_ITEMS) ? "(" : "")
                           .arg(totalItems ? _value * 100 / totalItems : 100)
                           .arg((::cfg.common.hudShownCheatCounters & CCH_ITEMS) ? ")" : "");
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(offset.x, offset.y, 0);
    DGL_Scalef(::cfg.common.hudCheatCounterScale, ::cfg.common.hudCheatCounterScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(font());
    FR_SetColorAndAlpha(::cfg.common.hudColor[0], ::cfg.common.hudColor[1], ::cfg.common.hudColor[2], textOpacity);
    FR_DrawTextXY(valueAsText.toUtf8().constData(), 0, 0);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#else  // !__JHEXEN__
    DENG2_UNUSED(offset);
#endif
}

void guidata_items_t::updateGeometry()
{
#if !__JHEXEN__

    Rect_SetWidthHeight(&geometry(), 0, 0);

    if(!(::cfg.common.hudShownCheatCounters & (CCH_ITEMS | CCH_ITEMS_PRCNT))) return;
    if(ST_AutomapIsOpen(player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;
    if(::cfg.common.hudCheatCounterShowWithAutomap && !ST_AutomapIsOpen(player())) return;

    if(_value == 1994) return;

    String valueAsText("Items:");
    if(cfg.common.hudShownCheatCounters & CCH_ITEMS)
    {
        valueAsText += String(" %1/%2").arg(_value).arg(totalItems);
    }
    if(cfg.common.hudShownCheatCounters & CCH_ITEMS_PRCNT)
    {
        valueAsText += String(" %1%2%%3")
                           .arg((::cfg.common.hudShownCheatCounters & CCH_ITEMS) ? "(" : "")
                           .arg(totalItems ? _value * 100 / totalItems : 100)
                           .arg((::cfg.common.hudShownCheatCounters & CCH_ITEMS) ? ")" : "");
    }

    FR_SetFont(font());
    Size2Raw textSize; FR_TextSize(&textSize, valueAsText.toUtf8().constData());
    Rect_SetWidthHeight(&geometry(), .5f + textSize.width  * ::cfg.common.hudCheatCounterScale,
                                     .5f + textSize.height * ::cfg.common.hudCheatCounterScale);

#endif  // !__JHEXEN__
}
