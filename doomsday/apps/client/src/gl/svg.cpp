/** @file svg.cpp Scalable Vector Graphic (SVG) implementation. 
 * @ingroup gl
 *
 * @authors Copyright &copy; 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2013 Daniel Swanson <danij@dengine.net>
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

#include <assert.h>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_system.h"
#include "gl/sys_opengl.h"
#include "gl/gl_main.h"

#include "gl/svg.h"

typedef struct svglinepoint_s {
    /// Next and previous points on this line.
    struct svglinepoint_s* next, *prev;

    /// Coordinates for this line in the normalized coordinate space of the owning SVG.
    Point2Rawf coords;
} SvgLinePoint;

struct svgline_s {
    /// Total number of points for this line.
    uint numPoints;

    /// Head of the list of points for this line.
    SvgLinePoint* head;
};

struct svg_s {
    /// Unique identifier for this graphic.
    svgid_t id;

    /// GL display list containing all commands for drawing all primitives (no state changes).
    DGLuint dlist;

    /// Set of lines for this graphic.
    uint lineCount;
    SvgLine* lines;

    /// Set of points for this graphic.
    uint numPoints;
    SvgLinePoint* points;
};

dd_bool SvgLine_IsLoop(const SvgLine* line)
{
    assert(line);
    return line->head && line->head->prev != NULL;
}

void Svg_Delete(Svg* svg)
{
    assert(svg);

    Svg_Unload(svg);

    free(svg->lines);
    free(svg->points);
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
    uint i;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    lIt = svg->lines;
    for(i = 0; i < svg->lineCount; ++i, lIt++)
    {
        if(lIt->numPoints != 2)
        {
            nextPrimType = SvgLine_IsLoop(lIt)? GL_LINE_LOOP : GL_LINE_STRIP;

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
        if(lIt->head)
        {
            const SvgLinePoint* pIt = lIt->head;
            do
            {
                /// @todo Use TexGen?
                glTexCoord2dv((const GLdouble*)pIt->coords.xy);
                glVertex2dv((const GLdouble*)pIt->coords.xy);
            } while(NULL != (pIt = pIt->next) && pIt != lIt->head);
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

dd_bool Svg_Prepare(Svg* svg)
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
    uint finalLineCount, finalPointCount;
    const def_svgline_t* slIt;
    const Point2Rawf* spIt;
    dd_bool lineIsLoop;
    SvgLinePoint* dpIt, *prev;
    SvgLine* dlIt;
    uint i, j;
    Svg* svg;

    if(!lines || lineCount == 0) return NULL;

    svg = (Svg*)malloc(sizeof(*svg));
    if(!svg) App_Error("Svg::FromDef: Failed on allocation of %lu bytes for new Svg.", (unsigned long) sizeof(*svg));

    svg->id = uniqueId;
    svg->dlist = 0;

    // Count how many lines and points we actually need.
    finalLineCount = 0;
    finalPointCount = 0;
    slIt = lines;
    for(i = 0; i < lineCount; ++i, slIt++)
    {
        // Skip lines with missing vertices...
        if(slIt->numPoints < 2) continue;

        ++finalLineCount;

        finalPointCount += slIt->numPoints;
        if(slIt->numPoints > 2)
        {
            // If the end point is equal to the start point, we'll ommit it and
            // set this line up as a loop.
            if(FEQUAL(slIt->points[slIt->numPoints-1].x, slIt->points[0].x) &&
               FEQUAL(slIt->points[slIt->numPoints-1].y, slIt->points[0].y))
            {
                finalPointCount -= 1;
            }
        }
    }

    // Allocate the final point set.
    svg->numPoints = finalPointCount;
    svg->points = (SvgLinePoint*)malloc(sizeof(*svg->points) * svg->numPoints);
    if(!svg->points) App_Error("Svg::FromDef: Failed on allocation of %lu bytes for new SvgLinePoint set.", (unsigned long) (sizeof(*svg->points) * finalPointCount));

    // Allocate the final line set.
    svg->lineCount = finalLineCount;
    svg->lines = (SvgLine*)malloc(sizeof(*svg->lines) * finalLineCount);
    if(!svg->lines) App_Error("Svg::FromDef: Failed on allocation of %lu bytes for new SvgLine set.", (unsigned long) (sizeof(*svg->lines) * finalLineCount));

    // Setup the lines.
    slIt = lines;
    dlIt = svg->lines;
    dpIt = svg->points;
    for(i = 0; i < lineCount; ++i, slIt++)
    {
        // Skip lines with missing vertices...
        if(slIt->numPoints < 2) continue;

        // Determine how many points we'll need.
        dlIt->numPoints = slIt->numPoints;
        lineIsLoop = false;
        if(slIt->numPoints > 2)
        {
            // If the end point is equal to the start point, we'll ommit it and
            // set this line up as a loop.
            if(FEQUAL(slIt->points[slIt->numPoints-1].x, slIt->points[0].x) &&
               FEQUAL(slIt->points[slIt->numPoints-1].y, slIt->points[0].y))
            {
                dlIt->numPoints -= 1;
                lineIsLoop = true;
            }
        }

        // Copy points.
        spIt = slIt->points;
        dlIt->head = dpIt;
        prev = NULL;
        for(j = 0; j < dlIt->numPoints; ++j, spIt++)
        {
            SvgLinePoint* next = (j < dlIt->numPoints-1)? dpIt + 1 : NULL;

            dpIt->coords.x = spIt->x;
            dpIt->coords.y = spIt->y;

            // Link in list.
            dpIt->next = next;
            dpIt->prev = prev;

            // On to the next point!
            prev = dpIt;
            dpIt++;
        }

        // Link circularly?
        prev->next = lineIsLoop? dlIt->head : NULL;
        dlIt->head->prev = lineIsLoop? prev : NULL;

        // On to the next line!
        dlIt++;
    }

    return svg;
}
