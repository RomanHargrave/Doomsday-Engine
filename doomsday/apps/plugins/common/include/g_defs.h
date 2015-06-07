/** @file g_defs.h Game definition lookup utilities.
 *
 * @authors Copyright © 1999-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCOMMON_DEFINITION_UTILS_H
#define LIBCOMMON_DEFINITION_UTILS_H

#include <include/info.h>

#ifdef __cplusplus
#include <doomsday/defs/ded.h>
#include <doomsday/uri.h>


/**
 * Provides access to the engine's definition database (DED).
 */
ded_t &Defs();

/**
 * Returns the total number of 'playable' episodes. A playable episode is one whos
 * starting map is defined, and for which map data exists.
 */
int PlayableEpisodeCount();

/**
 * Returns the unique identifier of the first playable episode. If no playable episodes
 * are defined a zero-length string is returned.
 */
de::String FirstPlayableEpisodeId();

/**
 * Translates a map warp number for the @em current episode to a unique map identifier.
 *
 * @note This should only be used where necessary for compatibility reasons as the
 * "warp translation" mechanic is redundant in the context of Doomsday's altogether
 * better handling of map resources and their references. Instead, use the map URI
 * mechanism.
 *
 * @param episode     Episode identifier.
 * @param warpNumber  Warp number to translate.
 *
 * @return The unique identifier of the map. If no game session is in progress or the
 * warp number is not found, the URI "Maps:" is returned.
 */
de::Uri TranslateMapWarpNumber(de::String const &episodeId, int warpNumber);

extern "C" {
#endif

/**
 * @return  The default for a value (retrieved from Doomsday).
 */
int GetDefInt(char const *def, int* returnVal);

void GetDefState(char const *def, statenum_t* returnVal);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_DEFINITION_UTILS_H
