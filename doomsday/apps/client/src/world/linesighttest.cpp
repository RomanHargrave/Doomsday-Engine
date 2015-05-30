/** @file linesighttest.cpp  World map line of sight testing.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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
#include "world/linesighttest.h"

#include <cmath>
#include <de/aabox.h>
#include <de/fixedpoint.h>
#include <de/vector1.h>

#include "Face"

#include "world/worldsystem.h"  /// For validCount, @todo Remove me.
#include "BspLeaf"
#include "BspNode"
#include "ConvexSubspace"
#include "Line"
#include "Polyobj"
#include "Sector"

using namespace de;

DENG2_PIMPL_NOREF(LineSightTest)
{
    dint flags = 0;      ///< LS_* flags @ref lineSightFlags
    Vector3d from;       ///< Ray origin.
    Vector3d to;         ///< Ray target.
    dfloat bottomSlope;  ///< Slope to bottom of target.
    dfloat topSlope;     ///< Slope to top of target.

    /// The ray to be traced.
    struct Ray
    {
        fixed_t origin[2];
        fixed_t direction[2];
        AABoxd aabox;

        Ray(Vector3d const &from, Vector3d const &to)
        {
            origin[0]    = DBL2FIX(from.x);
            origin[1]    = DBL2FIX(from.y);
            direction[0] = DBL2FIX(to.x - from.x);
            direction[1] = DBL2FIX(to.y - from.y);

            ddouble v1From[2] = { from.x, from.y };
            V2d_InitBox(aabox.arvec2, v1From);

            ddouble v1To[2] = { to.x, to.y };
            V2d_AddToBox(aabox.arvec2, v1To);
        }
    } ray;

    Instance(Vector3d const &from, Vector3d const to, dfloat bottomSlope, dfloat topSlope)
        : from       (from)
        , to         (to)
        , bottomSlope(bottomSlope)
        , topSlope   (topSlope)
        , ray        (from, to)
    {}

    /**
     * @return  @c true if the ray passes the line @a side; otherwise @c false.
     *
     * @todo cleanup: Much unnecessary representation flipping...
     * @todo cleanup: Remove front-side assumption.
     */
    bool crossLine(LineSide &side)
    {
#define RTOP                    0x1  ///< Top range.
#define RBOTTOM                 0x2  ///< Bottom range.

        Line &line = side.line();

        if(line.validCount() == validCount)
            return true;  // Ignore

        line.setValidCount(validCount);

        // Does the ray intercept the line on the X/Y plane?
        // Try a quick bounding-box rejection.
        if(line.aaBox().minX > ray.aabox.maxX ||
           line.aaBox().maxX < ray.aabox.minX ||
           line.aaBox().minY > ray.aabox.maxY ||
           line.aaBox().maxY < ray.aabox.minY)
            return true;

        fixed_t const lineV1OriginX[2]  = { DBL2FIX(line.fromOrigin().x), DBL2FIX(line.fromOrigin().y) };
        fixed_t const lineV2OriginX[2]  = { DBL2FIX(line.toOrigin  ().x), DBL2FIX(line.toOrigin  ().y) };

        if(V2x_PointOnLineSide(lineV1OriginX, ray.origin, ray.direction) ==
           V2x_PointOnLineSide(lineV2OriginX, ray.origin, ray.direction))
            return true;

        fixed_t lineDirectionX[2] = { DBL2FIX(line.direction().x), DBL2FIX(line.direction().y) };

        fixed_t const fromPointX[2] = { DBL2FIX(from.x), DBL2FIX(from.y) };
        fixed_t const toPointX[2]   = { DBL2FIX(to  .x), DBL2FIX(to  .y) };

        if(V2x_PointOnLineSide(fromPointX, lineV1OriginX, lineDirectionX) ==
           V2x_PointOnLineSide(toPointX, lineV1OriginX, lineDirectionX))
            return true;

        // Is this the passable side of a one-way BSP window?
        if(!side.hasSections())
            return true;

        if(!side.hasSector())
            return false;

        Sector const *frontSec = side.sectorPtr();
        Sector const *backSec  = side.back().sectorPtr();

        bool noBack = side.considerOneSided();

        if(!noBack && !(flags & LS_PASSLEFT))
        {
            noBack = (!( backSec->floor().height() < frontSec->ceiling().height()) ||
                      !(frontSec->floor().height() <  backSec->ceiling().height()));
        }

        if(noBack)
        {
            // Does the ray pass from left to right?
            if(flags & LS_PASSLEFT) // Allowed.
            {
                if(line.pointOnSide(Vector2d(from.x, from.y)) < 0)
                    return true;
            }

            // No back side is present so if the ray is not allowed to pass over/under
            // the line then end it right here.
            if(!(flags & (LS_PASSOVER | LS_PASSUNDER)))
                return false;
        }

        // Handle the case of a zero height back side in the top range.
        dbyte ranges = 0;
        if(noBack)
        {
            ranges |= RTOP;
        }
        else
        {
            if(backSec->floor().height()   != frontSec->floor().height())
                ranges |= RBOTTOM;

            if(backSec->ceiling().height() != frontSec->ceiling().height())
                ranges |= RTOP;
        }

        // No partially closed ranges which require testing?
        if(!ranges)
            return true;

        dfloat frac = FIX2FLT(V2x_Intersection(lineV1OriginX, lineDirectionX, ray.origin, ray.direction));

        // Does the ray pass over the top range?
        if(flags & LS_PASSOVER) // Allowed.
        {
            if(bottomSlope > (frontSec->ceiling().height() - from.z) / frac)
                return true;
        }

        // Does the ray pass under the bottom range?
        if(flags & LS_PASSUNDER) // Allowed.
        {
            if(topSlope    < (  frontSec->floor().height() - from.z) / frac)
                return true;
        }

        // Test a partially closed top range?
        if(ranges & RTOP)
        {
            dfloat const top =                                      noBack ? frontSec->ceiling().height() :
                 frontSec->ceiling().height() < backSec->ceiling().height()? frontSec->ceiling().height() :
                                                                             backSec->ceiling().height();

            dfloat const slope = (top - from.z) / frac;

            if((slope < topSlope)     ^ (noBack && !(flags & LS_PASSOVER)) ||
               (noBack && topSlope    > (frontSec->floor().height() - from.z) / frac))
                topSlope = slope;

            if((slope < bottomSlope)  ^ (noBack && !(flags & LS_PASSUNDER)) ||
               (noBack && bottomSlope > (frontSec->floor().height() - from.z) / frac))
                bottomSlope = slope;
        }

        // Test a partially closed bottom range?
        if(ranges & RBOTTOM)
        {
            dfloat const bottom =                                noBack? frontSec->floor().height() :
                 frontSec->floor().height() > backSec->floor().height()? frontSec->floor().height() :
                                                                         backSec->floor().height();
            dfloat const slope = (bottom - from.z) / frac;

            if(slope > bottomSlope)
                bottomSlope = slope;

            if(slope > topSlope)
                topSlope = slope;
        }

        return topSlope <= bottomSlope? false : true;

#undef RTOP
#undef RBOTTOM
    }

    /**
     * @return  @c true if the ray passes @a subspace; otherwise @c false.
     */
    bool crossSubspace(ConvexSubspace const &subspace)
    {
        // Check polyobj lines.
        LoopResult blocked = subspace.forAllPolyobjs([this] (Polyobj &pob)
        {
            for(Line *line : pob.lines())
            {
                if(!crossLine(line->front()))
                    return LoopAbort;
            }
            return LoopContinue;
        });
        if(blocked) return false;

        // Check lines for the edges of the subspace geometry.
        HEdge *base  = subspace.poly().hedge();
        HEdge *hedge = base;
        do
        {
            if(hedge->hasMapElement())
            {
                if(!crossLine(hedge->mapElementAs<LineSideSegment>().lineSide()))
                    return false;
            }
        } while((hedge = &hedge->next()) != base);

        // Check lines for the extra meshes.
        blocked = subspace.forAllExtraMeshes([this] (Mesh &mesh)
        {
            for(HEdge *hedge : mesh.hedges())
            {
                // Is this on the back of a one-sided line?
                if(!hedge->hasMapElement())
                    continue;

                if(!crossLine(hedge->mapElementAs<LineSideSegment>().lineSide()))
                    return LoopAbort;
            }
            return LoopContinue;
        });

        return !blocked;
    }

    /**
     * @return  @c true if the ray passes @a bspTree; otherwise @c false.
     */
    bool crossBspNode(BspTree const *bspTree)
    {
        DENG2_ASSERT(bspTree);

        while(!bspTree->isLeaf())
        {
            DENG2_ASSERT(bspTree->userData());
            BspNode const &bspNode = bspTree->userData()->as<BspNode>();

            // Does the ray intersect the partition?
            /// @todo Optionally use the fixed precision version -ds
            dint const fromSide = bspNode.partition().pointOnSide(Vector2d(from.x, from.y)) < 0;
            dint const toSide   = bspNode.partition().pointOnSide(Vector2d(to.x, to.y)) < 0;
            if(fromSide != toSide)
            {
                // Yes.
                if(!crossBspNode(bspTree->childPtr(BspTree::ChildId(fromSide))))
                    return false;  // Cross the From side.

                bspTree = bspTree->childPtr(BspTree::ChildId(fromSide ^ 1));  // Cross the To side.
            }
            else
            {
                // No - descend!
                bspTree = bspTree->childPtr(BspTree::ChildId(fromSide));
            }
        }

        // We've arrived at a leaf.
        auto const &bspLeaf = bspTree->userData()->as<BspLeaf>();

        if(bspLeaf.hasSubspace())
        {
            return crossSubspace(bspLeaf.subspace());
        }

        // No subspace geometry implies a mapping error.
        return false;
    }
};

LineSightTest::LineSightTest(Vector3d const &from, Vector3d const &to, dfloat bottomSlope,
    dfloat topSlope, dint flags)
    : d(new Instance(from, to, bottomSlope, topSlope))
{
    d->flags = flags;
}

bool LineSightTest::trace(BspTree const &bspRoot)
{
    validCount++;

    d->topSlope    = d->to.z + d->topSlope    - d->from.z;
    d->bottomSlope = d->to.z + d->bottomSlope - d->from.z;

    return d->crossBspNode(&bspRoot);
}
