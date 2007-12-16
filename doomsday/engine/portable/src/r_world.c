/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
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
 * r_world.c: World Setup and Refresh
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include <assert.h>
#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// $smoothplane: Maximum speed for a smoothed plane.
#define MAX_SMOOTH_PLANE_MOVE   (64)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     rendSkyLight = 1;       // cvar

boolean firstFrameAfterLoad;
boolean levelSetup;

nodeindex_t     *linelinks;         // indices to roots

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean noSkyColorGiven;
static float skyColorRGB[4], balancedRGB[4];
static float skyColorBalance;

static vertex_t *rootVtx; // Used when sorting vertex line owners.

// CODE --------------------------------------------------------------------

void R_AddWatchedPlane(watchedplanelist_t *wpl, plane_t *pln)
{
    uint                i;

    if(!wpl || !pln)
        return;

    // Check whether we are already tracking this plane.
    for(i = 0; i < wpl->num; ++i)
        if(wpl->list[i] == pln)
            return; // Yes we are.

    wpl->num++;

    // Only allocate memory when it's needed.
    if(wpl->num > wpl->maxnum)
    {
        wpl->maxnum *= 2;

        // The first time, allocate 8 watched plane nodes.
        if(!wpl->maxnum)
            wpl->maxnum = 8;

        wpl->list =
            Z_Realloc(wpl->list, sizeof(plane_t*) * (wpl->maxnum + 1),
                      PU_LEVEL);
    }

    // Add the plane to the list.
    wpl->list[wpl->num-1] = pln;
    wpl->list[wpl->num] = NULL; // Terminate.
}

boolean R_RemoveWatchedPlane(watchedplanelist_t *wpl, const plane_t *pln)
{
    uint            i;

    if(!wpl || !pln)
        return false;

    for(i = 0; i < wpl->num; ++i)
    {
        if(wpl->list[i] == pln)
        {
            if(i == wpl->num - 1)
                wpl->list[i] = NULL;
            else
                memmove(&wpl->list[i], &wpl->list[i+1],
                        sizeof(plane_t*) * (wpl->num - 1 - i));
            wpl->num--;
            return true;
        }
    }

    return false;
}

/**
 * $smoothplane: Roll the height tracker buffers.
 */
void R_UpdateWatchedPlanes(watchedplanelist_t *wpl)
{
    uint                i;

    if(!wpl)
        return;

    for(i = 0; i < wpl->num; ++i)
    {
        plane_t        *pln = wpl->list[i];

        pln->oldheight[0] = pln->oldheight[1];
        pln->oldheight[1] = pln->height;

        if(pln->oldheight[0] != pln->oldheight[1])
            if(fabs(pln->oldheight[0] - pln->oldheight[1]) >=
               MAX_SMOOTH_PLANE_MOVE)
            {
                // Too fast: make an instantaneous jump.
                pln->oldheight[0] = pln->oldheight[1];
            }
    }
}

/**
 * $smoothplane: interpolate the visual offset.
 */
void R_InterpolateWatchedPlanes(watchedplanelist_t *wpl,
                                boolean resetNextViewer)
{
    uint                i;
    plane_t            *pln;

    if(!wpl)
        return;

    if(resetNextViewer)
    {
        // $smoothplane: Reset the plane height trackers.
        for(i = 0; i < wpl->num; ++i)
        {
            pln = wpl->list[i];

            pln->visoffset = 0;
            pln->oldheight[0] = pln->oldheight[1] = pln->height;

            // Has this plane reached its destination?
            if(R_RemoveWatchedPlane(wpl, pln))
                i = (i > 0? i-1 : 0);
        }
    }
    // While the game is paused there is no need to calculate any
    // visual plane offsets $smoothplane.
    else //if(!clientPaused)
    {
        // $smoothplane: Set the visible offsets.
        for(i = 0; i < wpl->num; ++i)
        {
            pln = wpl->list[i];

            pln->visoffset = pln->oldheight[0] * (1 - frameTimePos) +
                        pln->height * frameTimePos -
                        pln->height;

            // Visible plane height.
            pln->visheight = pln->height + pln->visoffset;

            // Has this plane reached its destination?
            if(pln->visheight == pln->height)
                if(R_RemoveWatchedPlane(wpl, pln))
                    i = (i > 0? i-1 : 0);
        }
    }
}

/**
 * Create a new plane for the given sector. The plane will be initialized
 * with default values.
 *
 * Post: The sector's plane list will be replaced, the new plane will be
 *       linked to the end of the list.
 *
 * @param sec       Sector for which a new plane will be created.
 *
 * @return          Ptr to the newly created plane.
 */
plane_t *R_NewPlaneForSector(sector_t *sec)
{
    uint            i;
    surface_t      *suf;
    plane_t        *plane;

    if(!sec)
        return NULL; // Do wha?

    if(sec->planecount >= 2)
        Con_Error("P_NewPlaneForSector: Cannot create plane for sector, "
                  "limit is %i per sector.\n", 2);

    // Allocate the new plane.
    plane = Z_Calloc(sizeof(plane_t), PU_LEVEL, 0);
    suf = &plane->surface;

    // Resize this sector's plane list.
    sec->planes =
        Z_Realloc(sec->planes, sizeof(plane_t*) * (++sec->planecount + 1),
                  PU_LEVEL);
    // Add the new plane to the end of the list.
    sec->planes[sec->planecount-1] = plane;
    sec->planes[sec->planecount] = NULL; // Terminate.

    // Setup header for DMU.
    plane->header.type = DMU_PLANE;

    // Initalize the plane.
    for(i = 0; i < 3; ++i)
        plane->glowrgb[i] = 1;

    // The back pointer (temporary)
    plane->sector = sec;

    // Initialize the surface.
    // \todo The initial material should be the "unknown" material.
	suf->material = NULL;
    for(i = 0; i < 4; ++i)
        suf->rgba[i] = 1;
    suf->flags = 0;
    suf->offset[VX] = suf->offset[VY] = 0;
    suf->decorations = NULL;
    suf->numdecorations = 0;

    // Set normal.
    suf->normal[VX] = 0;
    suf->normal[VY] = 0;
    suf->normal[VZ] = 1;

	return plane;
}

/**
 * Permanently destroys the specified plane of the given sector.
 * The sector's plane list is updated accordingly.
 *
 * @param id            The sector, plane id to be destroyed.
 * @param sec           Ptr to sector for which a plane will be destroyed.
 */
void R_DestroyPlaneOfSector(uint id, sector_t *sec)
{
    plane_t        *plane, **newList = NULL;

    if(!sec)
        return; // Do wha?

    if(id >= sec->planecount)
        Con_Error("P_DestroyPlaneOfSector: Plane id #%i is not valid for "
                  "sector #%u", id, (uint) GET_SECTOR_IDX(sec));

    plane = sec->planes[id];

    // Create a new plane list?
    if(sec->planecount > 1)
    {
        uint        i, n;

        newList = Z_Malloc(sizeof(plane_t**) * sec->planecount, PU_LEVEL, 0);

        // Copy ptrs to the planes.
        n = 0;
        for(i = 0; i < sec->planecount; ++i)
        {
            if(i == id)
                continue;
            newList[n++] = sec->planes[i];
        }
        newList[n] = NULL; // Terminate.
    }

    // If this plane is currently being watched, remove it.
    R_RemoveWatchedPlane(watchedPlaneList, plane);

    // Destroy the specified plane.
    Z_Free(plane);
    sec->planecount--;

    // Link the new list to the sector.
    Z_Free(sec->planes);
    sec->planes = newList;
}

void R_CreateSurfaceDecoration(surface_t *suf, float pos[3],
                               ded_decorlight_t *def)
{
    uint                i;
    surfacedecor_t     *d, *s, *decorations;

    if(!suf || !def)
        return;

    decorations =
        Z_Malloc(sizeof(*decorations) * (++suf->numdecorations),
                 PU_LEVEL, 0);

    if(suf->numdecorations > 1)
    {   // Copy the existing decorations.
        for(i = 0; i < suf->numdecorations - 1; ++i)
        {
            d = &decorations[i];
            s = &suf->decorations[i];

            d->pos[VX] = s->pos[VX];
            d->pos[VY] = s->pos[VY];
            d->pos[VZ] = s->pos[VZ];
            d->def = s->def;
        }

        Z_Free(suf->decorations);
    }

    // Add the new decoration.
    d = &decorations[suf->numdecorations - 1];
    d->pos[VX] = pos[VX];
    d->pos[VY] = pos[VY];
    d->pos[VZ] = pos[VZ];
    d->def = def;

    suf->decorations = decorations;
}

void R_ClearSurfaceDecorations(surface_t *suf)
{
    if(!suf)
        return;

    if(suf->decorations)
        Z_Free(suf->decorations);
    suf->decorations = NULL;
    suf->numdecorations = 0;
}

#ifdef _MSC_VER
#  pragma optimize("g", off)
#endif

/**
 * Called whenever the sector changes.
 *
 * This routine handles plane hacks where all of the sector's lines are
 * twosided and missing upper or lower textures.
 *
 * \note This does not support sectors with disjoint groups of lines
 *       (e.g. a sector with a "control" sector such as the forcefields in
 *       ETERNAL.WAD MAP01).
 *
 * \todo Needs updating for $nplanes.
 */
static void R_SetSectorLinks(sector_t *sec)
{
    uint        i;
    boolean     hackfloor, hackceil;
    sector_t   *floorLinkCandidate = 0, *ceilLinkCandidate = 0;

    // Must have a valid sector!
    if(!sec || !sec->linecount || (sec->flags & SECF_PERMANENTLINK))
        return;                 // Can't touch permanent links.

    hackfloor = (!R_IsSkySurface(&sec->SP_floorsurface));
    hackceil = (!R_IsSkySurface(&sec->SP_ceilsurface));
    if(!(hackfloor || hackceil))
        return;

    for(i = 0; i < sec->subsgroupcount; ++i)
    {
        subsector_t   **ssecp;
        ssecgroup_t    *ssgrp;

        if(!hackfloor && !hackceil)
            break;

        ssgrp = &sec->subsgroups[i];

        ssecp = sec->subsectors;
        while(*ssecp)
        {
            subsector_t    *ssec = *ssecp;

            if(!hackfloor && !hackceil)
                break;

            // Must be in the same group.
            if(ssec->group == i)
            {
                seg_t         **segp = ssec->segs;

                while(*segp)
                {
                    seg_t          *seg = *segp;
                    line_t         *lin;

                    if(!hackfloor && !hackceil)
                        break;

                    lin = seg->linedef;

                    if(lin && // minisegs don't count.
                       lin->L_frontside && lin->L_backside) // Must be twosided.
                    {
                        sector_t       *back;
                        side_t         *sid, *frontsid, *backsid;

                        // Check the vertex line owners for both verts.
                        // We are only interested in lines that do NOT share either vertex
                        // with a one-sided line (ie, its not "anchored").
                        if(lin->L_v1->anchored || lin->L_v2->anchored)
                            return;

                        // Check which way the line is facing.
                        sid = lin->L_frontside;
                        if(sid->sector == sec)
                        {
                            frontsid = sid;
                            backsid = lin->L_backside;
                        }
                        else
                        {
                            frontsid = lin->L_backside;
                            backsid = sid;
                        }

                        back = backsid->sector;
                        if(back == sec)
                            return;

                        // Check that there is something on the other side.
                        if(back->SP_ceilheight == back->SP_floorheight)
                            return;

                        // Check the conditions that prevent the invis plane.
                        if(back->SP_floorheight == sec->SP_floorheight)
                        {
                            hackfloor = false;
                        }
                        else
                        {
                            if(back->SP_floorheight > sec->SP_floorheight)
                                sid = frontsid;
                            else
                                sid = backsid;

                            if((sid->SW_bottommaterial && !(sid->SW_bottomflags & SUF_TEXFIX)) ||
                               (sid->SW_middlematerial && !(sid->SW_middleflags & SUF_TEXFIX)))
                                hackfloor = false;
                            else
                                floorLinkCandidate = back;
                        }

                        if(back->SP_ceilheight == sec->SP_ceilheight)
                        {
                            hackceil = false;
                        }
                        else
                        {
                            if(back->SP_ceilheight < sec->SP_ceilheight)
                                sid = frontsid;
                            else
                                sid = backsid;

                            if((sid->SW_topmaterial && !(sid->SW_topflags & SUF_TEXFIX)) ||
                               (sid->SW_middlematerial && !(sid->SW_middleflags & SUF_TEXFIX)))
                                hackceil = false;
                            else
                                ceilLinkCandidate = back;
                        }
                    }

                    segp++;
                }
            }

            ssecp++;
        }

        if(hackfloor)
        {
            if(floorLinkCandidate == sec->containsector)
                ssgrp->linked[PLN_FLOOR] = floorLinkCandidate;
        }

        if(hackceil)
        {
            if(ceilLinkCandidate == sec->containsector)
                ssgrp->linked[PLN_CEILING] = ceilLinkCandidate;
        }
    }
}

#ifdef _MSC_VER
#  pragma optimize("", on)
#endif

/**
 * Initialize the skyfix. In practice all this does is to check for mobjs
 * intersecting ceilings and if so: raises the sky fix for the sector a
 * bit to accommodate them.
 */
void R_InitSkyFix(void)
{
    float       f, b;
    float      *fix;
    uint        i;
    mobj_t     *it;
    sector_t   *sec;

    for(i = 0; i < numsectors; ++i)
    {
        sec = SECTOR_PTR(i);

        // Must have a sky ceiling.
        if(!R_IsSkySurface(&sec->SP_ceilsurface))
            continue;

        fix = &sec->skyfix[PLN_CEILING].offset;

        // Check that all the mobjs in the sector fit in.
        for(it = sec->mobjList; it; it = it->snext)
        {
            b = it->height;
            f = sec->SP_ceilheight + *fix -
                sec->SP_floorheight;

            if(b > f)
            {   // Must increase skyfix.
                *fix += b - f;

                if(verbose)
                {
                    Con_Printf("S%li: (mo)skyfix to %g (ceil=%g)\n",
                               (long) GET_SECTOR_IDX(sec), *fix,
                               sec->SP_ceilheight + *fix);
                }
            }
        }
    }
}

static boolean doSkyFix(sector_t *front, sector_t *back, uint pln)
{
    float       f, b;
    float       height = 0;
    boolean     adjusted = false;
    float      *fix = NULL;
    sector_t   *adjustSec = NULL;

    // Both the front and back surfaces must be sky on this plane.
    if(!R_IsSkySurface(&front->planes[pln]->surface) ||
       !R_IsSkySurface(&back->planes[pln]->surface))
        return false;

    f = front->planes[pln]->height + front->skyfix[pln].offset;
    b = back->planes[pln]->height + back->skyfix[pln].offset;

    if(f == b)
        return false;

    if(pln == PLN_CEILING)
    {
        if(f < b)
        {
            height = b - front->planes[pln]->height;
            fix = &front->skyfix[pln].offset;
            adjustSec = front;
        }
        else if(f > b)
        {
            height = f - back->planes[pln]->height;
            fix = &back->skyfix[pln].offset;
            adjustSec = back;
        }

        if(height > *fix)
        {   // Must increase skyfix.
           *fix = height;
            adjusted = true;
        }
    }
    else // its the floor.
    {
        if(f > b)
        {
            height = b - front->planes[pln]->height;
            fix = &front->skyfix[pln].offset;
            adjustSec = front;
        }
        else if(f < b)
        {
            height = f - back->planes[pln]->height;
            fix = &back->skyfix[pln].offset;
            adjustSec = back;
        }

        if(height < *fix)
        {   // Must increase skyfix.
           *fix = height;
            adjusted = true;
        }
    }

    if(verbose && adjusted)
    {
        Con_Printf("S%li: skyfix to %g (%s=%g)\n",
                   GET_SECTOR_IDX(adjustSec), *fix,
                   (pln == PLN_CEILING? "ceil" : "floor"),
                   adjustSec->planes[pln]->height + *fix);
    }

    return adjusted;
}

static void spreadSkyFixForNeighbors(vertex_t *vtx, line_t *refLine,
                                     boolean fixFloors, boolean fixCeilings,
                                     boolean *adjustedFloor,
                                     boolean *adjustedCeil)
{
    uint        pln;
    lineowner_t *base, *lOwner, *rOwner;
    boolean     doFix[2];
    boolean    *adjusted[2];

    doFix[PLN_FLOOR]   = fixFloors;
    doFix[PLN_CEILING] = fixCeilings;
    adjusted[PLN_FLOOR] = adjustedFloor;
    adjusted[PLN_CEILING] = adjustedCeil;

    // Find the reference line in the owner list.
    base = R_GetVtxLineOwner(vtx, refLine);

    // Spread will begin from the next line anti-clockwise.
    lOwner = base->LO_prev;

    // Spread clockwise around this vertex from the reference plus one
    // until we reach the reference again OR a single sided line.
    rOwner = base->LO_next;
    do
    {
        if(rOwner != lOwner)
        {
            for(pln = 0; pln < 2; ++pln)
            {
                if(!doFix[pln])
                    continue;

                if(doSkyFix(rOwner->line->L_frontsector,
                            lOwner->line->L_frontsector, pln))
                    *adjusted[pln] = true;

                if(lOwner->line->L_backside)
                if(doSkyFix(rOwner->line->L_frontsector,
                            lOwner->line->L_backsector, pln))
                    *adjusted[pln] = true;

                if(rOwner->line->L_backside)
                if(doSkyFix(rOwner->line->L_backsector,
                            lOwner->line->L_frontsector, pln))
                    *adjusted[pln] = true;

                if(rOwner->line->L_backside && lOwner->line->L_backside)
                if(doSkyFix(rOwner->line->L_backsector,
                            lOwner->line->L_frontsector, pln))
                    *adjusted[pln] = true;
            }
        }

        if(!rOwner->line->L_backside)
            break;

        rOwner = rOwner->LO_next;
    } while(rOwner != base);

    // Spread will begin from the next line clockwise.
    rOwner = base->LO_next;

    // Spread anti-clockwise around this vertex from the reference minus one
    // until we reach the reference again OR a single sided line.
    lOwner = base->LO_prev;
    do
    {
        if(rOwner != lOwner)
        {
            for(pln = 0; pln < 2; ++pln)
            {
                if(!doFix[pln])
                    continue;

                if(doSkyFix(rOwner->line->L_frontsector,
                            lOwner->line->L_frontsector, pln))
                    *adjusted[pln] = true;

                if(lOwner->line->L_backside)
                if(doSkyFix(rOwner->line->L_frontsector,
                            lOwner->line->L_backsector, pln))
                    *adjusted[pln] = true;

                if(rOwner->line->L_backside)
                if(doSkyFix(rOwner->line->L_backsector,
                            lOwner->line->L_frontsector, pln))
                    *adjusted[pln] = true;

                if(rOwner->line->L_backside && lOwner->line->L_backside)
                if(doSkyFix(rOwner->line->L_backsector,
                            lOwner->line->L_frontsector, pln))
                    *adjusted[pln] = true;
            }
        }

        if(!lOwner->line->L_backside)
            break;

        lOwner = lOwner->LO_prev;
    } while(lOwner != base);
}

/**
 * Fixing the sky means that for adjacent sky sectors the lower sky
 * ceiling is lifted to match the upper sky. The raising only affects
 * rendering, it has no bearing on gameplay.
 */
void R_SkyFix(boolean fixFloors, boolean fixCeilings)
{
    uint        i, pln;
    boolean     adjusted[2], doFix[2];

    if(!fixFloors && !fixCeilings)
        return; // Why are we here?

    doFix[PLN_FLOOR]   = fixFloors;
    doFix[PLN_CEILING] = fixCeilings;

    // We'll do this as long as we must to be sure all the sectors are fixed.
    // Do both floors and ceilings at the same time.
    do
    {
        adjusted[PLN_FLOOR] = adjusted[PLN_CEILING] = false;

        // We need to check all the linedefs.
        for(i = 0; i < numlines; ++i)
        {
            line_t     *line = LINE_PTR(i);
            sector_t   *front = (line->L_frontside? line->L_frontsector : NULL);
            sector_t   *back = (line->L_backside? line->L_backsector : NULL);

            // The conditions: must have two sides.
            if(!front || !back)
                continue;

            if(front != back) // its a normal two sided line
            {
                // Perform the skyfix as usual using the front and back sectors
                // of THIS line for comparing.
                for(pln = 0; pln < 2; ++pln)
                {
                    if(doFix[pln])
                        if(doSkyFix(front, back, pln))
                            adjusted[pln] = true;
                }
            }
            else if(line->flags & LINEF_SELFREF)
            {
                // Its a selfreferencing linedef, these will ALWAYS return
                // the same height on the front and back so we need to find
                // the neighbouring lines either side of this and compare
                // the front and back sectors of those instead.
                uint        j;
                vertex_t   *vtx[2];

                vtx[0] = line->L_v1;
                vtx[1] = line->L_v2;
                // Walk around each vertex in each direction.
                for(j = 0; j < 2; ++j)
                    if(vtx[j]->numlineowners > 1)
                        spreadSkyFixForNeighbors(vtx[j], line,
                                                 doFix[PLN_FLOOR],
                                                 doFix[PLN_CEILING],
                                                 &adjusted[PLN_FLOOR],
                                                 &adjusted[PLN_CEILING]);
            }
        }
    }
    while(adjusted[PLN_FLOOR] || adjusted[PLN_CEILING]);
}

/**
 * @return          Ptr to the lineowner for this line for this vertex else
 *                  @c NULL.
 */
lineowner_t* R_GetVtxLineOwner(vertex_t *v, line_t *line)
{
    if(v == line->L_v1)
        return line->L_vo1;

    if(v == line->L_v2)
        return line->L_vo2;

    return NULL;
}

void R_SetupFog(float start, float end, float density, float *rgb)
{
    Con_Execute(CMDS_DDAY, "fog on", true, false);
    Con_Executef(CMDS_DDAY, true, "fog start %f", start);
    Con_Executef(CMDS_DDAY, true, "fog end %f", end);
    Con_Executef(CMDS_DDAY, true, "fog density %f", density);
    Con_Executef(CMDS_DDAY, true, "fog color %.0f %.0f %.0f",
                 rgb[0] * 255, rgb[1] * 255, rgb[2] * 255);
}

void R_SetupFogDefaults(void)
{
    // Go with the defaults.
    Con_Execute(CMDS_DDAY,"fog off", true, false);
}

void R_SetupSky(ded_mapinfo_t *mapinfo)
{
    int         i, k;
    int         skyTex;

    if(!mapinfo)
    {   // Go with the defaults.
        Rend_SkyParams(DD_SKY, DD_HEIGHT, .666667f);
        Rend_SkyParams(DD_SKY, DD_HORIZON, 0);
        Rend_SkyParams(0, DD_ENABLE, 0);
        Rend_SkyParams(0, DD_MATERIAL, R_MaterialNumForName("SKY1", MAT_TEXTURE));
        Rend_SkyParams(0, DD_MASK, DD_NO);
        Rend_SkyParams(0, DD_OFFSET, 0);
        Rend_SkyParams(1, DD_DISABLE, 0);

        // There is no sky color.
        noSkyColorGiven = true;
        return;
    }

    Rend_SkyParams(DD_SKY, DD_HEIGHT, mapinfo->sky_height);
    Rend_SkyParams(DD_SKY, DD_HORIZON, mapinfo->horizon_offset);
    for(i = 0; i < 2; ++i)
    {
        k = mapinfo->sky_layers[i].flags;
        if(k & SLF_ENABLED)
        {
            skyTex = R_MaterialNumForName(mapinfo->sky_layers[i].texture, MAT_TEXTURE);
            if(skyTex == -1)
            {
                Con_Message("R_SetupSky: Invalid/missing texture \"%s\"\n",
                            mapinfo->sky_layers[i].texture);
                skyTex = R_MaterialNumForName("SKY1", MAT_TEXTURE);
            }

            Rend_SkyParams(i, DD_ENABLE, 0);
            Rend_SkyParams(i, DD_MATERIAL, skyTex);
            Rend_SkyParams(i, DD_MASK, k & SLF_MASKED ? DD_YES : DD_NO);
            Rend_SkyParams(i, DD_OFFSET, mapinfo->sky_layers[i].offset);
            Rend_SkyParams(i, DD_COLOR_LIMIT,
                           mapinfo->sky_layers[i].color_limit);
        }
        else
        {
            Rend_SkyParams(i, DD_DISABLE, 0);
        }
    }

    // Any sky models to setup? Models will override the normal
    // sphere.
    R_SetupSkyModels(mapinfo);

    // How about the sky color?
    noSkyColorGiven = true;
    for(i = 0; i < 3; ++i)
    {
        skyColorRGB[i] = mapinfo->sky_color[i];
        if(mapinfo->sky_color[i] > 0)
            noSkyColorGiven = false;
    }

    // Calculate a balancing factor, so the light in the non-skylit
    // sectors won't appear too bright.
    if(mapinfo->sky_color[0] > 0 || mapinfo->sky_color[1] > 0 ||
        mapinfo->sky_color[2] > 0)
    {
        skyColorBalance =
            (0 +
             (mapinfo->sky_color[0] * 2 + mapinfo->sky_color[1] * 3 +
              mapinfo->sky_color[2] * 2) / 7) / 1;
    }
    else
    {
        skyColorBalance = 1;
    }
}

/**
 * Returns pointers to the line's vertices in such a fashion that verts[0]
 * is the leftmost vertex and verts[1] is the rightmost vertex, when the
 * line lies at the edge of `sector.'
 */
void R_OrderVertices(const line_t *line, const sector_t *sector, vertex_t *verts[2])
{
    byte        edge;

    edge = (sector == line->L_frontsector? 0:1);
    verts[0] = line->L_v(edge);
    verts[1] = line->L_v(edge^1);
}

/**
 * A neighbour is a line that shares a vertex with 'line', and faces the
 * specified sector.
 */
line_t *R_FindLineNeighbor(sector_t *sector, line_t *line, lineowner_t *own,
                           boolean antiClockwise, binangle_t *diff)
{
    lineowner_t *cown = own->link[!antiClockwise];
    line_t     *other = cown->line;

    if(other == line)
        return NULL;

    if(diff) *diff += (antiClockwise? own->LO_prev->angle : own->angle);

    if(!other->L_backside || other->L_frontsector != other->L_backsector)
    {
        if(sector) // Must one of the sectors match?
        {
            if(other->L_frontsector == sector ||
               (other->L_backside && other->L_backsector == sector))
                return other;
        }
        else
            return other;
    }

    // Not suitable, try the next.
    return R_FindLineNeighbor(sector, line, cown, antiClockwise, diff);
}

line_t *R_FindSolidLineNeighbor(sector_t *sector, line_t *line, lineowner_t *own,
                                boolean antiClockwise, binangle_t *diff)
{
    lineowner_t *cown = own->link[!antiClockwise];
    line_t     *other = cown->line;
    int         side;

    if(other == line)
        return NULL;

    if(diff) *diff += (antiClockwise? own->LO_prev->angle : own->angle);

    if(!other->L_frontside || !other->L_backside)
        return other;

    if(!(other->flags & LINEF_SELFREF) &&
       (other->L_frontsector->SP_floorvisheight >= sector->SP_ceilvisheight ||
        other->L_frontsector->SP_ceilvisheight <= sector->SP_floorvisheight ||
        other->L_backsector->SP_floorvisheight >= sector->SP_ceilvisheight ||
        other->L_backsector->SP_ceilvisheight <= sector->SP_floorvisheight ||
        other->L_backsector->SP_ceilvisheight <= other->L_backsector->SP_floorvisheight))
        return other;

    // Both front and back MUST be open by this point.

    // Check for mid texture which fills the gap between floor and ceiling.
    // We should not give away the location of false walls (secrets).
    side = (other->L_frontsector == sector? 0 : 1);
    if(other->sides[side]->SW_middlematerial)
    {
        float oFCeil  = other->L_frontsector->SP_ceilvisheight;
        float oFFloor = other->L_frontsector->SP_floorvisheight;
        float oBCeil  = other->L_backsector->SP_ceilvisheight;
        float oBFloor = other->L_backsector->SP_floorvisheight;

        if((side == 0 &&
            ((oBCeil > sector->SP_floorvisheight &&
                  oBFloor <= sector->SP_floorvisheight) ||
             (oBFloor < sector->SP_ceilvisheight &&
                  oBCeil >= sector->SP_ceilvisheight) ||
             (oBFloor < sector->SP_ceilvisheight &&
                  oBCeil > sector->SP_floorvisheight))) ||
           ( /* side must be 1 */
            ((oFCeil > sector->SP_floorvisheight &&
                  oFFloor <= sector->SP_floorvisheight) ||
             (oFFloor < sector->SP_ceilvisheight &&
                  oFCeil >= sector->SP_ceilvisheight) ||
             (oFFloor < sector->SP_ceilvisheight &&
                  oFCeil > sector->SP_floorvisheight)))  )
        {

            if(!Rend_DoesMidTextureFillGap(other, side))
                return 0;
        }
    }

    // Not suitable, try the next.
    return R_FindSolidLineNeighbor(sector, line, cown, antiClockwise, diff);
}

/**
 * Find a backneighbour for the given line.
 * They are the neighbouring line in the backsector of the imediate line
 * neighbor.
 */
line_t *R_FindLineBackNeighbor(sector_t *sector, line_t *line, lineowner_t *own,
                               boolean antiClockwise, binangle_t *diff)
{
    lineowner_t *cown = own->link[!antiClockwise];
    line_t *other = cown->line;

    if(other == line)
        return NULL;

    if(diff) *diff += (antiClockwise? own->LO_prev->angle : own->angle);

    if(!other->L_backside || other->L_frontsector != other->L_backsector)
    {
        if(!(other->L_frontsector == sector ||
             (other->L_backside && other->L_backsector == sector)))
            return other;
    }

    // Not suitable, try the next.
    return R_FindLineBackNeighbor(sector, line, cown, antiClockwise,
                                   diff);
}

/**
 * A side's alignneighbor is a line that shares a vertex with 'line' and
 * whos orientation is aligned with it (thus, making it unnecessary to have
 * a shadow between them. In practice, they would be considered a single,
 * long sidedef by the shadow generator).
 */
line_t *R_FindLineAlignNeighbor(sector_t *sec, line_t *line,
                                lineowner_t *own, boolean antiClockwise,
                                int alignment)
{
#define SEP 10

    lineowner_t *cown = own->link[!antiClockwise];
    line_t     *other = cown->line;
    binangle_t diff;

    if(other == line)
        return NULL;

    if(!(other->flags & LINEF_SELFREF))
    {
        diff = line->angle - other->angle;

        if(alignment < 0)
            diff -= BANG_180;
        if(other->L_frontsector != sec)
            diff -= BANG_180;
        if(diff < SEP || diff > BANG_360 - SEP)
            return other;
    }

    // Can't step over non-twosided lines.
    if(!other->L_backside || !other->L_frontside)
        return NULL;

    // Not suitable, try the next.
    return R_FindLineAlignNeighbor(sec, line, cown, antiClockwise, alignment);

#undef SEP
}

void R_InitLinks(gamemap_t *map)
{
    uint        i;
    uint        starttime;

    Con_Message("R_InitLinks: Initializing\n");

    // Initialize node piles and line rings.
    NP_Init(&map->mobjnodes, 256);  // Allocate a small pile.
    NP_Init(&map->linenodes, map->numlines + 1000);

    // Allocate the rings.
    starttime = Sys_GetRealTime();
    map->linelinks =
        Z_Malloc(sizeof(*map->linelinks) * map->numlines, PU_LEVELSTATIC, 0);
    for(i = 0; i < map->numlines; ++i)
        map->linelinks[i] = NP_New(&map->linenodes, NP_ROOT_NODE);
    // How much time did we spend?
    VERBOSE(Con_Message
            ("R_InitLinks: Allocating line link rings. Done in %.2f seconds.\n",
             (Sys_GetRealTime() - starttime) / 1000.0f));
}


static ownernode_t *unusedNodeList = NULL;

static ownernode_t *newOwnerNode(void)
{
    ownernode_t *node;

    if(unusedNodeList)
    {   // An existing node is available for re-use.
        node = unusedNodeList;
        unusedNodeList = unusedNodeList->next;

        node->next = NULL;
        node->data = NULL;
    }
    else
    {   // Need to allocate another.
        node = M_Malloc(sizeof(ownernode_t));
    }

    return node;
}

static void addVertexToSSecOwnerList(ownerlist_t *ownerList, fvertex_t *v)
{
    ownernode_t *node;

    if(!v)
        return; // Wha?

    // Add a new owner.
    // NOTE: No need to check for duplicates.
    ownerList->count++;

    node = newOwnerNode();
    node->data = v;
    node->next = ownerList->head;
    ownerList->head = node;
}

/**
 * Create a list of vertices for the subsector which are suitable for
 * use as the points of single a trifan.
 *
 * We are assured by the node building process that subsector->segs has
 * been ordered by angle, clockwise starting from the smallest angle.
 * So, most of the time, the points can be created directly from the
 * seg vertexes.
 *
 * However, we do not want any overlapping tris so check the area of
 * each triangle is > 0, if not; try the next vertice in the list until
 * we find a good one to use as the center of the trifan. If a suitable
 * point cannot be found use the center of subsector instead (it will
 * always be valid as subsectors are convex).
 */
static void triangulateSubSector(subsector_t *ssec)
{
    uint        j;
    seg_t     **ptr;
    ownerlist_t subSecOwnerList;
    boolean     found = false;

    memset(&subSecOwnerList, 0, sizeof(subSecOwnerList));

    // Create one node for each vertex of the subsector.
    ptr = ssec->segs;
    while(*ptr)
    {
        fvertex_t *other = &((*ptr)->SG_v1->v);
        addVertexToSSecOwnerList(&subSecOwnerList, other);
        *ptr++;
    }

    // We need to find a good tri-fan base vertex, (one that doesn't
    // generate zero-area triangles).
    if(subSecOwnerList.count <= 3)
    {   // Always valid.
        found = true;
    }
    else
    {   // Higher vertex counts need checking, we'll test each one
        // and pick the first good one.
        ownernode_t *base = subSecOwnerList.head;

        while(base && !found)
        {
            ownernode_t *current;
            boolean     ok;

            current = base;
            ok = true;
            j = 0;
            while(j < subSecOwnerList.count - 2 && ok)
            {
#define TRIFAN_LIMIT    0.1

                ownernode_t *a, *b;

                if(current->next)
                    a = current->next;
                else
                    a = subSecOwnerList.head;
                if(a->next)
                    b = a->next;
                else
                    b = subSecOwnerList.head;

                if(TRIFAN_LIMIT >=
                   M_TriangleArea(((fvertex_t*) base->data)->pos,
                                  ((fvertex_t*) a->data)->pos,
                                  ((fvertex_t*) b->data)->pos))
                {
                    ok = false;
                }
                else
                {   // Keep checking...
                    if(current->next)
                        current = current->next;
                    else
                        current = subSecOwnerList.head;

                    j++;
                }

#undef TRIFAN_LIMIT
            }

            if(ok)
            {   // This will do nicely.
                // Must ensure that the vertices are ordered such that
                // base comes last (this is because when adding vertices
                // to the ownerlist; it is done backwards).
                ownernode_t *last;

                // Find the last.
                last = base;
                while(last->next) last = last->next;

                if(base != last)
                {   // Need to change the order.
                    last->next = subSecOwnerList.head;
                    subSecOwnerList.head = base->next;
                    base->next = NULL;
                }

                found = true;
            }
            else
            {
                base = base->next;
            }
        }
    }

    if(!found)
    {   // No suitable triangle fan base vertex found.
        ownernode_t *newNode, *last;

        // Use the subsector midpoint as the base since it will always
        // be valid.
        ssec->flags |= SUBF_MIDPOINT;

        // This entails adding the midpoint as a vertex at the start
        // and duplicating the first vertex at the end (so the fan
        // wraps around).

        // We'll have to add the end vertex manually...
        // Find the end.
        last = subSecOwnerList.head;
        while(last->next) last = last->next;

        newNode = newOwnerNode();
        newNode->data = &ssec->midpoint;
        newNode->next = NULL;

        last->next = newNode;
        subSecOwnerList.count++;

        addVertexToSSecOwnerList(&subSecOwnerList, last->data);
    }

    // We can now create the subsector vertex array by hardening the list.
    // NOTE: The same polygon is used for all planes of this subsector.
    ssec->numvertices = subSecOwnerList.count;
    ssec->vertices =
        Z_Malloc(sizeof(fvertex_t*) * (ssec->numvertices + 1),
                 PU_LEVELSTATIC, 0);

    {
    ownernode_t *node, *p;

    node = subSecOwnerList.head;
    j = ssec->numvertices - 1;
    while(node)
    {
        p = node->next;
        ssec->vertices[j--] = (fvertex_t*) node->data;

        // Move this node to the unused list for re-use.
        node->next = unusedNodeList;
        unusedNodeList = node;

        node = p;
    }
    }

    ssec->vertices[ssec->numvertices] = NULL; // terminate.
}

/**
 * Polygonizes all subsectors in the map.
 */
void R_PolygonizeMap(gamemap_t *map)
{
    uint        i;
    ownernode_t *node, *p;
    uint        startTime;

    startTime = Sys_GetRealTime();

    // Init the unused ownernode list.
    unusedNodeList = NULL;

    // Polygonize each subsector.
    for(i = 0; i < map->numsubsectors; ++i)
    {
        subsector_t *sub = &map->subsectors[i];
        triangulateSubSector(sub);
    }

    // Free any nodes left in the unused list.
    node = unusedNodeList;
    while(node)
    {
        p = node->next;
        M_Free(node);
        node = p;
    }
    unusedNodeList = NULL;

    // How much time did we spend?
    VERBOSE(Con_Message
            ("R_PolygonizeMap: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));

#ifdef _DEBUG
    Z_CheckHeap();
#endif
}

static void initPlaneIllumination(subsector_t *ssec, uint planeID)
{
    uint        i, j, num;
    subplaneinfo_t *plane = ssec->planes[planeID];

    num = ssec->numvertices;

    plane->illumination =
        Z_Calloc(num * sizeof(vertexillum_t), PU_LEVELSTATIC, NULL);

    for(i = 0; i < num; ++i)
    {
        plane->illumination[i].flags |= VIF_STILL_UNSEEN;

        for(j = 0; j < MAX_BIAS_AFFECTED; ++j)
            plane->illumination[i].casted[j].source = -1;
    }
}

static void initSSecPlanes(subsector_t *ssec)
{
    uint        i;

    // Allocate the subsector plane info array.
    ssec->planes =
        Z_Malloc(ssec->sector->planecount * sizeof(subplaneinfo_t*),
                 PU_LEVEL, NULL);
    for(i = 0; i < ssec->sector->planecount; ++i)
    {
        ssec->planes[i] =
            Z_Calloc(sizeof(subplaneinfo_t), PU_LEVEL, NULL);

        // Initialize the illumination for the subsector.
        initPlaneIllumination(ssec, i);
    }

    // \fixme $nplanes
    // Initialize the plane types.
    ssec->planes[PLN_FLOOR]->type = PLN_FLOOR;
    ssec->planes[PLN_CEILING]->type = PLN_CEILING;
}

static void prepareSubsectorForBias(subsector_t *ssec)
{
    seg_t         **segPtr;

    initSSecPlanes(ssec);

    segPtr = ssec->segs;
    while(*segPtr)
    {
        uint            i;
        seg_t          *seg = *segPtr;

        for(i = 0; i < i; ++i)
        {
            uint        j;

            for(j = 0; j < 3; ++j)
            {
                uint        n;
                seg->illum[j][i].flags = VIF_STILL_UNSEEN;

                for(n = 0; n < MAX_BIAS_AFFECTED; ++n)
                {
                    seg->illum[j][i].casted[n].source = -1;
                }
            }
        }

        *segPtr++;
    }
}

void R_PrepareForBias(gamemap_t *map)
{
    uint            i;
    uint            starttime = Sys_GetRealTime();

    Con_Message("prepareForBias: Processing...\n");

    for(i = 0; i < map->numsubsectors; ++i)
    {
        subsector_t    *ssec = &map->subsectors[i];
        prepareSubsectorForBias(ssec);
    }

    // How much time did we spend?
    VERBOSE(Con_Message
            ("prepareForBias: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - starttime) / 1000.0f));
}


/*boolean DD_SubContainTest(sector_t *innersec, sector_t *outersec)
   {
   uint i, k;
   boolean contained;
   float in[4], out[4];
   subsector_t *isub, *osub;

   // Test each subsector of innersec against all subsectors of outersec.
   for(i=0; i<numsubsectors; i++)
   {
   isub = SUBSECTOR_PTR(i);
   contained = false;
   // Only accept innersec's subsectors.
   if(isub->sector != innersec) continue;
   for(k=0; k<numsubsectors && !contained; k++)
   {
   osub = SUBSECTOR_PTR(i);
   // Only accept outersec's subsectors.
   if(osub->sector != outersec) continue;
   // Test if isub is inside osub.
   if(isub->bbox[BLEFT] >= osub->bbox[BLEFT]
   && isub->bbox[BRIGHT] <= osub->bbox[BRIGHT]
   && isub->bbox[BTOP] >= osub->bbox[BTOP]
   && isub->bbox[BBOTTOM] <= osub->bbox[BBOTTOM])
   {
   // This is contained.
   contained = true;
   }
   }
   if(!contained) return false;
   }
   // All tested subsectors were contained!
   return true;
   } */

/**
 * The test is done on subsectors.
 */
static sector_t *getContainingSectorOf(gamemap_t *map, sector_t *sec)
{
    uint        i;
    float       cdiff = -1, diff;
    float       inner[4], outer[4];
    sector_t   *other, *closest = NULL;

    memcpy(inner, sec->bbox, sizeof(inner));

    // Try all sectors that fit in the bounding box.
    for(i = 0, other = map->sectors; i < map->numsectors; other++, ++i)
    {
        if(!other->linecount || (other->flags & SECF_UNCLOSED))
            continue;

        if(other == sec)
            continue;           // Don't try on self!

        memcpy(outer, other->bbox, sizeof(outer));
        if(inner[BOXLEFT]  >= outer[BOXLEFT] &&
           inner[BOXRIGHT] <= outer[BOXRIGHT] &&
           inner[BOXTOP]   <= outer[BOXTOP] &&
           inner[BOXBOTTOM]>= outer[BOXBOTTOM])
        {
            // Inside! Now we must test each of the subsectors. Otherwise
            // we can't be sure...
            /*if(DD_SubContainTest(sec, other))
               { */
            // Sec is totally and completely inside other!
            diff = M_BoundingBoxDiff(inner, outer);
            if(cdiff < 0 || diff <= cdiff)
            {
                closest = other;
                cdiff = diff;
            }
            //}
        }
    }
    return closest;
}

void R_BuildSectorLinks(gamemap_t *map)
{
#define DOMINANT_SIZE 1000

    uint        i;

    for(i = 0; i < map->numsectors; ++i)
    {
        uint        k;
        sector_t   *other;
        line_t     *lin;
        sector_t   *sec = &map->sectors[i];
        boolean     dohack;

        if(!sec->linecount)
            continue;

        // Is this sector completely contained by another?
        sec->containsector = getContainingSectorOf(map, sec);

        dohack = true;
        for(k = 0; k < sec->linecount; ++k)
        {
            lin = sec->Lines[k];
            if(!lin->L_frontside || !lin->L_backside ||
                lin->L_frontsector != lin->L_backsector)
            {
                dohack = false;
                break;
            }
        }

        if(dohack)
        {   // Link all planes permanently.
            sec->flags |= SECF_PERMANENTLINK;

            // Only floor and ceiling can be linked, not all planes inbetween.
            for(k = 0; k < sec->subsgroupcount; ++k)
            {
                ssecgroup_t *ssgrp = &sec->subsgroups[k];
                uint        p;

                for(p = 0; p < sec->planecount; ++p)
                    ssgrp->linked[p] = sec->containsector;
            }

            Con_Printf("Linking S%i planes permanently to S%li\n", i,
                       (long) (sec->containsector - map->sectors));
        }

        // Is this sector large enough to be a dominant light source?
        if(sec->lightsource == NULL &&
           (R_IsSkySurface(&sec->SP_ceilsurface) ||
            R_IsSkySurface(&sec->SP_floorsurface)) &&
           sec->bbox[BOXRIGHT] - sec->bbox[BOXLEFT]   > DOMINANT_SIZE &&
           sec->bbox[BOXTOP]   - sec->bbox[BOXBOTTOM] > DOMINANT_SIZE)
        {
            // All sectors touching this one will be affected.
            for(k = 0; k < sec->linecount; ++k)
            {
                lin = sec->Lines[k];
                other = lin->L_frontsector;
                if(!other || other == sec)
                {
                    if(lin->L_backside)
                    {
                        other = lin->L_backsector;
                        if(!other || other == sec)
                            continue;
                    }
                }

                other->lightsource = sec;
            }
        }
    }

#undef DOMINANT_SIZE
}

/**
 * Called by the game at various points in the level setup process.
 */
void R_SetupLevel(int mode, int flags)
{
    uint        i;

    switch(mode)
    {
    case DDSLM_INITIALIZE:
        // Switch to fast malloc mode in the zone. This is intended for large
        // numbers of mallocs with no frees in between.
        Z_EnableFastMalloc(false);

        // A new level is about to be setup.
        levelSetup = true;
        return;

    case DDSLM_AFTER_LOADING:
    {
        side_t *side;

        // Loading a game usually destroys all thinkers. Until a proper
        // savegame system handled by the engine is introduced we'll have
        // to resort to re-initializing the most important stuff.
        P_SpawnTypeParticleGens();

        // Update everything again. Its possible that after loading we
        // now have more HOMs to fix, etc..

        R_SkyFix(true, true); // fix floors and ceilings.

        // Update all sectors. Set intial values of various tracked
        // and interpolated properties (lighting, smoothed planes etc).
        for(i = 0; i < numsectors; ++i)
        {
            uint            j;
            sector_t       *sec = SECTOR_PTR(i);

            R_UpdateSector(sec, false);
            for(j = 0; j < sec->planecount; ++j)
            {
                sec->planes[j]->visheight = sec->planes[j]->oldheight[0] =
                    sec->planes[j]->oldheight[1] = sec->planes[j]->height;
            }
        }

        // Do the same for side surfaces.
        for(i = 0; i < numsides; ++i)
        {
            side = SIDE_PTR(i);
            R_UpdateSurface(&side->SW_topsurface, false);
            R_UpdateSurface(&side->SW_middlesurface, false);
            R_UpdateSurface(&side->SW_bottomsurface, false);
        }

        // We don't render fakeradio on polyobjects...
        PO_SetupPolyobjs();
        return;
    }
    case DDSLM_FINALIZE:
    {
        int         j, k;
        line_t     *line;

        // Init server data.
        Sv_InitPools();

        // Recalculate the light range mod matrix.
        Rend_CalcLightRangeModMatrix(NULL);

        // Update all sectors. Set intial values of various tracked
        // and interpolated properties (lighting, smoothed planes etc).
        for(i = 0; i < numsectors; ++i)
        {
            uint            l;
            sector_t       *sec = SECTOR_PTR(i);

            R_UpdateSector(sec, true);
            for(l = 0; l < sec->planecount; ++l)
            {
                sec->planes[l]->visheight = sec->planes[l]->oldheight[0] =
                    sec->planes[l]->oldheight[1] = sec->planes[l]->height;
            }
        }

        for(i = 0; i < numlines; ++i)
        {
            line = LINE_PTR(i);

            if(line->mapflags & 0x0100) // The old ML_MAPPED flag
            {
                // This line wants to be seen in the map from the begining.
                memset(&line->mapped, 1, sizeof(&line->mapped));

                // Send a status report.
                if(gx.HandleMapObjectStatusReport)
                {
                    int  pid;

                    for(k = 0; k < DDMAXPLAYERS; ++k)
                    {
                        pid = k;
                        gx.HandleMapObjectStatusReport(DMUSC_LINE_FIRSTRENDERED,
                                                      i, DMU_LINE, &pid);
                    }
                }
                line->mapflags &= ~0x0100; // remove the flag.
            }

            // Update side surfaces.
            for(j = 0; j < 2; ++j)
            {
                if(!line->sides[j])
                    continue;

                R_UpdateSurface(&line->sides[j]->SW_topsurface, true);
                R_UpdateSurface(&line->sides[j]->SW_middlesurface, true);
                R_UpdateSurface(&line->sides[j]->SW_bottomsurface, true);
            }
        }

        // We don't render fakeradio on polyobjects...
        PO_SetupPolyobjs();

        // Run any commands specified in Map Info.
        {
        gamemap_t  *map = P_GetCurrentMap();
        ded_mapinfo_t *mapInfo = Def_GetMapInfo(P_GetMapID(map));

        if(mapInfo && mapInfo->execute)
            Con_Execute(CMDS_DED, mapInfo->execute, true, false);
        }

        // The level setup has been completed.  Run the special level
        // setup command, which the user may alias to do something
        // useful.
        if(levelid && levelid[0])
        {
            char    cmd[80];

            sprintf(cmd, "init-%s", levelid);
            if(Con_IsValidCommand(cmd))
            {
                Con_Executef(CMDS_DED, false, cmd);
            }
        }

        // Clear any input events that might have accumulated during the
        // setup period.
        DD_ClearEvents();

        // Now that the setup is done, let's reset the tictimer so it'll
        // appear that no time has passed during the setup.
        DD_ResetTimer();

        // Kill all local commands.
        for(i = 0; i < MAXPLAYERS; ++i)
        {
            clients[i].numTics = 0;
        }

        // Reset the level tick timer.
        levelTime = 0;

        // We've finished setting up the level
        levelSetup = false;

        // Inform the timing system to suspend the starting of the clock.
        firstFrameAfterLoad = true;

        // Switch back to normal malloc mode in the zone. Z_Malloc will look
        // for free blocks in the entire zone and purge purgable blocks.
        Z_EnableFastMalloc(false);
        return;
    }
    case DDSLM_AFTER_BUSY:
    {
        gamemap_t  *map = P_GetCurrentMap();
        ded_mapinfo_t *mapInfo = Def_GetMapInfo(P_GetMapID(map));

        // Shouldn't do anything time-consuming, as we are no longer in busy mode.
        if(!mapInfo || !(mapInfo->flags & MIF_FOG))
            R_SetupFogDefaults();
        else
            R_SetupFog(mapInfo->fog_start, mapInfo->fog_end,
                       mapInfo->fog_density, mapInfo->fog_color);
        break;
    }
    default:
        Con_Error("R_SetupLevel: Unknown setup mode %i", mode);
    }
}

void R_ClearSectorFlags(void)
{
    uint        i;
    sector_t   *sec;

    for(i = 0; i < numsectors; ++i)
    {
        sec = SECTOR_PTR(i);
        // Clear all flags that can be cleared before each frame.
        sec->frameflags &= ~SIF_FRAME_CLEAR;
    }
}

sector_t *R_GetLinkedSector(subsector_t *startssec, uint plane)
{
    ssecgroup_t        *ssgrp =
        &startssec->sector->subsgroups[startssec->group];

    if(!devNoLinkedSurfaces && ssgrp->linked[plane])
        return ssgrp->linked[plane];
    else
        return startssec->sector;
}

void R_UpdateAllSurfaces(boolean forceUpdate)
{
    uint        i, j;

    // First, all planes of all sectors.
    for(i = 0; i < numsectors; ++i)
    {
        sector_t *sec = SECTOR_PTR(i);

        for(j = 0; j < sec->planecount; ++j)
            R_UpdateSurface(&sec->planes[j]->surface, forceUpdate);
    }

    // Then all sections of all sides.
    for(i = 0; i < numsides; ++i)
    {
        side_t *side = SIDE_PTR(i);

        R_UpdateSurface(&side->SW_topsurface, forceUpdate);
        R_UpdateSurface(&side->SW_middlesurface, forceUpdate);
        R_UpdateSurface(&side->SW_bottomsurface, forceUpdate);
    }
}

void R_UpdateSurface(surface_t *suf, boolean forceUpdate)
{
    int         texFlags, oldTexFlags;

    // Any change to the texture or glow properties?
    texFlags = R_GetMaterialFlags(suf->material);
    oldTexFlags = R_GetMaterialFlags(suf->oldmaterial);

    // \fixme Update glowing status?
    // The order of these tests is important.
    if(forceUpdate || (suf->material != suf->oldmaterial))
    {
        // Check if the new texture is declared as glowing.
        // NOTE: Currently, we always discard the glow settings of the
        //       previous flat after a texture change.

        // Now that glows are properties of the sector this does not
        // need to be the case. If we expose these properties via DMU
        // they could be changed at any time. However in order to support
        // flats that are declared as glowing we would need some method
        // of telling Doomsday IF we want to inherit these properties when
        // the plane flat changes...
        if(texFlags & TXF_GLOW)
        {
            // The new texture is glowing.
            suf->flags |= SUF_GLOW;
        }
        else if(suf->oldmaterial && (oldTexFlags & TXF_GLOW))
        {
            // The old texture was glowing but the new one is not.
            suf->flags &= ~SUF_GLOW;
        }

        suf->oldmaterial = suf->material;

        // No longer a missing texture fix?
        if(suf->material && (oldTexFlags & SUF_TEXFIX))
            suf->flags &= ~SUF_TEXFIX;

        suf->flags |= SUF_UPDATE_DECORATIONS;
    }
    else if((texFlags & TXF_GLOW) != (oldTexFlags & TXF_GLOW))
    {
        // The glow property of the current flat has been changed
        // since last update.

        // NOTE:
        // This approach is hardly optimal but since flats will
        // rarely/if ever have this property changed during normal
        // gameplay (the RENDER_GLOWFLATS text string is depreciated and
        // the only time this property might change is after a console
        // RESET) so it doesn't matter.
        if(!(texFlags & TXF_GLOW) && (oldTexFlags & TXF_GLOW))
        {
            // The current flat is no longer glowing
            suf->flags &= ~SUF_GLOW;
        }
        else if((texFlags & TXF_GLOW) && !(oldTexFlags & TXF_GLOW))
        {
            // The current flat is now glowing
            suf->flags |= SUF_GLOW;
        }

        suf->flags |= SUF_UPDATE_DECORATIONS;
    }
    // < FIXME

    if(forceUpdate ||
       suf->flags != suf->oldflags)
    {
        suf->oldflags = suf->flags;
        suf->flags |= SUF_UPDATE_DECORATIONS;
    }

    if(forceUpdate ||
       (suf->offset[VX] != suf->oldoffset[VX]))
    {
        suf->oldoffset[VX] = suf->offset[VX];
        suf->flags |= SUF_UPDATE_DECORATIONS;
    }

    if(forceUpdate ||
       (suf->offset[VY] != suf->oldoffset[VY]))
    {
        suf->oldoffset[VY] = suf->offset[VY];
        suf->flags |= SUF_UPDATE_DECORATIONS;
    }

    // Surface color change?
    if(forceUpdate ||
       (suf->rgba[0] != suf->oldrgba[0] ||
        suf->rgba[1] != suf->oldrgba[1] ||
        suf->rgba[2] != suf->oldrgba[2] ||
        suf->rgba[3] != suf->oldrgba[3]))
    {
        // \todo when surface colours are intergrated with the
        // bias lighting model we will need to recalculate the
        // vertex colours when they are changed.
        memcpy(suf->oldrgba, suf->rgba, sizeof(suf->oldrgba));
    }
}

void R_UpdateSector(sector_t* sec, boolean forceUpdate)
{
    uint        i, j;
    plane_t    *plane;
    boolean     updateReverb = false;
    boolean     updateDecorations = false;

    // Check if there are any lightlevel or color changes.
    if(forceUpdate ||
       (sec->lightlevel != sec->oldlightlevel ||
        sec->rgb[0] != sec->oldrgb[0] ||
        sec->rgb[1] != sec->oldrgb[1] ||
        sec->rgb[2] != sec->oldrgb[2]))
    {
        sec->frameflags |= SIF_LIGHT_CHANGED;
        sec->oldlightlevel = sec->lightlevel;
        memcpy(sec->oldrgb, sec->rgb, sizeof(sec->oldrgb));

        LG_SectorChanged(sec);
        updateDecorations = true;
    }
    else
    {
        sec->frameflags &= ~SIF_LIGHT_CHANGED;
    }

    // For each plane.
    for(i = 0; i < sec->planecount; ++i)
    {
        plane = sec->planes[i];

        // Surface changes?
        R_UpdateSurface(&plane->surface, forceUpdate);

        // \fixme Now update the glow properties.
        if(plane->surface.flags & SUF_GLOW)
        {
            plane->glow = 4; // Default height factor is 4

            R_GetMaterialColor(plane->PS_material->ofTypeID,
                                plane->PS_material->type, plane->glowrgb);
        }
        else
        {
            plane->glow = 0;
            for(j = 0; j < 3; ++j)
                plane->glowrgb[j] = 0;
        }
        // < FIXME

        // Geometry change?
        if(forceUpdate ||
           plane->height != plane->oldheight[1])
        {
            ddplayer_t *player;

            // Check if there are any camera players in this sector. If their
            // height is now above the ceiling/below the floor they are now in
            // the void.
            for(j = 0; j < MAXPLAYERS; ++j)
            {
                player = &players[j];
                if(!player->ingame || !player->mo || !player->mo->subsector)
                    continue;

                if((player->flags & DDPF_CAMERA) &&
                   player->mo->subsector->sector == sec &&
                   (player->mo->pos[VZ] > sec->SP_ceilheight ||
                    player->mo->pos[VZ] < sec->SP_floorheight))
                {
                    player->invoid = true;
                }
            }

            P_PlaneChanged(sec, i);
            updateReverb = true;
            plane->surface.flags |= SUF_UPDATE_DECORATIONS;
        }

        if(updateDecorations)
            plane->surface.flags |= SUF_UPDATE_DECORATIONS;
    }

    if(updateReverb)
    {
        S_CalcSectorReverb(sec);
    }

    if(!(sec->flags & SECF_PERMANENTLINK)) // Assign new links
    {
        // Only floor and ceiling can be linked, not all inbetween
        for(i = 0; i < sec->subsgroupcount; ++i)
        {
            ssecgroup_t *ssgrp = &sec->subsgroups[i];

            ssgrp->linked[PLN_FLOOR] = NULL;
            ssgrp->linked[PLN_CEILING] = NULL;
        }
        R_SetSectorLinks(sec);
    }
}

/**
 * All links will be updated every frame (sectorheights may change at
 * any time without notice).
 */
void R_UpdatePlanes(void)
{
    // Nothing to do.
}

/**
 * Sector light color may be affected by the sky light color.
 */
const float *R_GetSectorLightColor(sector_t *sector)
{
    sector_t   *src;
    uint        i;

    if(!rendSkyLight || noSkyColorGiven)
        return sector->rgb;     // The sector's real color.

    if(!R_IsSkySurface(&sector->SP_ceilsurface) &&
       !R_IsSkySurface(&sector->SP_floorsurface))
    {
        // A dominant light source affects this sector?
        src = sector->lightsource;
        if(src && src->lightlevel >= sector->lightlevel)
        {
            // The color shines here, too.
            return R_GetSectorLightColor(src);
        }

        // Return the sector's real color (balanced against sky's).
        if(skyColorBalance >= 1)
        {
            return sector->rgb;
        }
        else
        {
            for(i = 0; i < 3; ++i)
                balancedRGB[i] = sector->rgb[i] * skyColorBalance;
            return balancedRGB;
        }
    }
    // Return the sky color.
    return skyColorRGB;
}


/**
 * Compares the angles of two lines that share a common vertex.
 *
 * pre: rootVtx must point to the vertex common between a and b
 *      which are (lineowner_t*) ptrs.
 */
static int C_DECL lineAngleSorter(const void *a, const void *b)
{
    uint        i;
    fixed_t     dx, dy;
    binangle_t  angles[2];
    lineowner_t *own[2];
    line_t     *line;

    own[0] = (lineowner_t *)a;
    own[1] = (lineowner_t *)b;
    for(i = 0; i < 2; ++i)
    {
        if(own[i]->LO_prev) // We have a cached result.
        {
            angles[i] = own[i]->angle;
        }
        else
        {
            vertex_t    *otherVtx;

            line = own[i]->line;
            otherVtx = line->L_v(line->L_v1 == rootVtx? 1:0);

            dx = otherVtx->V_pos[VX] - rootVtx->V_pos[VX];
            dy = otherVtx->V_pos[VY] - rootVtx->V_pos[VY];

            own[i]->angle = angles[i] = bamsAtan2(-100 *dx, 100 * dy);

            // Mark as having a cached angle.
            own[i]->LO_prev = (lineowner_t*) 1;
        }
    }

    return (angles[1] - angles[0]);
}

/**
 * Merge left and right line owner lists into a new list.
 *
 * @return          Ptr to the newly merged list.
 */
static lineowner_t *mergeLineOwners(lineowner_t *left, lineowner_t *right,
                                    int (C_DECL *compare) (const void *a,
                                                           const void *b))
{
    lineowner_t tmp, *np;

    np = &tmp;
    tmp.LO_next = np;
    while(left != NULL && right != NULL)
    {
        if(compare(left, right) <= 0)
        {
            np->LO_next = left;
            np = left;

            left = left->LO_next;
        }
        else
        {
            np->LO_next = right;
            np = right;

            right = right->LO_next;
        }
    }

    // At least one of these lists is now empty.
    if(left)
        np->LO_next = left;
    if(right)
        np->LO_next = right;

    // Is the list empty?
    if(tmp.LO_next == &tmp)
        return NULL;

    return tmp.LO_next;
}

static lineowner_t *splitLineOwners(lineowner_t *list)
{
    lineowner_t *lista, *listb, *listc;

    if(!list)
        return NULL;

    lista = listb = listc = list;
    do
    {
        listc = listb;
        listb = listb->LO_next;
        lista = lista->LO_next;
        if(lista != NULL)
            lista = lista->LO_next;
    } while(lista);

    listc->LO_next = NULL;
    return listb;
}

/**
 * This routine uses a recursive mergesort algorithm; O(NlogN)
 */
static lineowner_t *sortLineOwners(lineowner_t *list,
                                   int (C_DECL *compare) (const void *a,
                                                          const void *b))
{
    lineowner_t *p;

    if(list && list->LO_next)
    {
        p = splitLineOwners(list);

        // Sort both halves and merge them back.
        list = mergeLineOwners(sortLineOwners(list, compare),
                               sortLineOwners(p, compare), compare);
    }
    return list;
}

static void setVertexLineOwner(vertex_t *vtx, line_t *lineptr,
                               lineowner_t **storage)
{
    lineowner_t *p, *newOwner;

    if(!lineptr)
        return;

    // If this is a one-sided line then this is an "anchored" vertex.
    if(!(lineptr->L_frontside && lineptr->L_backside))
        vtx->anchored = true;

    // Has this line already been registered with this vertex?
    if(vtx->numlineowners != 0)
    {
        p = vtx->lineowners;
        while(p)
        {
            if(p->line == lineptr)
                return;             // Yes, we can exit.

            p = p->LO_next;
        }
    }

    //Add a new owner.
    vtx->numlineowners++;

    newOwner = (*storage)++;
    newOwner->line = lineptr;
    newOwner->LO_prev = NULL;

    // Link it in.
    // NOTE: We don't bother linking everything at this stage since we'll
    // be sorting the lists anyway. After which we'll finish the job by
    // setting the prev and circular links.
    // So, for now this is only linked singlely, forward.
    newOwner->LO_next = vtx->lineowners;
    vtx->lineowners = newOwner;

    // Link the line to its respective owner node.
    if(vtx == lineptr->L_v1)
        lineptr->L_vo1 = newOwner;
    else
        lineptr->L_vo2 = newOwner;
}

/**
 * Generates the line owner rings for each vertex. Each ring includes all
 * the lines which the vertex belongs to sorted by angle, (the rings is
 * arranged in clockwise order, east = 0).
 */
void R_BuildVertexOwners(gamemap_t *map)
{
    uint        startTime = Sys_GetRealTime();

    uint        i;
    lineowner_t *lineOwners, *allocator;

    // We know how many vertex line owners we need (numlines * 2).
    lineOwners =
        Z_Malloc(sizeof(lineowner_t) * map->numlines * 2, PU_LEVELSTATIC, 0);
    allocator = lineOwners;

    for(i = 0; i < map->numlines; ++i)
    {
        uint        p;
        line_t     *line = &map->lines[i];

        for(p = 0; p < 2; ++p)
        {
            vertex_t    *vtx = line->L_v(p);

            setVertexLineOwner(vtx, line, &allocator);
        }
    }

    // Sort line owners and then finish the rings.
    for(i = 0; i < map->numvertexes; ++i)
    {
        vertex_t   *v = &map->vertexes[i];

        // Line owners:
        if(v->numlineowners != 0)
        {
            lineowner_t *p, *last;
            binangle_t  lastAngle = 0;

            // Sort them so that they are ordered clockwise based on angle.
            rootVtx = v;
            v->lineowners =
                sortLineOwners(v->lineowners, lineAngleSorter);

            // Finish the linking job and convert to relative angles.
            // They are only singly linked atm, we need them to be doubly
            // and circularly linked.
            last = v->lineowners;
            p = last->LO_next;
            while(p)
            {
                p->LO_prev = last;

                // Convert to a relative angle between last and this.
                last->angle = last->angle - p->angle;
                lastAngle += last->angle;

                last = p;
                p = p->LO_next;
            }
            last->LO_next = v->lineowners;
            v->lineowners->LO_prev = last;

            // Set the angle of the last owner.
            last->angle = BANG_360 - lastAngle;
/*
#if _DEBUG
{
// For checking the line owner link rings are formed correctly.
lineowner_t *base;
uint        idx;

if(verbose >= 2)
    Con_Message("Vertex #%i: line owners #%i\n", i, v->numlineowners);

p = base = v->lineowners;
idx = 0;
do
{
    if(verbose >= 2)
        Con_Message("  %i: p= #%05i this= #%05i n= #%05i, dANG= %-3.f\n",
                    idx, p->LO_prev->line - map->lines,
                    p->line - map->lines,
                    p->LO_next->line - map->lines, BANG2DEG(p->angle));

    if(p->LO_prev->LO_next != p || p->LO_next->LO_prev != p)
       Con_Error("Invalid line owner link ring!");

    p = p->LO_next;
    idx++;
} while(p != base);
}
#endif
*/
        }
    }

    // How much time did we spend?
    VERBOSE(Con_Message
            ("buildVertexOwners: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));
}
