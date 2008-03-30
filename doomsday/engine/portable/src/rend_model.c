/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * rend_model.c: 3D Model Renderer v2.0
 *
 * Note: Light vectors and triangle normals are considered to be
 * in a totally independent, right-handed coordinate system.
 *
 * There is some more confusion with Y and Z axes as the game uses
 * Z as the vertical axis and the rendering code and model definitions
 * use the Y axis.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <math.h>

#include "de_base.h"
#include "de_dgl.h"
#include "de_console.h"
#include "de_render.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_misc.h"

#include "net_main.h"           // for gametic

// MACROS ------------------------------------------------------------------

#define MAX_VERTS           4096    // Maximum number of vertices per model.
#define DOTPROD(a, b)       (a[0]*b[0] + a[1]*b[1] + a[2]*b[2])
#define QATAN2(y,x)         qatan2(y,x)
#define QASIN(x)            asin(x) // \fixme Precalculate arcsin.

// TYPES -------------------------------------------------------------------

typedef enum rendcmd_e {
    RC_COMMAND_COORDS,
    RC_OTHER_COORDS,
    RC_BOTH_COORDS
} rendcmd_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     modelLight = 4;
int     frameInter = true;
int     mirrorHudModels = false;
int     modelShinyMultitex = true;
float   modelShinyFactor = 1.0f;
int     modelTriCount;
float   rend_model_lod = 256;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Fixed-size vertex arrays for the model.
static gl_vertex_t modelVertices[MAX_VERTS];
static gl_vertex_t modelNormals[MAX_VERTS];
static gl_color_t modelColors[MAX_VERTS];
static gl_texcoord_t modelTexCoords[MAX_VERTS];

// Global variables for ease of use. (Egads!)
static float modelCenter[3];
static int activeLod;
static char *vertexUsage;

// CODE --------------------------------------------------------------------

void Rend_ModelRegister(void)
{
    C_VAR_INT("rend-model", &useModels, CVF_NO_MAX, 0, 1);
    C_VAR_INT("rend-model-lights", &modelLight, 0, 0, 10);
    C_VAR_INT("rend-model-inter", &frameInter, 0, 0, 1);
    C_VAR_FLOAT("rend-model-aspect", &rModelAspectMod,
                CVF_NO_MAX | CVF_NO_MIN, 0, 0);
    C_VAR_INT("rend-model-distance", &maxModelDistance, CVF_NO_MAX, 0, 0);
    C_VAR_BYTE("rend-model-precache", &precacheSkins, 0, 0, 1);
    C_VAR_FLOAT("rend-model-lod", &rend_model_lod, CVF_NO_MAX, 0, 0);
    C_VAR_INT("rend-model-mirror-hud", &mirrorHudModels, 0, 0, 1);
    C_VAR_FLOAT("rend-model-spin-speed", &modelSpinSpeed,
                CVF_NO_MAX | CVF_NO_MIN, 0, 0);
    C_VAR_INT("rend-model-shiny-multitex", &modelShinyMultitex, 0, 0, 1);
    C_VAR_FLOAT("rend-model-shiny-strength", &modelShinyFactor, 0, 0, 10);
}

static __inline float qatan2(float y, float x)
{
    float       ang = BANG2RAD(bamsAtan2(y * 512, x * 512));

    if(ang > PI)
        ang -= 2 * (float) PI;
    return ang;

    // This is slightly faster, I believe...
    //return atan2(y, x);
}

/**
 * Linear interpolation between two values.
 */
__inline float Mod_Lerp(float start, float end, float pos)
{
    return end * pos + start * (1 - pos);
}

/**
 * Return a pointer to the visible model frame.
 */
model_frame_t *Mod_GetVisibleFrame(modeldef_t *mf, int subnumber, int mobjid)
{
    model_t    *mdl = modellist[mf->sub[subnumber].model];
    int         index = mf->sub[subnumber].frame;

    if(mf->flags & MFF_IDFRAME)
    {
        index += mobjid % mf->sub[subnumber].frameRange;
    }
    if(index >= mdl->info.numFrames)
    {
        Con_Error("Mod_GetVisibleFrame: Frame index out of bounds.\n"
                  "  (Model: %s)\n", mdl->fileName);
    }
    return mdl->frames + index;
}

/**
 * Render a set of GL commands using the given data.
 */
void Mod_RenderCommands(rendcmd_t mode, void *glCommands, /*uint numVertices,*/
                        gl_vertex_t *vertices, gl_color_t *colors,
                        gl_texcoord_t *texCoords)
{
    byte       *pos;
    glcommand_vertex_t *v;
    int         count;
    void       *coords[2];

    // Disable all vertex arrays.
    DGL_DisableArrays(true, true, DGL_ALL_BITS);

    // Load the vertex array.
    switch(mode)
    {
    case RC_OTHER_COORDS:
        coords[0] = texCoords;
        DGL_Arrays(vertices, colors, 1, coords, 0);
        break;

    case RC_BOTH_COORDS:
        coords[0] = NULL;
        coords[1] = texCoords;
        DGL_Arrays(vertices, colors, 2, coords, 0);
        break;

    default:
        DGL_Arrays(vertices, colors, 0, NULL, 0 /* numVertices */ );
        break;
    }

    for(pos = glCommands; *pos;)
    {
        count = LONG( *(int *) pos );
        pos += 4;

        // The type of primitive depends on the sign.
        DGL_Begin(count > 0 ? DGL_TRIANGLE_STRIP : DGL_TRIANGLE_FAN);
        if(count < 0)
            count = -count;

        // Increment the total model triangle counter.
        modelTriCount += count - 2;

        while(count--)
        {
            v = (glcommand_vertex_t *) pos;
            pos += sizeof(glcommand_vertex_t);

            if(mode != RC_OTHER_COORDS)
            {
                DGL_TexCoord2f(FLOAT(v->s), FLOAT(v->t));
            }

            DGL_ArrayElement(LONG(v->index));
        }

        // The primitive is complete.
        DGL_End();
    }
}

/**
 * Interpolate linearly between two sets of vertices.
 */
void Mod_LerpVertices(float pos, int count, model_vertex_t *start,
                      model_vertex_t *end, gl_vertex_t *out)
{
    int         i;
    float       inv;

    if(start == end || pos == 0)
    {
        for(i = 0; i < count; ++i, start++, out++)
        {
            out->xyz[0] = start->xyz[0];
            out->xyz[1] = start->xyz[1];
            out->xyz[2] = start->xyz[2];
        }
        return;
    }

    inv = 1 - pos;

    if(vertexUsage)
    {
        for(i = 0; i < count; ++i, start++, end++, out++)
            if(vertexUsage[i] & (1 << activeLod))
            {
                out->xyz[0] = inv * start->xyz[0] + pos * end->xyz[0];
                out->xyz[1] = inv * start->xyz[1] + pos * end->xyz[1];
                out->xyz[2] = inv * start->xyz[2] + pos * end->xyz[2];
            }
    }
    else
    {
        for(i = 0; i < count; ++i, start++, end++, out++)
        {
            out->xyz[0] = inv * start->xyz[0] + pos * end->xyz[0];
            out->xyz[1] = inv * start->xyz[1] + pos * end->xyz[1];
            out->xyz[2] = inv * start->xyz[2] + pos * end->xyz[2];
        }
    }
}

/**
 * Negate all Z coordinates.
 */
void Mod_MirrorVertices(int count, gl_vertex_t *v, int axis)
{
    for(; count-- > 0; v++)
        v->xyz[axis] = -v->xyz[axis];
}

/**
 * Calculate vertex lighting.
 */
void Mod_VertexColors(int count, gl_color_t *out, gl_vertex_t *normal,
                      int numLights, vlight_t *lights, float ambient[4])
{
    int         i, k;
    float       color[3], extra[3], *dest, dot;
    vlight_t   *light;

    for(i = 0; i < count; ++i, out++, normal++)
    {
        if(vertexUsage && !(vertexUsage[i] & (1 << activeLod)))
            continue;

        // Begin with total darkness.
        memset(color, 0, sizeof(color));
        memset(extra, 0, sizeof(extra));

        // Add light from each source.
        for(k = 0, light = lights; k < numLights; ++k, light++)
        {
            if(!light->used)
                continue;

            dot = DOTPROD(light->vector, normal->xyz);
            //dot *= 1.2f; // Looks-good factor :-).
            if(light->lum)
            {
                dest = color;
            }
            else
            {
                // This is world light (won't be affected by ambient).
                // Ability to both light and shade.
                dest = extra;
                dot += light->offset;   // Shift a bit towards the light.
                if(dot > 0)
                    dot *= light->lightSide;
                else
                    dot *= light->darkSide;
            }
            // No light from the wrong side.
            if(dot <= 0)
            {
                // Lights with a source won't shade anything.
                if(light->lum)
                    continue;
                if(dot < -1)
                    dot = -1;
            }
            else
            {
                if(dot > 1)
                    dot = 1;
            }

            dest[0] += dot * light->color[0];
            dest[1] += dot * light->color[1];
            dest[2] += dot * light->color[2];
        }

        // Check for ambient and convert to ubyte.
        for(k = 0; k < 3; ++k)
        {
            if(color[k] < ambient[k])
                color[k] = ambient[k];
            color[k] += extra[k];
            if(color[k] < 0)
                color[k] = 0;
            if(color[k] > 1)
                color[k] = 1;

            // This is the final color.
            out->rgba[k] = (byte) (255 * color[k]);
        }
        out->rgba[CA] = (byte) (255 * ambient[CA]);
    }
}

/**
 * Set all the colors in the array to bright white.
 */
void Mod_FullBrightVertexColors(int count, gl_color_t *colors, float alpha)
{
    for(; count-- > 0; colors++)
    {
        memset(colors->rgba, 255, 3);
        colors->rgba[3] = (byte) (255 * alpha);
    }
}

/**
 * Set all the colors into the array to the same values.
 */
void Mod_FixedVertexColors(int count, gl_color_t *colors, float *color)
{
    byte        rgba[4];

    rgba[0] = color[0] * 255;
    rgba[1] = color[1] * 255;
    rgba[2] = color[2] * 255;
    rgba[3] = color[3] * 255;
    for(; count-- > 0; colors++)
        memcpy(colors->rgba, rgba, 4);
}

/**
 * Calculate cylindrically mapped, shiny texture coordinates.
 */
void Mod_ShinyCoords(int count, gl_texcoord_t *coords, gl_vertex_t *normals,
                     float normYaw, float normPitch, float shinyAng,
                     float shinyPnt, float reactSpeed)
{
    int         i;
    float       u, v;
    float       rotatedNormal[3];

    for(i = 0; i < count; ++i, coords++, normals++)
    {
        if(vertexUsage && !(vertexUsage[i] & (1 << activeLod)))
            continue;

        rotatedNormal[VX] = normals->xyz[VX];
        rotatedNormal[VY] = normals->xyz[VY];
        rotatedNormal[VZ] = normals->xyz[VZ];

        // Rotate the normal vector so that it approximates the
        // model's orientation compared to the viewer.
        M_RotateVector(rotatedNormal,
                       (shinyPnt + normYaw) * 360 * reactSpeed,
                       (shinyAng + normPitch - .5f) * 180 * reactSpeed);

        u = rotatedNormal[VX] + 1;
        v = rotatedNormal[VZ];

        coords->st[0] = u;
        coords->st[1] = v;
    }
}

/**
 * Render a submodel from the vissprite.
 */
static void Mod_RenderSubModel(uint number, const modelparams_t *params)
{
    modeldef_t *mf = params->mf, *mfNext = params->nextMF;
    submodeldef_t *smf = &mf->sub[number];
    model_t    *mdl = modellist[smf->model];
    model_frame_t *frame = Mod_GetVisibleFrame(mf, number, params->id);
    model_frame_t *nextFrame = NULL;

    //int mainFlags = mf->flags;
    int         subFlags = smf->flags;
    int         numVerts;
    int         useSkin;
    int         i, c;
    float       endPos, offset;
    float       alpha, customAlpha;
    float       delta[3], color[4];
    float       ambient[4];
    float       shininess, *shinyColor;
    float       normYaw, normPitch, shinyAng, shinyPnt;
    float       inter = params->inter;
    blendmode_t blending = mf->def->sub[number].blendMode;
    DGLuint     skinTexture = 0, shinyTexture = 0;
    int         zSign = (params->mirror? -1 : 1);

    if(mf->scale[VX] == 0 && mf->scale[VY] == 0 && mf->scale[VZ] == 0)
    {
        // Why bother? It's infinitely small...
        return;
    }

    // Submodel can define a custom Transparency level.
    customAlpha = 1 - smf->alpha / 255.0f;

    if(missileBlend &&
       ((params->flags & DDMF_BRIGHTSHADOW) || (subFlags & MFF_BRIGHTSHADOW)))
    {
        alpha = .80f;
        blending = BM_ADD;
    }
    else if(subFlags & MFF_BRIGHTSHADOW2)
    {
        alpha = customAlpha;
        blending = BM_ADD;
    }
    else if(subFlags & MFF_DARKSHADOW)
    {
        alpha = customAlpha;
        blending = BM_DARK;
    }
    else if((params->flags & DDMF_SHADOW) || (subFlags & MFF_SHADOW2))
        alpha = .2f;
    else if((params->flags & DDMF_ALTSHADOW) || (subFlags & MFF_SHADOW1))
        alpha = .62f;
    else
        alpha = customAlpha;

    // More custom alpha?
    if(params->ambientColor[CA] >= 0)
        alpha *= params->ambientColor[CA];
    if(alpha <= 0)
        return; // Fully transparent.
    if(alpha > 1)
        alpha = 1;

    // Extra blending modes.
    if(subFlags & MFF_SUBTRACT)
        blending = BM_SUBTRACT;
    if(subFlags & MFF_REVERSE_SUBTRACT)
        blending = BM_REVERSE_SUBTRACT;

    useSkin = smf->skin;

    // Selskin overrides the skin range.
    if(subFlags & MFF_SELSKIN)
    {
        i = (params->selector >> DDMOBJ_SELECTOR_SHIFT) &
            mf->def->sub[number].selSkinBits[0]; // Selskin mask
        c = mf->def->sub[number].selSkinBits[1]; // Selskin shift
        if(c > 0)
            i >>= c;
        else
            i <<= -c;
        if(i > 7)
            i = 7; // Maximum number of skins for selskin.
        if(i < 0)
            i = 0; // Improbable (impossible?), but doesn't hurt.
        useSkin = mf->def->sub[number].selSkins[i];
    }

    // Is there a skin range for this frame?
    // (During model setup skintics and skinrange are set to >0.)
    if(smf->skinRange > 1)
    {
        // What rule to use for determining the skin?
        useSkin +=
            (subFlags & MFF_IDSKIN ? params->id : SECONDS_TO_TICKS(gameTime) / mf->skinTics) % smf->skinRange;
    }

    // Scale interpos. Intermark becomes zero and endmark becomes one.
    // (Full sub-interpolation!) But only do it for the standard
    // interrange. If a custom one is defined, don't touch interpos.
    if((mf->interRange[0] == 0 && mf->interRange[1] == 1) ||
       (subFlags & MFF_WORLD_TIME_ANIM))
    {
        endPos = (mf->interNext ? mf->interNext->interMark : 1);
        inter = (params->inter - mf->interMark) / (endPos - mf->interMark);
    }

    // Do we have a sky/particle model here?
    if(params->alwaysInterpolate)
    {
        // Always interpolate, if there's animation.
        // Used with sky and particle models.
        nextFrame = mdl->frames + (smf->frame + 1) % mdl->info.numFrames;
        mfNext = mf;
    }
    else
    {
        // Check for possible interpolation.
        if(frameInter && mfNext && !(subFlags & MFF_DONT_INTERPOLATE))
        {
            if(mfNext->sub[number].model == smf->model)
            {
                nextFrame = Mod_GetVisibleFrame(mfNext, number, params->id);
            }
        }
    }

    // Need translation?
    if(subFlags & MFF_SKINTRANS)
        useSkin = (params->flags & DDMF_TRANSLATION) >> DDMF_TRANSSHIFT;

    // Clamp interpolation.
    if(inter < 0)
        inter = 0;
    if(inter > 1)
        inter = 1;

    if(!nextFrame)
    {
        // If not interpolating, use the same frame as interpolation target.
        // The lerp routines will recognize this special case.
        nextFrame = frame;
        mfNext = mf;
    }

    // Setup transformation.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    // Model space => World space
    DGL_Translatef(params->center[VX] + params->srvo[VX] +
                    Mod_Lerp(mf->offset[VX], mfNext->offset[VX], inter),
                  params->center[VZ] + params->srvo[VZ] +
                    Mod_Lerp(mf->offset[VY], mfNext->offset[VY], inter),
                  params->center[VY] + params->srvo[VY] + zSign *
                    Mod_Lerp(mf->offset[VZ], mfNext->offset[VZ], inter));

    if(params->extraYawAngle || params->extraPitchAngle)
    {
        // Sky models have an extra rotation.
        DGL_Scalef(1, 200 / 240.0f, 1);
        DGL_Rotatef(params->extraYawAngle, 1, 0, 0);
        DGL_Rotatef(params->extraPitchAngle, 0, 0, 1);
        DGL_Scalef(1, 240 / 200.0f, 1);
    }

    // Model rotation.
    DGL_Rotatef(params->viewAligned ? params->yawAngleOffset : params->yaw,
               0, 1, 0);
    DGL_Rotatef(params->viewAligned ? params->pitchAngleOffset : params->pitch,
               0, 0, 1);

    // Scaling and model space offset.
    DGL_Scalef(Mod_Lerp(mf->scale[VX], mfNext->scale[VX], inter),
              Mod_Lerp(mf->scale[VY], mfNext->scale[VY], inter),
              Mod_Lerp(mf->scale[VZ], mfNext->scale[VZ], inter));
    if(params->extraScale)
    {
        // Particle models have an extra scale.
        DGL_Scalef(params->extraScale, params->extraScale, params->extraScale);
    }
    DGL_Translatef(smf->offset[VX], smf->offset[VY], smf->offset[VZ]);

    // Now we can draw.
    numVerts = mdl->info.numVertices;

    // Determine the suitable LOD.
    if(mdl->info.numLODs > 1 && rend_model_lod != 0)
    {
        float   lodFactor;

        lodFactor = rend_model_lod * theWindow->width / 640.0f / (fieldOfView / 90.0f);
        if(lodFactor)
            lodFactor = 1 / lodFactor;

        // Determine the LOD we will be using.
        activeLod = (int) (lodFactor * params->distance);
        if(activeLod < 0)
            activeLod = 0;
        if(activeLod >= mdl->info.numLODs)
            activeLod = mdl->info.numLODs - 1;
        vertexUsage = mdl->vertexUsage;
    }
    else
    {
        activeLod = 0;
        vertexUsage = NULL;
    }

    // Interpolate vertices and normals.
    Mod_LerpVertices(inter, numVerts, frame->vertices, nextFrame->vertices,
                     modelVertices);
    Mod_LerpVertices(inter, numVerts, frame->normals, nextFrame->normals,
                     modelNormals);
    if(zSign < 0)
    {
        Mod_MirrorVertices(numVerts, modelVertices, VZ);
        Mod_MirrorVertices(numVerts, modelNormals, VY);
    }

    // Coordinates to the center of the model (game coords).
    modelCenter[VX] = params->center[VX] + params->srvo[VX] + mf->offset[VX];
    modelCenter[VY] = params->center[VY] + params->srvo[VY] + mf->offset[VZ];
    modelCenter[VZ] = ((params->center[VZ] + params->gzt) * 2) + params->srvo[VZ] +
                        mf->offset[VY];

    // Calculate lighting.
    if(params->uniformColor)
    {   // Specified uniform color.
        ambient[CR] = params->ambientColor[CR];
        ambient[CG] = params->ambientColor[CG];
        ambient[CB] = params->ambientColor[CB];
        ambient[CA] = alpha;
        Mod_FixedVertexColors(numVerts, modelColors, ambient);
    }
    else if((subFlags & MFF_FULLBRIGHT) && !(subFlags & MFF_DIM))
    {   // Fullbright white.
        ambient[CR] = ambient[CG] = ambient[CB] = ambient[CA] = 1;
        Mod_FullBrightVertexColors(numVerts, modelColors, alpha);
    }
    else
    {   // Lit normally.
        uint            i;
        vlight_t       *light;

        ambient[CR] = params->ambientColor[CR];
        ambient[CG] = params->ambientColor[CG];
        ambient[CB] = params->ambientColor[CB];
        ambient[CA] = alpha;

        // We need to make some adjustments to the light vectors.
        for(i = 0; i < params->numLights; ++i)
        {
            light = &params->lights[i];

            // We must transform the light vector to model space.
            M_RotateVector(light->vector, -params->yaw, -params->pitch);
            // Quick hack: Flip light normal if model inverted.
            if(mf->scale[VY] < 0)
            {
                light->vector[VX] = -light->vector[VX];
                light->vector[VY] = -light->vector[VY];
            }
        }

        Mod_VertexColors(numVerts, modelColors, modelNormals,
                         params->numLights, params->lights, ambient);
    }

    // Calculate shiny coordinates.
    shininess = mf->def->sub[number].shiny * modelShinyFactor;
    if(shininess < 0)
        shininess = 0;
    if(shininess > 1)
        shininess = 1;

    if(shininess > 0)
    {
        shinyColor = mf->def->sub[number].shinyColor;

        // With psprites, add the view angle/pitch.
        offset = params->shineYawOffset;

        // Calculate normalized (0,1) model yaw and pitch.
        normYaw =
            M_CycleIntoRange(((params->viewAligned ? params->yawAngleOffset :
                                params->yaw) + offset) / 360, 1);

        offset = params->shinePitchOffset;

        normPitch =
            M_CycleIntoRange(((params->viewAligned ? params->pitchAngleOffset :
                                params->pitch) + offset) / 360, 1);

        if(params->shinepspriteCoordSpace)
        {
            // This is a hack to accommodate the psprite coordinate space.
            shinyAng = 0;
            shinyPnt = 0.5;
        }
        else
        {
            delta[VX] = modelCenter[VX] - vx;
            delta[VY] = modelCenter[VY] - vz;
            delta[VZ] = modelCenter[VZ] - vy;

            if(params->shineTranslateWithViewerPos)
            {
                delta[VX] += vx;
                delta[VY] += vz;
                delta[VZ] += vy;
            }

            shinyAng = QATAN2(delta[VZ],
                              M_ApproxDistancef(delta[VX], delta[VY])) / PI
                + 0.5f; // shinyAng is [0,1]

            shinyPnt = QATAN2(delta[VY], delta[VX]) / (2 * PI);
        }

        Mod_ShinyCoords(numVerts, modelTexCoords, modelNormals, normYaw,
                        normPitch, shinyAng, shinyPnt,
                        mf->def->sub[number].shinyReact);

        // Shiny color.
        if(subFlags & MFF_SHINY_LIT)
        {
            for(c = 0; c < 3; ++c)
                color[c] = ambient[c] * shinyColor[c];
        }
        else
        {
            for(c = 0; c < 3; ++c)
                color[c] = shinyColor[c];
        }
        color[3] = shininess;

        shinyTexture = GL_PrepareShinySkin(R_GetSkinTexByIndex(mf->sub[number].shinySkin));
    }

    if(renderTextures == 2)
    {
        // For lighting debug, render all surfaces using the gray texture.
        skinTexture = GL_PrepareMaterial(R_GetMaterial(DDT_GRAY, MAT_DDTEX), NULL);
    }
    else
    {
        skintex_t          *st;

        if(useSkin < 0 || useSkin >= mdl->info.numSkins)
            useSkin = 0;

        st = R_GetSkinTexByIndex(mdl->skins[useSkin].id);
        skinTexture = GL_PrepareSkin(st, mdl->allowTexComp);
    }

    // If we mirror the model, triangles have a different orientation.
    if(zSign < 0)
    {
        glFrontFace(GL_CCW);
    }

    // Twosided models won't use backface culling.
    if(subFlags & MFF_TWO_SIDED)
        glDisable(GL_CULL_FACE);

    // Render using multiple passes?
    if(!modelShinyMultitex || shininess <= 0 || alpha < 1 ||
       blending != BM_NORMAL || !(subFlags & MFF_SHINY_SPECULAR) ||
       numTexUnits < 2 || !envModAdd)
    {
        // The first pass can be skipped if it won't be visible.
        if(shininess < 1 || subFlags & MFF_SHINY_SPECULAR)
        {
            RL_SelectTexUnits(1);
            GL_BlendMode(blending);
            RL_Bind(skinTexture);

            Mod_RenderCommands(RC_COMMAND_COORDS,
                               mdl->lods[activeLod].glCommands, /*numVerts,*/
                               modelVertices, modelColors, NULL);
        }

        if(shininess > 0)
        {
            glDepthFunc(GL_LEQUAL);

            // Set blending mode, two choices: reflected and specular.
            if(subFlags & MFF_SHINY_SPECULAR)
                GL_BlendMode(BM_ADD);
            else
                GL_BlendMode(BM_NORMAL);

            // Shiny color.
            Mod_FixedVertexColors(numVerts, modelColors, color);

            if(numTexUnits > 1 && modelShinyMultitex)
            {
                // We'll use multitexturing to clear out empty spots in
                // the primary texture.
                RL_SelectTexUnits(2);
                DGL_SetInteger(DGL_MODULATE_TEXTURE, 11);
                RL_BindTo(1, shinyTexture);
                RL_BindTo(0, skinTexture);

                Mod_RenderCommands(RC_BOTH_COORDS,
                                   mdl->lods[activeLod].glCommands, /*numVerts,*/
                                   modelVertices, modelColors, modelTexCoords);

                RL_SelectTexUnits(1);
                DGL_SetInteger(DGL_MODULATE_TEXTURE, 1);
            }
            else
            {
                // Empty spots will get shine, too.
                RL_SelectTexUnits(1);
                RL_Bind(shinyTexture);
                Mod_RenderCommands(RC_OTHER_COORDS,
                                   mdl->lods[activeLod].glCommands, /*numVerts,*/
                                   modelVertices, modelColors, modelTexCoords);
            }
        }
    }
    else
    {
        // A special case: specular shininess on an opaque object.
        // Multitextured shininess with the normal blending.
        GL_BlendMode(blending);
        RL_SelectTexUnits(2);
        // Tex1*Color + Tex2RGB*ConstRGB
        DGL_SetInteger(DGL_MODULATE_TEXTURE, 10);
        RL_BindTo(1, shinyTexture);
        // Multiply by shininess.
        for(c = 0; c < 3; ++c)
            color[c] *= color[3];
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
        RL_BindTo(0, skinTexture);

        Mod_RenderCommands(RC_BOTH_COORDS, mdl->lods[activeLod].glCommands,
                           /*numVerts,*/ modelVertices, modelColors,
                           modelTexCoords);

        RL_SelectTexUnits(1);
        DGL_SetInteger(DGL_MODULATE_TEXTURE, 1);
    }

    // We're done!
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    // Normally culling is always enabled.
    if(subFlags & MFF_TWO_SIDED)
        glEnable(GL_CULL_FACE);

    if(zSign < 0)
    {
        glFrontFace(GL_CW);
    }
    glDepthFunc(GL_LESS);

    GL_BlendMode(BM_NORMAL);
}

/**
 * Render all the submodels of a model.
 */
void Rend_RenderModel(const modelparams_t *params)
{
    int         i;

    if(!params || !params->mf)
        return;

    // Render all the submodels of this model.
    for(i = 0; i < MAX_FRAME_MODELS; ++i)
    {
        if(params->mf->sub[i].model)
        {
            boolean disableZ = (params->mf->flags & MFF_DISABLE_Z_WRITE ||
                                params->mf->sub[i].flags & MFF_DISABLE_Z_WRITE);

            if(disableZ)
                glDepthMask(GL_FALSE);

            // Render the submodel.
            Mod_RenderSubModel(i, params);

            if(disableZ)
                glDepthMask(GL_TRUE);
        }
    }
}
