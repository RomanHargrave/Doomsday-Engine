/** @file p_inter.h
 *
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBHERETIC_P_INTER_H
#define LIBHERETIC_P_INTER_H

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "doomdef.h"
#include "player.h"

DENG_EXTERN_C int maxAmmo[NUM_AMMO_TYPES];
DENG_EXTERN_C int clipAmmo[NUM_AMMO_TYPES];

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @return  @c true if power accepted.
 */
dd_bool P_GivePower(player_t *player, powertype_t powerType);

/**
 * @param player     Player to relieve of the power.
 * @param powerType  Power type to take.
 *
 * @return  @c true iff the power was taken.
 */
dd_bool P_TakePower(player_t *player, powertype_t powerType);

/**
 * @param player     Player to toggle a power for.
 * @param powerType  Power type to toggle.
 *
 * @return  @c true iff the power was toggled.
 */
dd_bool P_TogglePower(player_t *player, powertype_t powerType);

/**
 * Give key(s) to the specified player. If a key is successfully given a short
 * "bonus flash" screen tint animation is played and a HUE_ON_PICKUP_KEY event
 * is generated (which optionally reveals the HUD if hidden). If the specified
 * key(s) are already owned then nothing will happen (and false is returned).
 *
 * @param player   Player to receive the key(s).
 * @param keyType  Key type to give. Use @c NUM_KEY_TYPES to give ALL keys.
 *
 * @return  @c true iff at least one new key was given (not already owned).
 */
dd_bool P_GiveKey(player_t *player, keytype_t keyType);

/**
 * Give ammo(s) to the specified player. If a ammo is successfully given the
 * player 'brain' may decide to change weapon (depends on the user's config)
 * and a HUE_ON_PICKUP_AMMO event is generated (which optionally reveals the
 * HUD if hidden). If the specified ammo(s) are already owned then nothing will
 * happen (and false is returned).
 *
 * @note The final number of rounds the player will receive depends on both the
 * ammount given and how many the player can carry. Use @ref P_GiveBackpack()
 * to equip the player with a backpack, thereby increasing this capacity.
 *
 * @param player     Player to receive the ammo(s).
 * @param ammoType   Ammo type to give. Use @c NUM_AMMO_TYPES to give ALL ammos.
 *                   Giving the special 'unlimited ammo' type @c AT_NOAMMO will
 *                   always succeed, however no sideeffects will occur.
 * @param numRounds  Number of rounds to give. Use @c -1 to give as many rounds
 *                   as necessary to fully replenish stock.
 *
 * @return  @c true iff at least one new round was given (not already owned).
 */
dd_bool P_GiveAmmo(player_t *player, ammotype_t ammoType, int numRounds);

/**
 * @param player    Player to receive the health.
 * @param amount    Amount of health to give. Use @c -1 to fully replenish.
 *
 * @return  @c true iff at least some of the health was given.
 */
dd_bool P_GiveHealth(player_t *player, int amount);

/**
 * @param player    Player to receive the backpack.
 */
void P_GiveBackpack(player_t *player);

/**
 * @return  @c true if the weapon or its ammo was accepted.
 */
dd_bool P_GiveWeapon(player_t *player, weapontype_t weaponType);

/**
 * @return  @c true iff the armor was given.
 */
dd_bool P_GiveArmor(player_t *player, int armorType, int armorPoints);

void P_TouchSpecialMobj(mobj_t *special, mobj_t *toucher);

/**
 * @return  @c true, if the player is morphed.
 */
dd_bool P_MorphPlayer(player_t *player);

/**
 * Damages both enemies and players.
 *
 * @param inflictor     Mobj that caused the damage creature or missile,
 *                      can be NULL (slime, etc)
 * @param source        Mobj to target after taking damage. Can be @c NULL
 *                      for barrel explosions and other environmental stuff.
 *                      Source and inflictor are the same for melee attacks.
 *
 * @return  Actual amount of damage done.
 */
int P_DamageMobj2(mobj_t *target, mobj_t *inflictor, mobj_t *source, int damageP, dd_bool stomping, dd_bool skipNetworkCheck);
int P_DamageMobj(mobj_t *target, mobj_t *inflictor, mobj_t *source, int damageP, dd_bool stomping/*, dd_bool skipNetworkCheck = false*/);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBHERETIC_P_INTER_H
