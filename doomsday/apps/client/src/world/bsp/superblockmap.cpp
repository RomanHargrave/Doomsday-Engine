/** @file superblockmap.cpp  BSP line segment blockmap block.
 *
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @authors Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @authors Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * @authors Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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

#include "world/bsp/superblockmap.h"
#include <de/vector1.h>

using namespace de;
using namespace de::bsp;

DENG2_PIMPL_NOREF(LineSegmentBlock)
{
    AABox bounds;       ///< Block bounds at the node.
    All segments;       ///< Line segments contained by the node (not owned).
    int mapCount  = 0;  ///< Running total of map-line segments at/under this node.
    int partCount = 0;  ///< Running total of partition-line segments at/under this node.
};

LineSegmentBlock::LineSegmentBlock(AABox const &bounds) : d(new Instance)
{
    d->bounds = bounds;
}

AABox const &LineSegmentBlock::bounds() const
{
    return d->bounds;
}

void LineSegmentBlock::link(LineSegmentSide &seg)
{
    d->segments.prepend(&seg);
}

void LineSegmentBlock::addRef(LineSegmentSide const &seg)
{
    if(seg.hasMapSide()) d->mapCount++;
    else                 d->partCount++;
}

void LineSegmentBlock::decRef(LineSegmentSide const &seg)
{
    if(seg.hasMapSide()) d->mapCount--;
    else                 d->partCount--;
}

LineSegmentSide *LineSegmentBlock::pop()
{
    if(!d->segments.isEmpty())
    {
        LineSegmentSide *seg = d->segments.takeFirst();
        decRef(*seg);
        return seg;
    }
    return 0;
}

int LineSegmentBlock::mapCount() const
{
    return d->mapCount;
}

int LineSegmentBlock::partCount() const
{
    return d->partCount;
}

int LineSegmentBlock::totalCount() const
{
    return d->mapCount + d->partCount;
}

LineSegmentBlock::All const &LineSegmentBlock::all() const
{
    return d->segments;
}
