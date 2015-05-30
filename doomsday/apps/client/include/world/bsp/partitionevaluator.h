/** @file partitionevaluator.h  Evaluator for a would-be BSP.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_BSP_PARTITIONEVALUATOR_H
#define DENG_WORLD_BSP_PARTITIONEVALUATOR_H

#include "world/bsp/superblockmap.h"

namespace de {
namespace bsp {

/**
 * Evaluates the suitability of a would-be partition, determining a cost metric
 * which takes into account the number of splits and the resulting tree balance
 * post partitioning.
 */
class PartitionEvaluator
{
public:
    /**
     * @param splitCostFactor  Split cost multiplier.
     */
    PartitionEvaluator(int splitCostFactor);

    /**
     * Find the best line segment to use as the next partition.
     *
     * @param node  Block tree node containing the remaining line segments.
     *
     * @return  The chosen partition line.
     */
    LineSegmentSide *choose(LineSegmentBlockTreeNode &node);

private:
    DENG2_PRIVATE(d)
};

} // namespace bsp
} // namespace de

#endif // DENG_WORLD_BSP_PARTITIONEVALUATOR_H
