/** @file bspleaf.cpp  World map BSP leaf half-space.
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

#include "de_base.h"
#include "world/bspleaf.h"
#ifdef __CLIENT__
#  include "world/map.h"
#endif
#include "ConvexSubspace"
#include "Sector"

using namespace de;

BspLeaf::BspLeaf(Sector *sector)
    : _sector  (sector)
    , _subspace(nullptr)
{}

bool BspLeaf::hasSubspace() const
{
    return _subspace != nullptr;
}

ConvexSubspace &BspLeaf::subspace() const
{
    if(hasSubspace())
    {
        return *_subspace;
    }
    /// @throw MissingSubspaceError Attempted with no subspace attributed.
    throw MissingSubspaceError("BspLeaf::subspace", "No subspace is attributed");
}

void BspLeaf::setSubspace(ConvexSubspace *newSubspace)
{
    if(_subspace == newSubspace) return;

    if(hasSubspace())
    {
        _subspace->setBspLeaf(nullptr);
    }

    _subspace = newSubspace;

    if(hasSubspace())
    {
        _subspace->setBspLeaf(this);
    }
}

Sector *BspLeaf::sectorPtr()
{
    return _sector;
}

Sector const *BspLeaf::sectorPtr() const
{
    return _sector;
}

void BspLeaf::setSector(Sector *newSector)
{
    _sector = newSector;
}
