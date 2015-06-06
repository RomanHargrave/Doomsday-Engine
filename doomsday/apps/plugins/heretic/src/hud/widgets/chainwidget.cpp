/** @file chainwidget.cpp  GUI widget for -.
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

#include "hud/widgets/chainwidget.h"

#include "common.h"
#include "hu_lib.h"
#include "p_actor.h"
#include "p_tick.h"

using namespace de;

static void ChainWidget_Draw(guidata_chain_t *chain, Point2Raw const *offset)
{
    DENG2_ASSERT(chain);
    chain->draw(offset? Vector2i(offset->xy) : Vector2i());
}

static void ChainWidget_UpdateGeometry(guidata_chain_t *chain)
{
    DENG2_ASSERT(chain);
    chain->updateGeometry();
}

static patchid_t pChain;
static patchid_t pGem[NUMTEAMS];

guidata_chain_t::guidata_chain_t(dint player)
    : HudWidget(function_cast<UpdateGeometryFunc>(ChainWidget_UpdateGeometry),
                function_cast<DrawFunc>(ChainWidget_Draw),
                player)
{}

guidata_chain_t::~guidata_chain_t()
{}

void guidata_chain_t::reset()
{
    _healthMarker = 0;
    _wiggle       = 0;
}

void guidata_chain_t::tick(timespan_t /*elapsed*/)
{
    static int const MAX_DELTA = 4;

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    player_t const *plr  = &::players[player()];
    dint const curHealth = de::max(plr->plr->mo->health, 0);

    // Health marker chain animates up to the actual health value.
    dint delta = 0;
    if(curHealth < _healthMarker)
    {
        delta = -de::clamp(1, (_healthMarker - curHealth) >> 2, MAX_DELTA);
    }
    else if(curHealth > _healthMarker)
    {
        delta = de::clamp(1, (curHealth - _healthMarker) >> 2, MAX_DELTA);
    }
    _healthMarker += delta;

    if(_healthMarker != curHealth && (::mapTime & 1))
    {
        _wiggle = P_Random() & 1;
    }
    else
    {
        _wiggle = 0;
    }

}

static void drawShadows(dint x, dint y, dfloat alpha)
{
    DGL_Begin(DGL_QUADS);
        // Left shadow.
        DGL_Color4f(0, 0, 0, alpha);
        DGL_Vertex2f(x+20, y+ST_HEIGHT);
        DGL_Vertex2f(x+20, y+ST_HEIGHT-10);
        DGL_Color4f(0, 0, 0, 0);
        DGL_Vertex2f(x+35, y+ST_HEIGHT-10);
        DGL_Vertex2f(x+35, y+ST_HEIGHT);

        // Right shadow.
        DGL_Vertex2f(x+ST_WIDTH-43, y+ST_HEIGHT);
        DGL_Vertex2f(x+ST_WIDTH-43, y+ST_HEIGHT-10);
        DGL_Color4f(0, 0, 0, alpha);
        DGL_Vertex2f(x+ST_WIDTH-27, y+ST_HEIGHT-10);
        DGL_Vertex2f(x+ST_WIDTH-27, y+ST_HEIGHT);
    DGL_End();
}

void guidata_chain_t::draw(Vector2i const &offset) const
{
    static float const ORIGINX = -ST_WIDTH / 2;
    static float const ORIGINY = 0;

    static dint const theirColors[] = {
        /*Green*/ 220, /*Yellow*/ 144, /*Red*/ 150, /*Blue*/ 197
    };

    dint const activeHud     = ST_ActiveHud(player());
    dint const yOffset       = ST_HEIGHT * (1 - ST_StatusBarShown(player()));
    dfloat const iconOpacity = (activeHud == 0? 1 : ::uiRendState->pageAlpha * ::cfg.common.statusbarCounterAlpha);

    if(ST_AutomapIsOpen(player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

    patchinfo_t pChainInfo;
    if(!R_GetPatchInfo(pChain, &pChainInfo)) return;

    dint teamColor = 2; // Always use the red gem in single player.
    if(IS_NETGAME)
    {
        teamColor = ::cfg.playerColor[player()];
    }

    patchinfo_t pGemInfo;
    if(!R_GetPatchInfo(::pGem[teamColor], &pGemInfo)) return;

    dint const chainY      = -9 + _wiggle;
    dfloat const healthPos = de::clamp(0.f, _healthMarker / 100.f, 1.f);
    dfloat const gemglow   = healthPos;

    // Draw the chain.
    dint x = ORIGINX + 21;
    dint y = ORIGINY + chainY;
    dint w = ST_WIDTH - 21 - 28;
    dint h = 8;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(offset.x, offset.y, 0);
    DGL_Scalef(::cfg.common.statusbarScale, ::cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_SetPatch(pChain, DGL_REPEAT, DGL_CLAMP);
    DGL_Color4f(1, 1, 1, iconOpacity);

    dfloat const gemXOffset = (w - pGemInfo.geometry.size.width) * healthPos;
    if(gemXOffset > 0)
    {
        // Left chain section.
        dfloat cw = gemXOffset / pChainInfo.geometry.size.width;
        DGL_Begin(DGL_QUADS);
            DGL_TexCoord2f(0, 1 - cw, 0);
            DGL_Vertex2f(x, y);

            DGL_TexCoord2f(0, 1, 0);
            DGL_Vertex2f(x + gemXOffset, y);

            DGL_TexCoord2f(0, 1, 1);
            DGL_Vertex2f(x + gemXOffset, y + h);

            DGL_TexCoord2f(0, 1 - cw, 1);
            DGL_Vertex2f(x, y + h);
        DGL_End();
    }

    if(gemXOffset + pGemInfo.geometry.size.width < w)
    {
        // Right chain section.
        dfloat cw = (w - gemXOffset - pGemInfo.geometry.size.width) / pChainInfo.geometry.size.width;
        DGL_Begin(DGL_QUADS);
            DGL_TexCoord2f(0, 0, 0);
            DGL_Vertex2f(x + gemXOffset + pGemInfo.geometry.size.width, y);

            DGL_TexCoord2f(0, cw, 0);
            DGL_Vertex2f(x + w, y);

            DGL_TexCoord2f(0, cw, 1);
            DGL_Vertex2f(x + w, y + h);

            DGL_TexCoord2f(0, 0, 1);
            DGL_Vertex2f(x + gemXOffset + pGemInfo.geometry.size.width, y + h);
        DGL_End();
    }

    // Draw the life gem.
    DGL_Color4f(1, 1, 1, iconOpacity);
    GL_DrawPatch(pGemInfo.id, Vector2i(x + gemXOffset, chainY));

    DGL_Disable(DGL_TEXTURE_2D);

    drawShadows(ORIGINX, ORIGINY - ST_HEIGHT, iconOpacity / 2);

    // How about a glowing gem?
    DGL_BlendMode(BM_ADD);
    DGL_Bind(Get(DD_DYNLIGHT_TEXTURE));
    DGL_Enable(DGL_TEXTURE_2D);

    dfloat rgb[3]; R_GetColorPaletteRGBf(0, theirColors[teamColor], rgb, false);
    DGL_DrawRectf2Color(x + gemXOffset - 11, chainY - 6, 41, 24, rgb[0], rgb[1], rgb[2], gemglow - (1 - iconOpacity));

    DGL_Color4f(1, 1, 1, 1);
    DGL_Disable(DGL_TEXTURE_2D);
    DGL_BlendMode(BM_NORMAL);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void guidata_chain_t::updateGeometry()
{
    Rect_SetWidthHeight(&geometry(), 0, 0);

    if(ST_AutomapIsOpen(player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

    /// @todo Calculate dimensions properly.
    Rect_SetWidthHeight(&geometry(), (ST_WIDTH - 21 - 28) * ::cfg.common.statusbarScale,
                                     8 * ::cfg.common.statusbarScale);
}

void guidata_chain_t::prepareAssets()
{
    ::pChain  = R_DeclarePatch("CHAIN");
    ::pGem[0] = R_DeclarePatch("LIFEGEM0");
    ::pGem[1] = R_DeclarePatch("LIFEGEM1");
    ::pGem[2] = R_DeclarePatch("LIFEGEM2");
    ::pGem[3] = R_DeclarePatch("LIFEGEM3");
}
