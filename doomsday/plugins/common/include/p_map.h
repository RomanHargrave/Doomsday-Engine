/** @file p_map.h Common play/maputil functions.
 *
 * @authors Copyright © 1999-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1993-1996 by id Software, Inc.
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

#ifndef LIBCOMMON_P_MAP_H
#define LIBCOMMON_P_MAP_H

#include "common.h"

DENG_EXTERN_C coord_t attackRange;

// If "floatOk" true, move would be ok if within "tmFloorZ - tmCeilingZ".
DENG_EXTERN_C boolean floatOk;
DENG_EXTERN_C coord_t tmFloorZ;
DENG_EXTERN_C coord_t tmCeilingZ;
DENG_EXTERN_C Material *tmFloorMaterial;

DENG_EXTERN_C Line *ceilingLine, *floorLine;
DENG_EXTERN_C Line *blockLine;
DENG_EXTERN_C mobj_t *lineTarget; // Who got hit (or NULL).
DENG_EXTERN_C mobj_t *tmThing;

#if __JHEXEN__
DENG_EXTERN_C mobj_t *puffSpawned;
DENG_EXTERN_C mobj_t *blockingMobj;
#endif

DENG_EXTERN_C AABoxd tmBox;
DENG_EXTERN_C boolean fellDown;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Look from eyes of t1 to any part of t2 (start from middle of t1).
 *
 * @param from  The mobj doing the looking.
 * @param to    The mobj being looked at.
 *
 * @return  @c true iff a straight line between t1 and t2 is unobstructed.
 */
boolean P_CheckSight(mobj_t const *from, mobj_t const *to);

/**
 * This is purely informative, nothing is modified (except things picked up).
 *
 * in:
 *  a mobj_t (can be valid or invalid)
 *  a position to be checked
 *   (doesn't need to be related to the mobj_t->x,y)
 *
 * during:
 *  special things are touched if MF_PICKUP early out on solid lines?
 *
 * out:
 *  newsubsec
 *  floorz
 *  ceilingz
 *  tmDropoffZ
 *   the lowest point contacted
 *   (monsters won't move to a drop off)
 *  speciallines[]
 *  numspeciallines
 */
boolean P_CheckPositionXYZ(mobj_t *thing, coord_t x, coord_t y, coord_t z);
boolean P_CheckPosition(mobj_t *thing, coord_t const pos[3]);

boolean P_CheckPositionXY(mobj_t *thing, coord_t x, coord_t y);

/**
 * Source is the creature that caused the explosion at spot.
 */
#if __JHEXEN__
void P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage, int distance, boolean canDamageSource);
#else
void P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage, int distance);
#endif

/**
 * Attempts to move a mobj to a new 3D position, crossing special lines
 * and picking up things.
 *
 * @note  This function is exported from the game plugin.
 *
 * @param mobj  Mobj to move.
 * @param x     New X coordinate.
 * @param y     New Y coordinate.
 * @param z     New Z coordinate.
 *
 * @return  @c true iff the move was successful.
 */
boolean P_TryMoveXYZ(mobj_t *mobj, coord_t x, coord_t y, coord_t z);

#if !__JHEXEN__
boolean P_TryMoveXY(mobj_t *thing, coord_t x, coord_t y, boolean dropoff, boolean slide);
#else
boolean P_TryMoveXY(mobj_t *thing, coord_t x, coord_t y);
#endif

boolean P_TeleportMove(mobj_t *thing, coord_t x, coord_t y, boolean alwaysStomp);

void P_TelefragMobjsTouchingPlayers(void);

void P_SlideMove(mobj_t *mo);

/**
 * Looks for special lines in front of the player to activate.
 *
 * @param player  The player to test.
 */
void P_UseLines(player_t *player);

/**
 * @param sector  The sector to check.
 * @param crunch  @c true= crush any things in the sector.
 */
boolean P_ChangeSector(Sector *sector, boolean crunch);

/**
 * This is called by the engine when it needs to change sector heights without
 * consulting game logic first. Most commonly this occurs on clientside, where
 * the client needs to apply plane height changes as per the deltas.
 *
 * @param sectorIdx  Index of the sector to update.
 */
void P_HandleSectorHeightChange(int sectorIdx);

float P_AimLineAttack(mobj_t *t1, angle_t angle, coord_t distance);
void P_LineAttack(mobj_t *t1, angle_t angle, coord_t distance, coord_t slope, int damage);

coord_t P_GetGravity(void);

boolean P_CheckSides(mobj_t *actor, coord_t x, coord_t y);

#if __JHERETIC__ || __JHEXEN__
/**
 * @param mo  The mobj whoose position to test.
 * @return boolean  @c true iff the mobj is not blocked by anything.
 */
boolean P_TestMobjLocation(mobj_t *mobj);
#endif

#if __JHEXEN__
void P_BounceWall(mobj_t *mobj);

mobj_t *P_CheckOnMobj(mobj_t *mobj);

void PIT_ThrustSpike(mobj_t *actor);

/**
 * See if the specified player can use the specified puzzle item on a
 * thing or line(s) at their current world location.
 *
 * @param player    The player using the puzzle item.
 * @param itemType  The type of item to try to use.
 * @return boolean  true if the puzzle item was used.
 */
boolean P_UsePuzzleItem(player_t *player, int itemType);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_P_MAP_H
