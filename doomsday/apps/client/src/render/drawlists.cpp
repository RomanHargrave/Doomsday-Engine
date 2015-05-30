/** @file drawlists.cpp  Drawable primitive list collection/management.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h"
#include "render/drawlists.h"

#include <de/Log>
#include <de/memoryzone.h>
#include <QMultiHash>
#include <QtAlgorithms>

using namespace de;

typedef QMultiHash<GLuint, DrawList *> DrawListHash;

DENG2_PIMPL(DrawLists)
{
    QScopedPointer<DrawList> skyMaskList;
    DrawListHash unlitHash;
    DrawListHash litHash;
    DrawListHash dynHash;
    DrawListHash shinyHash;
    DrawListHash shadowHash;

    Instance(Public *i) : Base(i)
    {
        DrawListSpec newSpec;
        newSpec.group = SkyMaskGeom;
        skyMaskList.reset(new DrawList(newSpec));
    }

    /// Choose the correct draw list hash table.
    DrawListHash &listHash(GeomGroup group)
    {
        switch(group)
        {
        case UnlitGeom:  return unlitHash;
        case LitGeom:    return litHash;
        case LightGeom:  return dynHash;
        case ShadowGeom: return shadowHash;
        case ShineGeom:  return shinyHash;

        case SkyMaskGeom: break; // n/a?
        }
        DENG2_ASSERT(false);
        return unlitHash;
    }
};

DrawLists::DrawLists() : d(new Instance(this))
{}

static void clearAllLists(DrawListHash &hash)
{
    foreach(DrawList *list, hash)
    {
        list->clear();
    }
    qDeleteAll(hash);
    hash.clear();
}

void DrawLists::clear()
{
    clearAllLists(d->unlitHash);
    clearAllLists(d->litHash);
    clearAllLists(d->dynHash);
    clearAllLists(d->shadowHash);
    clearAllLists(d->shinyHash);
    d->skyMaskList->clear();
}

static void resetList(DrawList &list)
{
    list.rewind();

    // Reset the list specification.
    // The interpolation target must be explicitly set.
    DrawListSpec &listSpec = list.spec();
    listSpec.unit(TU_INTER).unmanaged.glName = 0;
    listSpec.unit(TU_INTER).texture          = 0;
    listSpec.unit(TU_INTER).opacity          = 0;

    listSpec.unit(TU_INTER_DETAIL).unmanaged.glName = 0;
    listSpec.unit(TU_INTER_DETAIL).texture          = 0;
    listSpec.unit(TU_INTER_DETAIL).opacity          = 0;
}

static void resetAllLists(DrawListHash &hash)
{
    foreach(DrawList *list, hash)
    {
        resetList(*list);
    }
}

void DrawLists::reset()
{
    resetAllLists(d->unlitHash);
    resetAllLists(d->litHash);
    resetAllLists(d->dynHash);
    resetAllLists(d->shadowHash);
    resetAllLists(d->shinyHash);
    resetList(*d->skyMaskList);
}

/**
 * Specialized texture unit comparision function that ignores properties which
 * are applied per-primitive and which should not result in list separation.
 *
 * (These properties are written to the primitive header and applied dynamically
 * when reading back the draw list.)
 */
static bool compareTexUnit(GLTextureUnit const &lhs, GLTextureUnit const &rhs)
{
    if(lhs.texture)
    {
        if(lhs.texture != rhs.texture) return false;
    }
    else
    {
        if(lhs.unmanaged != rhs.unmanaged) return false;
    }
    if(!de::fequal(lhs.opacity, rhs.opacity)) return false;
    // Other properties are applied per-primitive and should not affect the outcome.
    return true;
}

DrawList &DrawLists::find(DrawListSpec const &spec)
{
    // Sky masked geometry is never textured; therefore no draw list hash.
    /// @todo Make hash management dynamic. -ds
    if(spec.group == SkyMaskGeom)
    {
        return *d->skyMaskList;
    }

    DrawList *convertable = 0;

    // Find/create a list in the hash.
    GLuint const key  = spec.unit(TU_PRIMARY).getTextureGLName();
    DrawListHash &hash = d->listHash(spec.group);
    for(DrawListHash::const_iterator it = hash.find(key);
        it != hash.end() && it.key() == key; ++it)
    {
        DrawList *list = it.value();
        DrawListSpec const &listSpec = list->spec();

        if((spec.group == ShineGeom &&
            compareTexUnit(listSpec.unit(TU_PRIMARY), spec.unit(TU_PRIMARY))) ||
           (spec.group != ShineGeom &&
            compareTexUnit(listSpec.unit(TU_PRIMARY), spec.unit(TU_PRIMARY)) &&
            compareTexUnit(listSpec.unit(TU_PRIMARY_DETAIL), spec.unit(TU_PRIMARY_DETAIL))))
        {
            if(!listSpec.unit(TU_INTER).hasTexture() &&
               !spec.unit(TU_INTER).hasTexture())
            {
                // This will do great.
                return *list;
            }

            // Is this eligible for conversion to a blended list?
            if(list->isEmpty() && !convertable && spec.unit(TU_INTER).hasTexture())
            {
                // If necessary, this empty list will be selected.
                convertable = list;
            }

            // Possibly an exact match?
            if((spec.group == ShineGeom &&
                compareTexUnit(listSpec.unit(TU_INTER), spec.unit(TU_INTER))) ||
               (spec.group != ShineGeom &&
                compareTexUnit(listSpec.unit(TU_INTER), spec.unit(TU_INTER)) &&
                compareTexUnit(listSpec.unit(TU_INTER_DETAIL), spec.unit(TU_INTER_DETAIL))))
            {
                return *list;
            }
        }
    }

    // Did we find a convertable list?
    if(convertable)
    {
        // This list is currently empty.
        if(spec.group == ShineGeom)
        {
            convertable->spec().unit(TU_INTER) = spec.unit(TU_INTER);
        }
        else
        {
            convertable->spec().unit(TU_INTER) = spec.unit(TU_INTER);
            convertable->spec().unit(TU_INTER_DETAIL) = spec.unit(TU_INTER_DETAIL);
        }

        return *convertable;
    }

    // Create a new list.
    return *hash.insert(key, new DrawList(spec)).value();
}

int DrawLists::findAll(GeomGroup group, FoundLists &found)
{
    LOG_AS("DrawLists::findAll");

    found.clear();
    if(group == SkyMaskGeom)
    {
        if(!d->skyMaskList->isEmpty())
        {
            found.append(d->skyMaskList.data());
        }
    }
    else
    {
        DrawListHash const &hash = d->listHash(group);
        foreach(DrawList *list, hash)
        {
            if(!list->isEmpty())
            {
                found.append(list);
            }
        }
    }

    return found.count();
}
