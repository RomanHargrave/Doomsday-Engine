/** @file billboard.h  Rendering billboard "sprites".
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_RENDER_BILLBOARD_H
#define CLIENT_RENDER_BILLBOARD_H

#include "dd_types.h"
#include "Material"
#include "MaterialAnimator"
#include "MaterialVariantSpec"

class BspLeaf;
struct vissprite_t;

/**
 * Billboard drawing arguments for a masked wall.
 *
 * A sort of a sprite, I guess... Masked walls must be rendered sorted with
 * sprites, so no artifacts appear when sprites are seen behind masked walls.
 *
 * @ingroup render
 */
struct drawmaskedwallparams_t
{
    MaterialAnimator *animator;
    blendmode_t blendMode;         ///< Blendmode to be used when drawing (two sided mid textures only)

    struct wall_vertex_s {
        de::dfloat pos[3];         ///< x y and z coordinates.
        de::dfloat color[4];
    } vertices[4];

    de::ddouble texOffset[2];
    de::dfloat texCoord[2][2];     ///< u and v coordinates.

    DGLuint modTex;                ///< Texture to modulate with.
    de::dfloat modTexCoord[2][2];  ///< [top-left, bottom-right][x, y]
    de::dfloat modColor[4];
};

void Rend_DrawMaskedWall(drawmaskedwallparams_t const &parms);

/**
 * Billboard drawing arguments for a "player" sprite (HUD sprite).
 * @ingroup render
 */
struct rendpspriteparams_t
{
    de::dfloat pos[2];           ///< [X, Y] Screen-space position.
    de::dfloat width;
    de::dfloat height;

    Material *mat;
    de::dfloat texOffset[2];
    dd_bool texFlip[2];          ///< [X, Y] Flip along the specified axis.

    de::dfloat ambientColor[4];
    de::duint vLightListIdx;
};

void Rend_DrawPSprite(rendpspriteparams_t const &parms);

/**
 * Billboard drawing arguments for a map entity, sprite visualization.
 * @ingroup render
 */
struct drawspriteparams_t
{
    dd_bool noZWrite;
    blendmode_t blendMode;
    MaterialAnimator *matAnimator;
    dd_bool matFlip[2];             ///< [S, T] Flip along the specified axis.
    BspLeaf *bspLeaf;
};

void Rend_DrawSprite(vissprite_t const &spr);

de::MaterialVariantSpec const &Rend_SpriteMaterialSpec(de::dint tclass = 0, de::dint tmap = 0);

/**
 * @defgroup rendFlareFlags  Flare renderer flags
 * @ingroup render
 * @{
 */
#define RFF_NO_PRIMARY      0x1  ///< Do not draw a primary flare (aka halo).
#define RFF_NO_TURN         0x2  ///< Flares do not turn in response to viewangle/viewdir
/**@}*/

/**
 * Billboard drawing arguments for a lens flare.
 *
 * @see H_RenderHalo()
 * @ingroup render
 */
struct drawflareparams_t
{
    de::dbyte flags;       ///< @ref rendFlareFlags.
    de::dint size;
    de::dfloat color[3];
    de::dbyte factor;
    de::dfloat xOff;
    DGLuint tex;           ///< Flaremap if flareCustom ELSE (flaretexName id. Zero = automatical)
    de::dfloat mul;        ///< Flare brightness factor.
    dd_bool isDecoration;
    de::dint lumIdx;
};

DENG_EXTERN_C de::dint alwaysAlign;
DENG_EXTERN_C de::dint spriteLight;
DENG_EXTERN_C de::dint useSpriteAlpha;
DENG_EXTERN_C de::dint useSpriteBlend;
DENG_EXTERN_C de::dint noSpriteZWrite;
DENG_EXTERN_C de::dbyte noSpriteTrans;
DENG_EXTERN_C de::dbyte devNoSprites;

DENG_EXTERN_C void Rend_SpriteRegister();

#endif  // CLIENT_RENDER_BILLBOARD_H
