/** @file linesighttest.h  World map line of sight testing.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_WORLD_LINE_SIGHT_TEST_H
#define DENG_WORLD_LINE_SIGHT_TEST_H

#include <de/libcore.h>
#include <de/Vector>
#include "world/map.h"

namespace de {

/**
 * Models the logic, parameters and state of a line (of) sight (LOS) test.
 *
 * @todo fixme: The state of a discrete trace is not fully encapsulated here
 * due to the manipulation of the validCount properties of the various map data elements.
 * (Which is used to avoid testing the same element multiple times during a trace.)
 *
 * @todo optimize: Make use of the blockmap to take advantage of the inherent spatial
 * locality in this data structure.
 *
 * @ingroup world
 */
class LineSightTest
{
public:
    typedef Map::BspTree BspTree;

public:
    /**
     * Constructs a new line (of) sight test.
     *
     * @param from         Trace origin point in the map coordinate space.
     * @param to           Trace target point in the map coordinate space.
     * @param bottomSlope  Lower limit to the Z axis angle/slope range.
     * @param topSlope     Upper limit to the Z axis angle/slope range.
     * @param flags        @ref lineSightFlags dictate trace behavior/logic.
     */
    LineSightTest(Vector3d const &from, Vector3d const &to,
                  dfloat bottomSlope = -1,
                  dfloat topSlope    = +1,
                  dint flags         = 0);

    /**
     * Execute the trace (i.e., cast the ray).
     *
     * @param bspRoot  Root of BSP to be traced.
     *
     * @return  @c true iff an uninterrupted path exists between the preconfigured Start
     * and End points of the trace line.
     */
    bool trace(BspTree const &bspRoot);

private:
    DENG2_PRIVATE(d)
};

}  // namespace de

#endif  // DENG_WORLD_LINE_SIGHT_TEST_H
