/** @file cl_world.cpp  Clientside world management.
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

#include "de_base.h"
#include "client/cl_world.h"

#include "client/cl_player.h"

#include "api_map.h"
#include "api_materialarchive.h"

#include "network/net_msg.h"
#include "network/protocol.h"

#include "world/map.h"
#include "Sector"
#include "Surface"

#include <QVector>

using namespace de;

static MaterialArchive *serverMaterials;

typedef QVector<int> IndexTransTable;
static IndexTransTable xlatMobjType;
static IndexTransTable xlatMobjState;

void Cl_InitTransTables()
{
    serverMaterials = 0;
    xlatMobjType.clear();
    xlatMobjState.clear();
}

void Cl_ResetTransTables()
{
    if(serverMaterials)
    {
        MaterialArchive_Delete(serverMaterials);
        serverMaterials = 0;
    }

    xlatMobjType.clear();
    xlatMobjState.clear();
}

void Cl_ReadServerMaterials()
{
    LOG_AS("Cl_ReadServerMaterials");

    if(!serverMaterials)
    {
        serverMaterials = MaterialArchive_NewEmpty(false /*no segment check*/);
    }
    MaterialArchive_Read(serverMaterials, msgReader, -1 /*no forced version*/);

    LOGDEV_NET_VERBOSE("Received %i materials") << MaterialArchive_Count(serverMaterials);
}

void Cl_ReadServerMobjTypeIDs()
{
    LOG_AS("Cl_ReadServerMobjTypeIDs");

    StringArray *ar = StringArray_New();
    StringArray_Read(ar, msgReader);

    LOGDEV_NET_VERBOSE("Received %i mobj type IDs") << StringArray_Size(ar);

    xlatMobjType.resize(StringArray_Size(ar));

    // Translate the type IDs to local.
    for(int i = 0; i < StringArray_Size(ar); ++i)
    {
        xlatMobjType[i] = Def_GetMobjNum(StringArray_At(ar, i));
        if(xlatMobjType[i] < 0)
        {
            LOG_NET_WARNING("Could not find '%s' in local thing definitions")
                    << StringArray_At(ar, i);
        }
    }

    StringArray_Delete(ar);
}

void Cl_ReadServerMobjStateIDs()
{
    LOG_AS("Cl_ReadServerMobjStateIDs");

    StringArray *ar = StringArray_New();
    StringArray_Read(ar, msgReader);

    LOGDEV_NET_VERBOSE("Received %i mobj state IDs") << StringArray_Size(ar);

    xlatMobjState.resize(StringArray_Size(ar));

    // Translate the type IDs to local.
    for(int i = 0; i < StringArray_Size(ar); ++i)
    {
        xlatMobjState[i] = Def_GetStateNum(StringArray_At(ar, i));
        if(xlatMobjState[i] < 0)
        {
            LOG_NET_WARNING("Could not find '%s' in local state definitions")
                    << StringArray_At(ar, i);
        }
    }

    StringArray_Delete(ar);
}

Material *Cl_LocalMaterial(materialarchive_serialid_t archId)
{    
    if(!serverMaterials)
    {
        // Can't do it.
        LOGDEV_NET_WARNING("Cannot translate serial id %i, server has not sent its materials!") << archId;
        return 0;
    }
    return (Material *) MaterialArchive_Find(serverMaterials, archId, 0);
}

int Cl_LocalMobjType(int serverMobjType)
{
    if(serverMobjType < 0 || serverMobjType >= xlatMobjType.size())
        return 0; // Invalid type.
    return xlatMobjType[serverMobjType];
}

int Cl_LocalMobjState(int serverMobjState)
{
    if(serverMobjState < 0 || serverMobjState >= xlatMobjState.size())
        return 0; // Invalid state.
    return xlatMobjState[serverMobjState];
}

void Cl_ReadSectorDelta(int /*deltaType*/)
{
    /// @todo Do not assume the CURRENT map.
    Map &map = App_WorldSystem().map();

#define PLN_FLOOR   0
#define PLN_CEILING 1

    float height[2] = { 0, 0 };
    float target[2] = { 0, 0 };
    float speed[2]  = { 0, 0 };

    // Sector index number.
    Sector *sec = map.sectorPtr(Reader_ReadUInt16(msgReader));
    DENG2_ASSERT(sec);

    // Flags.
    int df = Reader_ReadPackedUInt32(msgReader);

    if(df & SDF_FLOOR_MATERIAL)
    {
        P_SetPtrp(sec, DMU_FLOOR_OF_SECTOR | DMU_MATERIAL,
                  Cl_LocalMaterial(Reader_ReadPackedUInt16(msgReader)));
    }
    if(df & SDF_CEILING_MATERIAL)
    {
        P_SetPtrp(sec, DMU_CEILING_OF_SECTOR | DMU_MATERIAL,
                  Cl_LocalMaterial(Reader_ReadPackedUInt16(msgReader)));
    }

    if(df & SDF_LIGHT)
        P_SetFloatp(sec, DMU_LIGHT_LEVEL, Reader_ReadByte(msgReader) / 255.0f);

    if(df & SDF_FLOOR_HEIGHT)
        height[PLN_FLOOR] = FIX2FLT(Reader_ReadInt16(msgReader) << 16);
    if(df & SDF_CEILING_HEIGHT)
        height[PLN_CEILING] = FIX2FLT(Reader_ReadInt16(msgReader) << 16);
    if(df & SDF_FLOOR_TARGET)
        target[PLN_FLOOR] = FIX2FLT(Reader_ReadInt16(msgReader) << 16);
    if(df & SDF_FLOOR_SPEED)
        speed[PLN_FLOOR] = FIX2FLT(Reader_ReadByte(msgReader) << (df & SDF_FLOOR_SPEED_44 ? 12 : 15));
    if(df & SDF_CEILING_TARGET)
        target[PLN_CEILING] = FIX2FLT(Reader_ReadInt16(msgReader) << 16);
    if(df & SDF_CEILING_SPEED)
        speed[PLN_CEILING] = FIX2FLT(Reader_ReadByte(msgReader) << (df & SDF_CEILING_SPEED_44 ? 12 : 15));

    if(df & (SDF_COLOR_RED | SDF_COLOR_GREEN | SDF_COLOR_BLUE))
    {
        Vector3f newColor = sec->lightColor();
        if(df & SDF_COLOR_RED)
            newColor.x = Reader_ReadByte(msgReader) / 255.f;
        if(df & SDF_COLOR_GREEN)
            newColor.y = Reader_ReadByte(msgReader) / 255.f;
        if(df & SDF_COLOR_BLUE)
            newColor.z = Reader_ReadByte(msgReader) / 255.f;
        sec->setLightColor(newColor);
    }

    if(df & (SDF_FLOOR_COLOR_RED | SDF_FLOOR_COLOR_GREEN | SDF_FLOOR_COLOR_BLUE))
    {
        Vector3f newColor = sec->floorSurface().tintColor();
        if(df & SDF_FLOOR_COLOR_RED)
            newColor.x = Reader_ReadByte(msgReader) / 255.f;
        if(df & SDF_FLOOR_COLOR_GREEN)
            newColor.y = Reader_ReadByte(msgReader) / 255.f;
        if(df & SDF_FLOOR_COLOR_BLUE)
            newColor.z = Reader_ReadByte(msgReader) / 255.f;
        sec->floorSurface().setTintColor(newColor);
    }

    if(df & (SDF_CEIL_COLOR_RED | SDF_CEIL_COLOR_GREEN | SDF_CEIL_COLOR_BLUE))
    {
        Vector3f newColor = sec->ceilingSurface().tintColor();
        if(df & SDF_CEIL_COLOR_RED)
            newColor.x = Reader_ReadByte(msgReader) / 255.f;
        if(df & SDF_CEIL_COLOR_GREEN)
            newColor.y = Reader_ReadByte(msgReader) / 255.f;
        if(df & SDF_CEIL_COLOR_BLUE)
            newColor.z = Reader_ReadByte(msgReader) / 255.f;
        sec->ceilingSurface().setTintColor(newColor);
    }

    // The whole delta has now been read.

    // Do we need to start any moving planes?
    if(df & SDF_FLOOR_HEIGHT)
    {
        ClPlaneMover::newThinker(sec->floor(), height[PLN_FLOOR], 0);
    }
    else if(df & (SDF_FLOOR_TARGET | SDF_FLOOR_SPEED))
    {
        ClPlaneMover::newThinker(sec->floor(), target[PLN_FLOOR], speed[PLN_FLOOR]);
    }

    if(df & SDF_CEILING_HEIGHT)
    {
        ClPlaneMover::newThinker(sec->ceiling(), height[PLN_CEILING], 0);
    }
    else if(df & (SDF_CEILING_TARGET | SDF_CEILING_SPEED))
    {
        ClPlaneMover::newThinker(sec->ceiling(), target[PLN_CEILING], speed[PLN_CEILING]);
    }

#undef PLN_CEILING
#undef PLN_FLOOR
}

void Cl_ReadSideDelta(int /*deltaType*/)
{
    /// @todo Do not assume the CURRENT map.
    Map &map = App_WorldSystem().map();

    int const index = Reader_ReadUInt16(msgReader);
    int const df    = Reader_ReadPackedUInt32(msgReader); // Flags.

    LineSide *side = map.sidePtr(index);
    DENG2_ASSERT(side != 0);

    if(df & SIDF_TOP_MATERIAL)
    {
        int matIndex = Reader_ReadPackedUInt16(msgReader);
        side->top().setMaterial(Cl_LocalMaterial(matIndex));
    }

    if(df & SIDF_MID_MATERIAL)
    {
        int matIndex = Reader_ReadPackedUInt16(msgReader);
        side->middle().setMaterial(Cl_LocalMaterial(matIndex));
    }

    if(df & SIDF_BOTTOM_MATERIAL)
    {
        int matIndex = Reader_ReadPackedUInt16(msgReader);
        side->bottom().setMaterial(Cl_LocalMaterial(matIndex));
    }

    if(df & SIDF_LINE_FLAGS)
    {
        // The delta includes the entire lowest byte.
        int lineFlags = Reader_ReadByte(msgReader);
        Line &line = side->line();
        line.setFlags((line.flags() & ~0xff) | lineFlags, de::ReplaceFlags);
    }

    if(df & (SIDF_TOP_COLOR_RED | SIDF_TOP_COLOR_GREEN | SIDF_TOP_COLOR_BLUE))
    {
        Vector3f newColor = side->top().tintColor();
        if(df & SIDF_TOP_COLOR_RED)
            newColor.x = Reader_ReadByte(msgReader) / 255.f;
        if(df & SIDF_TOP_COLOR_GREEN)
            newColor.y = Reader_ReadByte(msgReader) / 255.f;
        if(df & SIDF_TOP_COLOR_BLUE)
            newColor.z = Reader_ReadByte(msgReader) / 255.f;
        side->top().setTintColor(newColor);
    }

    if(df & (SIDF_MID_COLOR_RED | SIDF_MID_COLOR_GREEN | SIDF_MID_COLOR_BLUE))
    {
        Vector3f newColor = side->middle().tintColor();
        if(df & SIDF_MID_COLOR_RED)
            newColor.x = Reader_ReadByte(msgReader) / 255.f;
        if(df & SIDF_MID_COLOR_GREEN)
            newColor.y = Reader_ReadByte(msgReader) / 255.f;
        if(df & SIDF_MID_COLOR_BLUE)
            newColor.z = Reader_ReadByte(msgReader) / 255.f;
        side->middle().setTintColor(newColor);
    }
    if(df & SIDF_MID_COLOR_ALPHA)
    {
        side->middle().setOpacity(Reader_ReadByte(msgReader) / 255.f);
    }

    if(df & (SIDF_BOTTOM_COLOR_RED | SIDF_BOTTOM_COLOR_GREEN | SIDF_BOTTOM_COLOR_BLUE))
    {
        Vector3f newColor = side->bottom().tintColor();
        if(df & SIDF_BOTTOM_COLOR_RED)
            newColor.x = Reader_ReadByte(msgReader) / 255.f;
        if(df & SIDF_BOTTOM_COLOR_GREEN)
            newColor.y = Reader_ReadByte(msgReader) / 255.f;
        if(df & SIDF_BOTTOM_COLOR_BLUE)
            newColor.z = Reader_ReadByte(msgReader) / 255.f;
        side->bottom().setTintColor(newColor);
    }

    if(df & SIDF_MID_BLENDMODE)
    {
        side->middle().setBlendMode(blendmode_t(Reader_ReadInt32(msgReader)));
    }

    if(df & SIDF_FLAGS)
    {
        // The delta includes the entire lowest byte.
        int sideFlags = Reader_ReadByte(msgReader);
        side->setFlags((side->flags() & ~0xff) | sideFlags, de::ReplaceFlags);
    }
}

void Cl_ReadPolyDelta()
{
    /// @todo Do not assume the CURRENT map.
    Map &map     = App_WorldSystem().map();
    Polyobj &pob = map.polyobj(Reader_ReadPackedUInt16(msgReader));

    int const df = Reader_ReadByte(msgReader); // Flags.
    if(df & PODF_DEST_X)
    {
        pob.dest[VX] = Reader_ReadFloat(msgReader);
    }

    if(df & PODF_DEST_Y)
    {
        pob.dest[VY] = Reader_ReadFloat(msgReader);
    }

    if(df & PODF_SPEED)
    {
        pob.speed = Reader_ReadFloat(msgReader);
    }

    if(df & PODF_DEST_ANGLE)
    {
        pob.destAngle = ((angle_t)Reader_ReadInt16(msgReader)) << 16;
    }

    if(df & PODF_ANGSPEED)
    {
        pob.angleSpeed = ((angle_t)Reader_ReadInt16(msgReader)) << 16;
    }

    if(df & PODF_PERPETUAL_ROTATE)
    {
        pob.destAngle = -1;
    }

    // Update/create the polymover thinker.
    ClPolyMover::newThinker(pob,
            /* move: */   CPP_BOOL(df & (PODF_DEST_X | PODF_DEST_Y | PODF_SPEED)),
            /* rotate: */ CPP_BOOL(df & (PODF_DEST_ANGLE | PODF_ANGSPEED | PODF_PERPETUAL_ROTATE)));
}
