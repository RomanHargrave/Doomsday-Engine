/** @file render/wallspec.cpp Wall Geometry Specification.
 *
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"

#include "Sector"
#include "Surface"
#include "world/p_players.h" // viewPlayer

#include "render/rend_main.h"

#include "render/walledge.h"

using namespace de;

/**
 * Should angle based light level deltas be applied?
 */
static bool useWallSectionLightLevelDeltas(LineSide const &side, int section)
{
    // Disabled?
    if(rendLightWallAngle <= 0)
        return false;

    // Never if the surface's material was chosen as a HOM fix (lighting must
    // be consistent with that applied to the relative back sector plane).
    if(side.surface(section).hasFixMaterial() &&
       side.hasSector() && side.back().hasSector())
    {
        Sector &backSector = side.back().sector();
        if(backSector.floor().height() < backSector.ceiling().height())
            return false;
    }

    return true;
}

WallSpec WallSpec::fromMapSide(LineSide const &side, int section) // static
{
    bool const isTwoSidedMiddle = (section == LineSide::Middle && !side.considerOneSided());

    WallSpec spec(section);

    if(side.line().definesPolyobj() || isTwoSidedMiddle)
    {
        spec.flags &= ~WallSpec::ForceOpaque;
        spec.flags |= WallSpec::NoEdgeDivisions;
    }

    if(isTwoSidedMiddle)
    {
        if(viewPlayer && ((viewPlayer->shared.flags & (DDPF_NOCLIP|DDPF_CAMERA)) ||
                          !side.line().isFlagged(DDLF_BLOCKING)))
            spec.flags |= WallSpec::NearFade;

        spec.flags |= WallSpec::SortDynLights;
    }

    // Suppress the sky clipping in debug mode.
    if(devRendSkyMode)
        spec.flags &= ~WallSpec::SkyClip;

    if(side.line().definesPolyobj())
        spec.flags |= WallSpec::NoFakeRadio;

    bool useLightLevelDeltas = useWallSectionLightLevelDeltas(side, section);
    if(!useLightLevelDeltas)
        spec.flags |= WallSpec::NoLightDeltas;

    // We can skip normal smoothing if light level delta smoothing won't be done.
    if(!useLightLevelDeltas || !rendLightWallAngleSmooth)
        spec.flags |= WallSpec::NoEdgeNormalSmoothing;

    return spec;
}
