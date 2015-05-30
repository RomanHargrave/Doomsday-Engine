/** @file thingarchive.cpp  Map save state thing archive.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include "common.h"
#include "thingarchive.h"

#include "mobj.h"
#include "p_saveg.h" /// @todo remove me
#include <de/memory.h>

#if __JHEXEN__
/// Symbolic identifier used to mark references to players.
static ThingArchive::SerialId const TargetPlayerId = -2;
#endif

DENG2_PIMPL(ThingArchive)
{
    int version;
    uint size;
    mobj_t **things;
    bool excludePlayers;

    Instance(Public *i)
        : Base(i)
        , version(0)
        , size(0)
        , things(0)
        , excludePlayers(false)
    {}

    ~Instance()
    {
        self.clear();
    }

    struct countmobjthinkerstoarchive_params_t
    {
        uint count;
        bool excludePlayers;
    };

    static int countMobjThinkersToArchive(thinker_t *th, void *context)
    {
        countmobjthinkerstoarchive_params_t &p = *(countmobjthinkerstoarchive_params_t *) context;
        if(!(Mobj_IsPlayer((mobj_t *) th) && p.excludePlayers))
        {
            p.count++;
        }
        return false; // Continue iteration.
    }
};

ThingArchive::ThingArchive(int version) : d(new Instance(this))
{
    d->version = version;
}

int ThingArchive::version() const
{
    return d->version;
}

bool ThingArchive::excludePlayers() const
{
    return d->excludePlayers;
}

uint ThingArchive::size() const
{
    return d->size;
}

void ThingArchive::clear()
{
    M_Free(d->things); d->things = 0;
    d->size = 0;
}

void ThingArchive::initForLoad(uint size)
{
    d->size   = size;
    d->things = (mobj_t **)M_Calloc(d->size * sizeof(*d->things));
}

void ThingArchive::initForSave(bool excludePlayers)
{
    // Count the number of things we'll be writing.
    Instance::countmobjthinkerstoarchive_params_t parm; de::zap(parm);
    parm.count          = 0;
    parm.excludePlayers = excludePlayers;
    Thinker_Iterate((thinkfunc_t) P_MobjThinker, Instance::countMobjThinkersToArchive, &parm);

    d->size           = parm.count;
    d->things         = (mobj_t **)M_Calloc(d->size * sizeof(*d->things));
    d->excludePlayers = excludePlayers;
}

void ThingArchive::insert(mobj_t const *mo, SerialId serialId)
{
    DENG_ASSERT(mo != 0);

#if __JHEXEN__
    if(d->version >= 1)
#endif
    {
        serialId -= 1;
    }

#if __JHEXEN__
    // Only signed in Hexen.
    DENG2_ASSERT(serialId >= 0);
    if(serialId < 0) return; // Does this ever occur?
#endif

    DENG_ASSERT(d->things != 0);
    DENG_ASSERT((unsigned)serialId < d->size);
    d->things[serialId] = const_cast<mobj_t *>(mo);
}

ThingArchive::SerialId ThingArchive::serialIdFor(mobj_t const *mo)
{
    DENG_ASSERT(d->things != 0);

    if(!mo) return 0;

    // We only archive mobj thinkers.
    if(((thinker_t *) mo)->function != (thinkfunc_t) P_MobjThinker)
    {
        return 0;
    }

#if __JHEXEN__
    if(mo->player && d->excludePlayers)
    {
        return TargetPlayerId;
    }
#endif

    uint firstUnused = 0;
    bool found = false;
    for(uint i = 0; i < d->size; ++i)
    {
        if(!d->things[i] && !found)
        {
            firstUnused = i;
            found = true;
            continue;
        }

        if(d->things[i] == mo)
        {
            return i + 1;
        }
    }

    if(!found)
    {
        Con_Error("ThingArchive::serialIdFor: Thing archive exhausted!");
        return 0; // No number available!
    }

    // Insert it in the archive.
    d->things[firstUnused] = const_cast<mobj_t *>(mo);
    return firstUnused + 1;
}

mobj_t *ThingArchive::mobj(SerialId serialId, void *address)
{
#if !__JHEXEN__
    DENG_UNUSED(address);
#endif

#if __JHEXEN__
    if(serialId == TargetPlayerId)
    {
        targetplraddress_t *tpa = (targetplraddress_t *)M_Malloc(sizeof(targetplraddress_t));

        tpa->address = (void **)address;

        tpa->next = targetPlayerAddrs;
        targetPlayerAddrs = tpa;

        return 0;
    }
#endif

    DENG_ASSERT(d->things != 0);

#if __JHEXEN__
    if(d->version < 1)
    {
        // Old format (base 0).

        // A NULL reference?
        if(serialId == -1) return 0;

        if(serialId < 0 || (unsigned) serialId > d->size - 1)
            return 0;
    }
    else
#endif
    {
        // New format (base 1).

        // A NULL reference?
        if(serialId == 0) return 0;

        if(serialId < 1 || (unsigned) serialId > d->size)
        {
            App_Log(DE2_RES_WARNING, "ThingArchive::mobj: Invalid serialId %i", serialId);
            return 0;
        }

        serialId -= 1;
    }

    return d->things[serialId];
}
