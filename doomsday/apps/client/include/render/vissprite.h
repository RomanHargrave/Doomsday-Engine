/** @file vissprite.h  Projected visible sprite ("vissprite") management.
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

#ifdef __CLIENT__
#ifndef DENG_CLIENT_RENDER_VISSPRITE_H
#define DENG_CLIENT_RENDER_VISSPRITE_H

#include <de/Vector>

#include "render/billboard.h"
#include "rend_model.h"

#define MAXVISSPRITES   8192

// These constants are used as the type of vissprite_s.
typedef enum {
    VSPR_SPRITE,
    VSPR_MASKED_WALL,
    VSPR_MODEL,
    VSPR_MODEL_GL2,     ///< GL2 model (de::ModelDrawable)
    VSPR_FLARE
} visspritetype_t;

#define MAX_VISSPRITE_LIGHTS    (10)

struct VisEntityPose
{
    de::Vector3d    origin;
    float           topZ; ///< Global top Z coordinate (origin Z is the bottom).
    de::Vector3d    srvo; ///< Short-range visual offset.
    coord_t         distance; ///< Distance from viewer.
    float           yaw;
    float           extraYawAngle;
    float           yawAngleOffset; ///< @todo We do not need three sets of angles...
    float           pitch;
    float           extraPitchAngle;
    float           pitchAngleOffset;
    float           extraScale;
    bool            viewAligned;
    bool            mirrored; ///< If true the model will be mirrored about its Z axis (in model space).

    VisEntityPose() { de::zap(*this); }

    VisEntityPose(de::Vector3d const &origin_,
                  de::Vector3d const &visOffset,
                  bool viewAlign_ = false,
                  float topZ_ = 0,
                  float yaw_ = 0,
                  float yawAngleOffset_ = 0,
                  float pitch_ = 0,
                  float pitchAngleOffset_= 0)
        : origin(origin_)
        , topZ(topZ_)
        , srvo(visOffset)
        , distance(Rend_PointDist2D(origin_))
        , yaw(yaw_)
        , extraYawAngle(0)
        , yawAngleOffset(yawAngleOffset_)
        , pitch(pitch_)
        , extraPitchAngle(0)
        , pitchAngleOffset(pitchAngleOffset_)
        , extraScale(0)
        , viewAligned(viewAlign_)
        , mirrored(false)
    {}

    inline coord_t midZ() const { return (origin.z + topZ) / 2; }

    de::Vector3d mid() const { return de::Vector3d(origin.x, origin.y, midZ()); }
};

struct VisEntityLighting
{
    de::Vector4f ambientColor;
    uint  vLightListIdx;

    VisEntityLighting() { de::zap(*this); }

    VisEntityLighting(de::Vector4f const &ambientColor_,
                      uint lightListIndex)
        : vLightListIdx(lightListIndex)
    {
        ambientColor = ambientColor_;
    }
};

/**
 * vissprite_t is a mobj or masked wall that will be drawn refresh.
 */
typedef struct vissprite_s {
    struct vissprite_s *prev, *next;
    visspritetype_t type; // VSPR_* type of vissprite.
    //coord_t distance; // Vissprites are sorted by distance.
    //de::Vector3d origin;

    VisEntityPose pose;
    VisEntityLighting light;

    // An anonymous union for the data.
    union vissprite_data_u {
        drawspriteparams_t sprite;
        drawmaskedwallparams_t wall;
        drawmodelparams_t model;
        drawmodel2params_t model2;
        drawflareparams_t flare;
    } data;
} vissprite_t;

#define VS_SPRITE(v)        (&((v)->data.sprite))
#define VS_WALL(v)          (&((v)->data.wall))
#define VS_MODEL(v)         (&((v)->data.model))
#define VS_MODEL2(v)        (&((v)->data.model2))
#define VS_FLARE(v)         (&((v)->data.flare))

void VisSprite_SetupSprite(vissprite_t *spr,
                           VisEntityPose const &pose,
                           VisEntityLighting const &light,
    float /*secFloor*/, float /*secCeil*/, float /*floorClip*/, float /*top*/,
    Material &material, bool matFlipS, bool matFlipT, blendmode_t blendMode,
    int tClass, int tMap, BspLeaf *bspLeafAtOrigin,
    bool /*floorAdjust*/, bool /*fitTop*/, bool /*fitBottom*/);

void VisSprite_SetupModel(vissprite_t *spr,
                          VisEntityPose const &pose,
                          VisEntityLighting const &light,
    ModelDef *mf, ModelDef *nextMF, float inter,
    int id, int selector, BspLeaf * /*bspLeafAtOrigin*/, int mobjDDFlags, int tmap,
    bool /*fullBright*/, bool alwaysInterpolate);

typedef enum {
    VPSPR_SPRITE,
    VPSPR_MODEL
} vispspritetype_t;

typedef struct vispsprite_s {
    vispspritetype_t type;
    ddpsprite_t *psp;
    coord_t origin[3];

    union vispsprite_data_u {
        struct vispsprite_sprite_s {
            BspLeaf *bspLeaf;
            float alpha;
            dd_bool isFullBright;
        } sprite;
        struct vispsprite_model_s {
            BspLeaf *bspLeaf;
            coord_t topZ; // global top for silhouette clipping
            int flags; // for color translation and shadow draw
            uint id;
            int selector;
            int pClass; // player class (used in translation)
            coord_t floorClip;
            dd_bool stateFullBright;
            dd_bool viewAligned;    // Align to view plane.
            coord_t secFloor, secCeil;
            float alpha;
            coord_t visOff[3]; // Last-minute offset to coords.
            dd_bool floorAdjust; // Allow moving sprite to match visible floor.

            ModelDef *mf, *nextMF;
            float yaw, pitch;
            float pitchAngleOffset;
            float yawAngleOffset;
            float inter; // Frame interpolation, 0..1
        } model;
    } data;
} vispsprite_t;

DENG_EXTERN_C vissprite_t visSprites[MAXVISSPRITES], *visSpriteP;
DENG_EXTERN_C vissprite_t visSprSortedHead;
DENG_EXTERN_C vispsprite_t visPSprites[DDMAXPSPRITES];

/// To be called at the start of the current render frame to clear the vissprite list.
void R_ClearVisSprites();

vissprite_t *R_NewVisSprite(visspritetype_t type);

void R_SortVisSprites();

#endif // DENG_CLIENT_RENDER_VISSPRITE_H
#endif // __CLIENT__