/** @file r_things.cpp  Map Object => Vissprite Projection.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @authors Copyright © 1993-1996 by id Software, Inc.
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

#include "de_platform.h"
#include "render/r_things.h"

#include <de/vector1.h>
#include <de/ModelDrawable>

#include "clientapp.h"
#include "de_render.h"
#include "dd_main.h" // App_WorldSystem()
#include "dd_loop.h" // frameTimePos
#include "def_main.h" // states

#include "gl/gl_tex.h"
#include "gl/gl_texmanager.h" // GL_PrepareFlaremap

#include "network/net_main.h" // clients[]

#include "render/angleclipper.h"
#include "render/mobjanimator.h"
#include "render/vissprite.h"

#include "world/map.h"
#include "world/p_object.h"
#include "world/p_players.h"
#include "world/clientmobjthinkerdata.h"
#include "BspLeaf"
#include "ConvexSubspace"
#include "SectorCluster"

using namespace de;

static inline RenderSystem &rendSys()
{
    return ClientApp::renderSystem();
}

static void evaluateLighting(Vector3d const &origin, ConvexSubspace &subspaceAtOrigin,
    coord_t distToEye, bool fullbright, Vector4f &ambientColor, duint *vLightListIdx)
{
    if(fullbright)
    {
        ambientColor = Vector3f(1, 1, 1);
        *vLightListIdx = 0;
    }
    else
    {
        SectorCluster &cluster = subspaceAtOrigin.cluster();
        Map &map = cluster.sector().map();

        if(useBias && map.hasLightGrid())
        {
            // Evaluate the position in the light grid.
            Vector4f color = map.lightGrid().evaluate(origin);
            // Apply light range compression.
            for(dint i = 0; i < 3; ++i)
            {
                color[i] += Rend_LightAdaptationDelta(color[i]);
            }
            ambientColor = color;
        }
        else
        {
            Vector4f const color = cluster.lightSourceColorfIntensity();

            dfloat lightLevel = color.w;
            /* if(spr->type == VSPR_DECORATION)
            {
                // Wall decorations receive an additional light delta.
                lightLevel += R_WallAngleLightLevelDelta(line, side);
            } */

            // Apply distance attenuation.
            lightLevel = Rend_AttenuateLightLevel(distToEye, lightLevel);

            // Add extra light.
            lightLevel = de::clamp(0.f, lightLevel + Rend_ExtraLightDelta(), 1.f);

            Rend_ApplyLightAdaptation(lightLevel);

            // Determine the final color.
            ambientColor = color * lightLevel;
        }
        Rend_ApplyTorchLight(ambientColor, distToEye);

        *vLightListIdx = Rend_CollectAffectingLights(origin, ambientColor, &subspaceAtOrigin);
    }
}

/// @todo use Mobj_OriginSmoothed
static Vector3d mobjOriginSmoothed(mobj_t *mob)
{
    DENG2_ASSERT(mob);
    coord_t origin[] = { mob->origin[0], mob->origin[1], mob->origin[2] };

    // The client may have a Smoother for this object.
    if(isClient && mob->dPlayer && P_GetDDPlayerIdx(mob->dPlayer) != consolePlayer)
    {
        Smoother_Evaluate(clients[P_GetDDPlayerIdx(mob->dPlayer)].smoother, origin);
    }

    return origin;
}

/**
 * Determine the correct Z coordinate for the mobj. The visible Z coordinate
 * may be slightly different than the actual Z coordinate due to smoothed
 * plane movement.
 *
 * @todo fixme: Should use the visual plane heights of sector clusters.
 */
static void findMobjZOrigin(mobj_t &mob, bool floorAdjust, vissprite_t &vis)
{
    validCount++;
    Mobj_Map(mob).forAllSectorsTouchingMobj(mob, [&mob, &floorAdjust, &vis] (Sector &sector)
    {
        if(floorAdjust && mob.origin[2] == sector.floor().height())
        {
            vis.pose.origin.z = sector.floor().heightSmoothed();
        }

        if(mob.origin[2] + mob.height == sector.ceiling().height())
        {
            vis.pose.origin.z = sector.ceiling().heightSmoothed() - mob.height;
        }
        return LoopContinue;
    });
}

void R_ProjectSprite(mobj_t &mob)
{
    /// @todo Lots of stuff here! This needs to be broken down into multiple functions
    /// and/or classes that handle preprocessing of visible entities. Keep in mind that
    /// data/state can persist across frames in the mobjs' private data. -jk

    // Not all objects can/will be visualized. Skip this object if:
    // ...hidden?
    if((mob.ddFlags & DDMF_DONTDRAW)) return;
    // ...not linked into the map?
    if(!Mobj_HasSubspace(mob)) return;
    // ...in an invalid state?
    if(!mob.state || !runtimeDefs.states.indexOf(mob.state)) return;
    // ...no sprite frame is defined?
    Sprite *sprite = Mobj_Sprite(mob);
    if(!sprite) return;
    // ...fully transparent?
    dfloat const alpha = Mobj_Alpha(mob);
    if(alpha <= 0) return;
    // ...origin lies in a sector with no volume?
    ConvexSubspace &subspace = Mobj_BspLeafAtOrigin(mob).subspace();
    SectorCluster &cluster   = subspace.cluster();
    if(!cluster.hasWorldVolume()) return;

    ClientMobjThinkerData const *mobjData = THINKER_DATA_MAYBE(mob.thinker, ClientMobjThinkerData);

    // Determine distance to object.
    Vector3d const moPos = mobjOriginSmoothed(&mob);
    coord_t const distFromEye = Rend_PointDist2D(moPos);

    // Should we use a 3D model?
    ModelDef *mf = nullptr, *nextmf = nullptr;
    dfloat interp = 0;

    ModelDrawable::Animator const *animator = nullptr; // GL2 model present?

    if(useModels)
    {
        mf = Mobj_ModelDef(mob, &nextmf, &interp);
        if(mf)
        {
            // Use a sprite if the object is beyond the maximum model distance.
            if(maxModelDistance && !(mf->flags & MFF_NO_DISTANCE_CHECK)
               && distFromEye > maxModelDistance)
            {
                mf = nextmf = nullptr;
                interp = -1;
            }
        }

        if(mobjData)
        {
            animator = mobjData->animator();
        }
    }

    bool const hasModel = (mf || animator);

    // Decide which material to use according to the sprite's angle and position
    // relative to that of the viewer.
    Material *mat = nullptr;
    bool matFlipS = false;
    bool matFlipT = false;

    try
    {
        SpriteViewAngle const &sprViewAngle =
            sprite->closestViewAngle(mob.angle, R_ViewPointToAngle(mob.origin), !!mf);

        mat      = sprViewAngle.material;
        matFlipS = sprViewAngle.mirrorX;
    }
    catch(Sprite::MissingViewAngleError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_GL_WARNING("Projecting sprite '%i' frame '%i': %s")
                << mob.sprite << mob.frame << er.asText();
    }
    if(!mat) return;
    MaterialAnimator &matAnimator = mat->getAnimator(Rend_SpriteMaterialSpec(mob.tclass, mob.tmap));

    // Ensure we've up to date info about the material.
    matAnimator.prepare();

    Vector2i const &matDimensions = matAnimator.dimensions();
    TextureVariant *tex           = matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture;

    // A valid sprite texture in the "Sprites" scheme is required.
    if(!tex || tex->base().manifest().schemeName().compareWithoutCase("Sprites"))
    {
        return;
    }

    bool const fullbright = ((mob.state->flags & STF_FULLBRIGHT) != 0 || levelFullBright);
    // Align to the view plane? (Means scaling down Z with models)
    bool const viewAlign  = (!mf && ((mob.ddFlags & DDMF_VIEWALIGN) || alwaysAlign == 1))
                            || alwaysAlign == 3;

    // Perform visibility checking by projecting a view-aligned line segment
    // relative to the viewer and determining if the whole of the segment has
    // been clipped away according to the 360 degree angle clipper.
    coord_t const visWidth = Mobj_VisualRadius(mob) * 2;  /// @todo ignorant of rotation...
    Vector2d v1, v2;
    R_ProjectViewRelativeLine2D(moPos, mf || viewAlign, visWidth,
                                (mf? 0 : coord_t(-tex->base().origin().x) - (visWidth / 2.0f)),
                                v1, v2);

    // Not visible?
    if(!rendSys().angleClipper().checkRangeFromViewRelPoints(v1, v2))
    {
        coord_t const MAX_OBJECT_RADIUS = 128;

        // Sprite visibility is absolute.
        if(!hasModel) return;

        // If the model is close to the viewpoint we should still to draw it,
        // otherwise large models are likely to disappear too early.
        viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);
        Vector2d delta(distFromEye, moPos.z + (mob.height / 2) - viewData->current.origin.z);
        if(M_ApproxDistance(delta.x, delta.y) > MAX_OBJECT_RADIUS)
            return;
    }

    // Store information in a vissprite.
    vissprite_t *vis = R_NewVisSprite(animator? VSPR_MODEL_GL2 :
                                            mf? VSPR_MODEL :
                                                VSPR_SPRITE);

    vis->pose.origin   = moPos;
    vis->pose.distance = distFromEye;

    // The Z origin of the visual should match that of the mobj. When smoothing
    // is enabled this requires examining all touched sector planes in the vicinity.
    Plane &floor     = cluster.visFloor();
    Plane &ceiling   = cluster.visCeiling();
    bool floorAdjust = false;
    if(!Mobj_OriginBehindVisPlane(&mob))
    {
        floorAdjust = de::abs(floor.heightSmoothed() - floor.height()) < 8;
        findMobjZOrigin(mob, floorAdjust, *vis);
    }

    coord_t topZ = vis->pose.origin.z + -tex->base().origin().y;  // global z top

    // Determine floor clipping.
    coord_t floorClip = mob.floorClip;
    if(mob.ddFlags & DDMF_BOB)
    {
        // Bobbing is applied using floorclip.
        floorClip += Mobj_BobOffset(mob);
    }

    // Determine angles.
    /// @todo Surely this can be done in a subclass/function. -jk
    dfloat yaw = 0, pitch = 0;
    if(animator)
    {
        // TODO: More angle options with GL2 models.

        yaw = Mobj_AngleSmoothed(&mob) / dfloat( ANGLE_MAX ) * -360;
    }
    else if(mf)
    {
        // Determine the rotation angles (in degrees).
        if(mf->testSubFlag(0, MFF_ALIGN_YAW))
        {
            // Transform the origin point.
            viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);
            Vector2d delta(moPos.y - viewData->current.origin.y,
                           moPos.x - viewData->current.origin.x);

            yaw = 90 - (BANG2RAD(bamsAtan2(delta.x * 10, delta.y * 10)) - PI / 2) / PI * 180;
        }
        else if(mf->testSubFlag(0, MFF_SPIN))
        {
            yaw = modelSpinSpeed * 70 * App_WorldSystem().time() + MOBJ_TO_ID(&mob) % 360;
        }
        else if(mf->testSubFlag(0, MFF_MOVEMENT_YAW))
        {
            yaw = R_MovementXYYaw(mob.mom[0], mob.mom[1]);
        }
        else
        {
            yaw = Mobj_AngleSmoothed(&mob) / dfloat( ANGLE_MAX ) * -360;
        }

        // How about a unique offset?
        if(mf->testSubFlag(0, MFF_IDANGLE))
        {
            yaw += MOBJ_TO_ID(&mob) % 360;  // arbitrary
        }

        if(mf->testSubFlag(0, MFF_ALIGN_PITCH))
        {
            viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);
            Vector2d delta(vis->pose.midZ() - viewData->current.origin.z, distFromEye);

            pitch = -BANG2DEG(bamsAtan2(delta.x * 10, delta.y * 10));
        }
        else if(mf->testSubFlag(0, MFF_MOVEMENT_PITCH))
        {
            pitch = R_MovementXYZPitch(mob.mom[0], mob.mom[1], mob.mom[2]);
        }
        else
        {
            pitch = 0;
        }
    }

    // Determine possible short-range visual offset.
    Vector3d visOff;
    if((hasModel && useSRVO > 0) || (!hasModel && useSRVO > 1))
    {
        if(mob.tics >= 0)
        {
            visOff = Vector3d(mob.srvo) * (mob.tics - frameTimePos) / (float) mob.state->tics;
        }

        if(!INRANGE_OF(mob.mom[0], 0, NOMOMENTUM_THRESHOLD) ||
           !INRANGE_OF(mob.mom[1], 0, NOMOMENTUM_THRESHOLD) ||
           !INRANGE_OF(mob.mom[2], 0, NOMOMENTUM_THRESHOLD))
        {
            // Use the object's speed to calculate a short-range offset.
            visOff += Vector3d(mob.mom) * frameTimePos;
        }
    }

    // Will it be drawn as a 2D sprite?
    if(!hasModel)
    {
        bool const brightShadow = (mob.ddFlags & DDMF_BRIGHTSHADOW) != 0;
        bool const fitTop       = (mob.ddFlags & DDMF_FITTOP)       != 0;
        bool const fitBottom    = (mob.ddFlags & DDMF_NOFITBOTTOM)  == 0;

        // Additive blending?
        blendmode_t blendMode;
        if(brightShadow)
        {
            blendMode = BM_ADD;
        }
        // Use the "no translucency" blending mode?
        else if(noSpriteTrans && alpha >= .98f)
        {
            blendMode = BM_ZEROALPHA;
        }
        else
        {
            blendMode = BM_NORMAL;
        }

        // We must find the correct positioning using the sector floor
        // and ceiling heights as an aid.
        if(matDimensions.y < ceiling.heightSmoothed() - floor.heightSmoothed())
        {
            // Sprite fits in, adjustment possible?
            if(fitTop && topZ > ceiling.heightSmoothed())
                topZ = ceiling.heightSmoothed();

            if(floorAdjust && fitBottom && topZ - matDimensions.y < floor.heightSmoothed())
                topZ = floor.heightSmoothed() + matDimensions.y;
        }
        // Adjust by the floor clip.
        topZ -= floorClip;

        Vector3d const origin(vis->pose.origin.x, vis->pose.origin.y, topZ - matDimensions.y / 2.0f);
        Vector4f ambientColor;
        duint vLightListIdx = 0;
        evaluateLighting(origin, subspace, vis->pose.distance, fullbright,
                         ambientColor, &vLightListIdx);

        // Apply uniform alpha (overwritting intensity factor).
        ambientColor.w = alpha;

        VisSprite_SetupSprite(vis,
                              VisEntityPose(origin, visOff, viewAlign),
                              VisEntityLighting(ambientColor, vLightListIdx),
                              floor.heightSmoothed(), ceiling.heightSmoothed(),
                              floorClip, topZ, *mat, matFlipS, matFlipT, blendMode,
                              mob.tclass, mob.tmap,
                              &Mobj_BspLeafAtOrigin(mob),
                              floorAdjust, fitTop, fitBottom);
    }
    else // It will be drawn as a 3D model.
    {
        Vector4f ambientColor;
        duint vLightListIdx = 0;
        evaluateLighting(vis->pose.origin, subspace, vis->pose.distance,
                         fullbright, ambientColor, &vLightListIdx);

        // Apply uniform alpha (overwritting intensity factor).
        ambientColor.w = alpha;

        if(animator)
        {
            // Set up a GL2 model for drawing.
            vis->pose = VisEntityPose(vis->pose.origin,
                                      Vector3d(visOff.x, visOff.y, visOff.z - floorClip),
                                      viewAlign, topZ, yaw, 0, pitch, 0);
            vis->light = VisEntityLighting(ambientColor, vLightListIdx);

            vis->data.model2.object   = &mob;
            vis->data.model2.animator = animator;
            vis->data.model2.model    = &animator->model();
        }
        else
        {
            DENG2_ASSERT(mf);
            VisSprite_SetupModel(vis,
                                 VisEntityPose(vis->pose.origin,
                                               Vector3d(visOff.x, visOff.y, visOff.z - floorClip),
                                               viewAlign, topZ, yaw, 0, pitch, 0),
                                 VisEntityLighting(ambientColor, vLightListIdx),
                                 mf, nextmf, interp,
                                 mob.thinker.id, mob.selector,
                                 &Mobj_BspLeafAtOrigin(mob),
                                 mob.ddFlags, mob.tmap,
                                 fullbright && !(mf && mf->testSubFlag(0, MFF_DIM)), false);
        }
    }

    // Do we need to project a flare source too?
    if(mob.lumIdx != Lumobj::NoIndex && haloMode > 0)
    {
        /// @todo mark this light source visible for LensFx
        try
        {
            SpriteViewAngle const &sprViewAngle =
                sprite->closestViewAngle(mob.angle, R_ViewPointToAngle(mob.origin));

            DENG2_ASSERT(sprViewAngle.material);
            MaterialAnimator &matAnimator = sprViewAngle.material->getAnimator(Rend_SpriteMaterialSpec(mob.tclass, mob.tmap));

            // Ensure we've up to date info about the material.
            matAnimator.prepare();

            Vector2i const &matDimensions = matAnimator.dimensions();
            TextureVariant *tex           = matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture;

            // A valid sprite texture in the "Sprites" scheme is required.
            if(!tex || tex->base().manifest().schemeName().compareWithoutCase("Sprites"))
            {
                return;
            }

            auto const *pl = (pointlight_analysis_t const *) tex->base().analysisDataPointer(Texture::BrightPointAnalysis);
            DENG2_ASSERT(pl);

            Lumobj const &lob = cluster.sector().map().lumobj(mob.lumIdx);
            vissprite_t *vis  = R_NewVisSprite(VSPR_FLARE);

            vis->pose.distance = distFromEye;

            // Determine the exact center of the flare.
            vis->pose.origin = moPos + visOff;
            vis->pose.origin.z += lob.zOffset();

            dfloat flareSize = pl->brightMul;
            // X offset to the flare position.
            dfloat xOffset = matDimensions.x * pl->originX - -tex->base().origin().x;

            // Does the mobj have an active light definition?
            ded_light_t const *def = (mob.state? runtimeDefs.stateInfo[runtimeDefs.states.indexOf(mob.state)].light : 0);
            if(def)
            {
                if(def->size)
                    flareSize = def->size;
                if(def->haloRadius)
                    flareSize = def->haloRadius;
                if(def->offset[0])
                    xOffset = def->offset[0];

                vis->data.flare.flags = def->flags;
            }

            vis->data.flare.size = flareSize * 60 * (50 + haloSize) / 100.0f;
            if(vis->data.flare.size < 8)
                vis->data.flare.size = 8;

            // Color is taken from the associated lumobj.
            V3f_Set(vis->data.flare.color, lob.color().x, lob.color().y, lob.color().z);

            vis->data.flare.factor = mob.haloFactors[viewPlayer - ddPlayers];
            vis->data.flare.xOff = xOffset;
            vis->data.flare.mul = 1;
            vis->data.flare.tex = 0;

            if(def && def->flare)
            {
                de::Uri const &flaremapResourceUri = *def->flare;
                if(flaremapResourceUri.path().toStringRef().compareWithoutCase("-"))
                {
                    vis->data.flare.tex = GL_PrepareFlaremap(flaremapResourceUri);
                }
            }
        }
        catch(Sprite::MissingViewAngleError const &er)
        {
            // Log but otherwise ignore this error.
            LOG_GL_WARNING("Projecting flare source for sprite '%i' frame '%i': %s")
                    << mob.sprite << mob.frame << er.asText();
        }
    }
}
