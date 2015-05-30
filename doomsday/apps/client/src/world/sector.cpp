/** @file sector.h  World map sector.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "world/sector.h"

#include <QList>
#include <QtAlgorithms>
#include <de/vector1.h>
#include <de/Log>

#include "world/map.h"
#include "world/p_object.h"
#include "Line"
#include "Plane"
#include "Surface"
#include "SectorCluster"

using namespace de;

DENG2_PIMPL(Sector)
, DENG2_OBSERVES(Plane, HeightChange)
{
    AABoxd aaBox;                     ///< Bounding box for the whole sector (all clusters).
    bool needAABoxUpdate = true;

    ThinkerT<SoundEmitter> emitter;   ///< Head of the sound emitter chain.

    typedef QList<Plane *> Planes;
    Planes planes;                    ///< All owned planes.

    typedef QList<LineSide *> Sides;
    Sides sides;                      ///< All referencing line sides (not owned).

    mobj_t *mobjList = nullptr;       ///< All mobjs "in" the sector (not owned).

    float lightLevel = 0;             ///< Ambient light level.
    Vector3f lightColor;              ///< Ambient light color.

    int validCount = 0;

#ifdef __CLIENT__
    coord_t roughArea = 0;            ///< Approximated. @c <0 means an update is needed.
    bool needRoughAreaUpdate = true;
#endif

    Instance(Public *i) : Base(i) {}
    ~Instance() { qDeleteAll(planes); }

    void notifyLightLevelChanged()
    {
        DENG2_FOR_PUBLIC_AUDIENCE(LightLevelChange, i) i->sectorLightLevelChanged(self);
    }

    void notifyLightColorChanged()
    {
        DENG2_FOR_PUBLIC_AUDIENCE(LightColorChange, i) i->sectorLightColorChanged(self);
    }

    /**
     * Update the axis-aligned bounding box in the map coordinate space to
     * encompass the geometry of all BSP leaf clusters of the sector.
     */
    void updateAABoxIfNeeded()
    {
        if(!needAABoxUpdate) return;
        needAABoxUpdate = false;

        aaBox.clear();
        bool haveGeometry = false;

        self.map().forAllClusters(thisPublic, [this, &haveGeometry] (SectorCluster &cluster)
        {
            if(haveGeometry)
            {
                V2d_UniteBox(aaBox.arvec2, cluster.aaBox().arvec2);
            }
            else
            {
                aaBox = cluster.aaBox();
                haveGeometry = true;
            }
            return LoopContinue;
        });

        // The XY origin of our sound emitter can now be updated as the center
        // point of the sector geometry is now known.
        if(haveGeometry)
        {
            emitter->origin[0] = (aaBox.minX + aaBox.maxX) / 2;
            emitter->origin[1] = (aaBox.minY + aaBox.maxY) / 2;
        }
        else
        {
            emitter->origin[0] = emitter->origin[1] = 0;
        }
    }

#ifdef __CLIENT__
    void updateRoughAreaIfNeeded()
    {
        if(!needRoughAreaUpdate) return;
        needRoughAreaUpdate = false;

        roughArea = 0;
        self.map().forAllClusters(thisPublic, [&] (SectorCluster &cluster)
        {
            roughArea += cluster.roughArea();
            return LoopContinue;
        });
    }
#endif // __CLIENT__

    /**
     * To be called to update sound emitter origins for all dependent surfaces.
     */
    void updateDependentSurfaceSoundEmitterOrigins()
    {
        for(LineSide *side : sides)
        {
            side->updateAllSoundEmitterOrigins();
            side->back().updateAllSoundEmitterOrigins();
        }
    }

    // Observes Plane HeightChange.
    void planeHeightChanged(Plane & /*plane*/)
    {
        // Update the z-height origin of our sound emitter right away.
        emitter->origin[2] = (self.floor().height() + self.ceiling().height()) / 2;

#ifdef __CLIENT__
        // A plane move means we must re-apply missing material fixes.
        for(LineSide *side : sides)
        {
            side->fixMissingMaterials();
            side->back().fixMissingMaterials();
        }
#endif

        updateDependentSurfaceSoundEmitterOrigins();
    }
};

Sector::Sector(float lightLevel, Vector3f const &lightColor)
    : MapElement(DMU_SECTOR)
    , d(new Instance(this))
{
    d->lightLevel = de::clamp(0.f, lightLevel, 1.f);
    d->lightColor = lightColor.min(Vector3f(1, 1, 1)).max(Vector3f(0, 0, 0));
}

float Sector::lightLevel() const
{
    return d->lightLevel;
}

void Sector::setLightLevel(float newLightLevel)
{
    newLightLevel = de::clamp(0.f, newLightLevel, 1.f);

    if(!de::fequal(d->lightLevel, newLightLevel))
    {
        d->lightLevel = newLightLevel;
        d->notifyLightLevelChanged();
    }
}

Vector3f const &Sector::lightColor() const
{
    return d->lightColor;
}

void Sector::setLightColor(Vector3f const &newLightColor)
{
    Vector3f newColorClamped(de::clamp(0.f, newLightColor.x, 1.f),
                             de::clamp(0.f, newLightColor.y, 1.f),
                             de::clamp(0.f, newLightColor.z, 1.f));

    if(d->lightColor != newColorClamped)
    {
        d->lightColor = newColorClamped;
        d->notifyLightColorChanged();
    }
}

struct mobj_s *Sector::firstMobj() const
{
    return d->mobjList;
}

/**
 * Two links to update:
 * 1) The link to the mobj from the previous node (sprev, always set) will
 *    be modified to point to the node following it.
 * 2) If there is a node following the mobj, set its sprev pointer to point
 *    to the pointer that points back to it (the mobj's sprev, just modified).
 */
void Sector::unlink(mobj_t *mobj)
{
    if(!mobj || !Mobj_IsSectorLinked(mobj))
        return;

    if((*mobj->sPrev = mobj->sNext))
        mobj->sNext->sPrev = mobj->sPrev;

    // Not linked any more.
    mobj->sNext = nullptr;
    mobj->sPrev = nullptr;

    // Ensure this has been completely unlinked.
#ifdef DENG2_DEBUG
    for(mobj_t *iter = d->mobjList; iter; iter = iter->sNext)
    {
        DENG2_ASSERT(iter != mobj);
    }
#endif
}

void Sector::link(mobj_t *mobj)
{
    if(!mobj) return;

    // Ensure this isn't already linked.
#ifdef DENG2_DEBUG
    for(mobj_t *iter = d->mobjList; iter; iter = iter->sNext)
    {
        DENG2_ASSERT(iter != mobj);
    }
#endif

    // Prev pointers point to the pointer that points back to us.
    // (Which practically disallows traversing the list backwards.)

    if((mobj->sNext = d->mobjList))
        mobj->sNext->sPrev = &mobj->sNext;

    *(mobj->sPrev = &d->mobjList) = mobj;
}

SoundEmitter &Sector::soundEmitter()
{
    // Emitter origin depends on the axis-aligned bounding box.
    d->updateAABoxIfNeeded();
    return d->emitter;
}

SoundEmitter const &Sector::soundEmitter() const
{
    return const_cast<SoundEmitter const &>(const_cast<Sector &>(*this).soundEmitter());
}

int Sector::validCount() const
{
    return d->validCount;
}

void Sector::setValidCount(int newValidCount)
{
    d->validCount = newValidCount;
}

bool Sector::hasSkyMaskedPlane() const
{
    for(Plane *plane : d->planes)
    {
        if(plane->surface().hasSkyMaskedMaterial())
            return true;
    }
    return false;
}

int Sector::planeCount() const
{
    return d->planes.count();
}

Plane &Sector::plane(int planeIndex)
{
    if(planeIndex >= 0 && planeIndex < d->planes.count())
    {
        return *d->planes.at(planeIndex);
    }
    /// @throw MissingPlaneError The referenced plane does not exist.
    throw MissingPlaneError("Sector::plane", QString("Missing plane %1").arg(planeIndex));
}

Plane const &Sector::plane(int planeIndex) const
{
    return const_cast<Sector *>(this)->plane(planeIndex);
}

Plane *Sector::addPlane(Vector3f const &normal, coord_t height)
{
    Plane *plane = new Plane(*this, normal, height);

    plane->setIndexInSector(d->planes.count());
    d->planes.append(plane);

    if(plane->isSectorFloor() || plane->isSectorCeiling())
    {
        // We want notification of height changes so that we can update sound
        // emitter origins of dependent surfaces.
        plane->audienceForHeightChange() += d;
    }

    // Once both floor and ceiling are known we can determine the z-height origin
    // of our sound emitter.
    /// @todo fixme: Assume planes are defined in order.
    if(planeCount() == 2)
    {
        d->emitter->origin[2] = (floor().height() + ceiling().height()) / 2;
    }

    return plane;
}

LoopResult Sector::forAllPlanes(std::function<LoopResult (Plane &)> func) const
{
    for(Plane *plane : d->planes)
    {
        if(auto result = func(*plane)) return result;
    }
    return LoopContinue;
}

int Sector::sideCount() const
{
    return d->sides.count();
}

LoopResult Sector::forAllSides(std::function<LoopResult (LineSide &)> func) const
{
    for(LineSide *side : d->sides)
    {
        if(auto result = func(*side)) return result;
    }
    return LoopContinue;
}

void Sector::buildSides()
{
    d->sides.clear();

#ifdef DENG2_QT_4_7_OR_NEWER
    int count = 0;
    map().forAllLines([this, &count] (Line &line)
    {
        if(line.frontSectorPtr() == this || line.backSectorPtr()  == this)
        {
            ++count;
        }
        return LoopContinue;
    });

    if(!count) return;

    d->sides.reserve(count);
#endif

    map().forAllLines([this] (Line &line)
    {
        if(line.frontSectorPtr() == this)
        {
            // Ownership of the side is not given to the sector.
            d->sides.append(&line.front());
        }
        else if(line.backSectorPtr()  == this)
        {
            // Ownership of the side is not given to the sector.
            d->sides.append(&line.back());
        }
        return LoopContinue;
    });
}

static void linkSoundEmitter(SoundEmitter &root, SoundEmitter &newEmitter)
{
    // The sector's base is always root of the chain, so link the other after it.
    newEmitter.thinker.prev = &root.thinker;
    newEmitter.thinker.next = root.thinker.next;
    if(newEmitter.thinker.next)
        newEmitter.thinker.next->prev = &newEmitter.thinker;
    root.thinker.next = &newEmitter.thinker;
}

void Sector::chainSoundEmitters()
{
    SoundEmitter &root = d->emitter;

    // Clear the root of the emitter chain.
    root.thinker.next = root.thinker.prev = nullptr;

    // Link plane surface emitters:
    for(Plane *plane : d->planes)
    {
        linkSoundEmitter(root, plane->soundEmitter());
    }

    // Link wall surface emitters:
    for(LineSide *side : d->sides)
    {
        if(side->hasSections())
        {
            linkSoundEmitter(root, side->middleSoundEmitter());
            linkSoundEmitter(root, side->bottomSoundEmitter());
            linkSoundEmitter(root, side->topSoundEmitter());
        }
        if(side->line().isSelfReferencing() && side->back().hasSections())
        {
            LineSide &back = side->back();
            linkSoundEmitter(root, back.middleSoundEmitter());
            linkSoundEmitter(root, back.bottomSoundEmitter());
            linkSoundEmitter(root, back.topSoundEmitter());
        }
    }
}

#ifdef __CLIENT__

AABoxd const &Sector::aaBox() const
{
    d->updateAABoxIfNeeded();
    return d->aaBox;
}

coord_t Sector::roughArea() const
{
    d->updateRoughAreaIfNeeded();
    return d->roughArea;
}

#endif // __CLIENT__

int Sector::property(DmuArgs &args) const
{
    switch(args.prop)
    {
    case DMU_LIGHT_LEVEL:
        args.setValue(DMT_SECTOR_LIGHTLEVEL, &d->lightLevel, 0);
        break;
    case DMU_COLOR:
        args.setValue(DMT_SECTOR_RGB, &d->lightColor.x, 0);
        args.setValue(DMT_SECTOR_RGB, &d->lightColor.y, 1);
        args.setValue(DMT_SECTOR_RGB, &d->lightColor.z, 2);
        break;
    case DMU_COLOR_RED:
        args.setValue(DMT_SECTOR_RGB, &d->lightColor.x, 0);
        break;
    case DMU_COLOR_GREEN:
        args.setValue(DMT_SECTOR_RGB, &d->lightColor.y, 0);
        break;
    case DMU_COLOR_BLUE:
        args.setValue(DMT_SECTOR_RGB, &d->lightColor.z, 0);
        break;
    case DMU_EMITTER: {
        SoundEmitter const *emitterAdr = d->emitter;
        args.setValue(DMT_SECTOR_EMITTER, &emitterAdr, 0);
        break; }
    case DMT_MOBJS:
        args.setValue(DMT_SECTOR_MOBJLIST, &d->mobjList, 0);
        break;
    case DMU_VALID_COUNT:
        args.setValue(DMT_SECTOR_VALIDCOUNT, &d->validCount, 0);
        break;
    case DMU_FLOOR_PLANE: {
        Plane *pln = d->planes.at(Floor);
        args.setValue(DMT_SECTOR_FLOORPLANE, &pln, 0);
        break; }
    case DMU_CEILING_PLANE: {
        Plane *pln = d->planes.at(Ceiling);
        args.setValue(DMT_SECTOR_CEILINGPLANE, &pln, 0);
        break; }
    default:
        return MapElement::property(args);
    }

    return false; // Continue iteration.
}

int Sector::setProperty(DmuArgs const &args)
{
    switch(args.prop)
    {
    case DMU_COLOR: {
        Vector3f newColor = d->lightColor;
        args.value(DMT_SECTOR_RGB, &newColor.x, 0);
        args.value(DMT_SECTOR_RGB, &newColor.y, 1);
        args.value(DMT_SECTOR_RGB, &newColor.z, 2);
        setLightColor(newColor);
        break; }
    case DMU_COLOR_RED: {
        Vector3f newColor = d->lightColor;
        args.value(DMT_SECTOR_RGB, &newColor.x, 0);
        setLightColor(newColor);
        break; }
    case DMU_COLOR_GREEN: {
        Vector3f newColor = d->lightColor;
        args.value(DMT_SECTOR_RGB, &newColor.y, 0);
        setLightColor(newColor);
        break; }
    case DMU_COLOR_BLUE: {
        Vector3f newColor = d->lightColor;
        args.value(DMT_SECTOR_RGB, &newColor.z, 0);
        setLightColor(newColor);
        break; }
    case DMU_LIGHT_LEVEL: {
        float newLightLevel;
        args.value(DMT_SECTOR_LIGHTLEVEL, &newLightLevel, 0);
        setLightLevel(newLightLevel);
        break; }
    case DMU_VALID_COUNT:
        args.value(DMT_SECTOR_VALIDCOUNT, &d->validCount, 0);
        break;
    default:
        return MapElement::setProperty(args);
    }

    return false; // Continue iteration.
}
