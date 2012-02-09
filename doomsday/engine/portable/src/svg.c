/**\file svg.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include <assert.h>

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_render.h"

#include "svg.h"

struct Svg_s {
    /// Unique identifier for this graphic.
    svgid_t id;

    /// GL display list containing all commands for drawing all primitives (no state changes).
    DGLuint dlist;

    /// List of lines for this graphic.
    uint lineCount;
    SvgLine* lines;
};

void Svg_Delete(Svg* svg)
{
    uint i;
    assert(svg);

    Svg_Unload(svg);

    for(i = 0; i < svg->lineCount; ++i)
    {
        free(svg->lines[i].points);
    }
    free(svg->lines);
    free(svg);
}

svgid_t Svg_UniqueId(Svg* svg)
{
    assert(svg);
    return svg->id;
}

static void draw(const Svg* svg)
{
    GLenum nextPrimType, primType = GL_LINE_STRIP;
    const SvgLine* lIt;
    const Point2Rawf* pIt;
    uint i, j;

    LIBDENG_ASSERT_IN_MAIN_THREAD();

    lIt = svg->lines;
    for(i = 0; i < svg->lineCount; ++i, lIt++)
    {
        if(lIt->numPoints != 2)
        {
            nextPrimType = (lIt->flags & SLF_IS_LOOP)? GL_LINE_LOOP : GL_LINE_STRIP;

            // Do we need to end the current primitive?
            if(primType == GL_LINES)
            {
                glEnd(); // 2-vertex set ends.
            }

            // A new n-vertex primitive begins.
            glBegin(nextPrimType);
        }
        else
        {
            // Do we need to start a new 2-vertex primitive set?
            if(primType != GL_LINES)
            {
                primType = GL_LINES;
                glBegin(GL_LINES);
            }
        }

        // Write the vertex data.
        pIt = lIt->points;
        for(j = 0; j < lIt->numPoints; ++j, pIt++)
        {
            /// @todo Use TexGen?
            glTexCoord2dv((const GLdouble*)pIt->xy);
            glVertex2dv((const GLdouble*)pIt->xy);
        }

        if(lIt->numPoints != 2)
        {
            glEnd(); // N-vertex primitive ends.
        }
    }

    if(primType == GL_LINES)
    {
        // Close any remaining open 2-vertex set.
        glEnd();
    }
}

static DGLuint constructDisplayList(DGLuint name, const Svg* svg)
{
    if(GL_NewList(name, DGL_COMPILE))
    {
        draw(svg);
        return GL_EndList();
    }
    return 0;
}

void Svg_Draw(Svg* svg)
{
    assert(svg);

    if(novideo || isDedicated)
    {
        assert(0); // Should not have been called!
        return;
    }

    // Have we uploaded our draw-optimized representation yet?
    if(svg->dlist)
    {
        // Draw!
        GL_CallList(svg->dlist);
        return;
    }

    // Draw manually in so-called 'immediate' mode.
    draw(svg);
}

boolean Svg_Prepare(Svg* svg)
{
    assert(svg);
    if(!novideo && !isDedicated)
    {
        if(!svg->dlist)
        {
            svg->dlist = constructDisplayList(0, svg);
        }
    }
    return !!svg->dlist;
}

void Svg_Unload(Svg* svg)
{
    assert(svg);
    if(novideo || isDedicated) return;

    if(svg->dlist)
    {
        GL_DeleteLists(svg->dlist, 1);
        svg->dlist = 0;
    }
}

Svg* Svg_FromDef(svgid_t uniqueId, const def_svgline_t* lines, uint lineCount)
{
    const def_svgline_t* slIt;
    const Point2Rawf* spIt;
    uint finalLineCount;
    Point2Rawf* dpIt;
    SvgLine* dlIt;
    uint i, j;
    Svg* svg;

    if(!lines || lineCount == 0) return NULL;

    svg = (Svg*)malloc(sizeof(*svg));
    if(!svg) Con_Error("Svg::FromDef: Failed on allocation of %lu bytes for new Svg.", (unsigned long) sizeof(*svg));

    svg->id = uniqueId;
    svg->dlist = 0;

    // Count how many lines we actually need.
    finalLineCount = 0;
    slIt = lines;
    for(i = 0; i < lineCount; ++i, slIt++)
    {
        // Skip lines with missing vertices...
        if(slIt->numPoints < 2) continue;

        ++finalLineCount;
    }

    // Allocate the final line set.
    svg->lineCount = finalLineCount;
    svg->lines = (SvgLine*)malloc(sizeof(*svg->lines) * finalLineCount);
    if(!svg->lines) Con_Error("Svg::FromDef: Failed on allocation of %lu bytes for new SvgLine list.", (unsigned long) (sizeof(*svg->lines) * finalLineCount));

    // Setup the lines.
    slIt = lines;
    dlIt = svg->lines;
    for(i = 0; i < finalLineCount; ++i, slIt++)
    {
        // Skip lines with missing vertices...
        if(slIt->numPoints <= 1) continue;

        dlIt->flags = 0;

        // Determine how many points we'll need.
        dlIt->numPoints = slIt->numPoints;
        if(slIt->numPoints > 2)
        {
            // If the end point is equal to the start point, we'll ommit it and
            // set this line up as a loop.
            if(FEQUAL(slIt->points[slIt->numPoints-1].x, slIt->points[0].x) &&
               FEQUAL(slIt->points[slIt->numPoints-1].y, slIt->points[0].y))
            {
                dlIt->numPoints -= 1;
                dlIt->flags |= SLF_IS_LOOP;
            }
        }

        // Allocate points.
        /// @todo Calculate point total and allocate them all using a continuous
        ///       region of memory.
        dlIt->points = (Point2Rawf*)malloc(sizeof(*dlIt->points) * dlIt->numPoints);
        if(!dlIt->points) Con_Error("Svg::FromDef: Failed on allocation of %lu bytes for new SvgLine Point list.", (unsigned long) (sizeof(*dlIt->points) * dlIt->numPoints));

        // Copy points.
        spIt = slIt->points;
        dpIt = dlIt->points;
        for(j = 0; j < dlIt->numPoints; ++j, spIt++, dpIt++)
        {
            dpIt->x = spIt->x;
            dpIt->y = spIt->y;
        }

        // On to the next line!
        dlIt++;
    }

    return svg;
}
