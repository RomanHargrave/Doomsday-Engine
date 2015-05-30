/** @file s_environ.h Audio environment management.
 * @ingroup audio
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_SOUND_ENVIRON
#define DENG_SOUND_ENVIRON

#include <doomsday/uri.h>

enum AudioEnvironmentId
{
    AE_NONE = -1,
    AE_FIRST = 0,
    AE_METAL = AE_FIRST,
    AE_ROCK,
    AE_WOOD,
    AE_CLOTH,
    NUM_AUDIO_ENVIRONMENTS
};

/**
 * Defines the properties of an audio environment.
 */
struct AudioEnvironment
{
    char const name[9]; ///< Environment type name.
    int volumeMul;
    int decayMul;
    int dampingMul;
};

/**
 * Lookup the symbolic name of the identified audio environment.
 */
char const *S_AudioEnvironmentName(AudioEnvironmentId id);

/**
 * Lookup the identified audio environment.
 */
AudioEnvironment const &S_AudioEnvironment(AudioEnvironmentId id);

/**
 * Lookup the audio environment associated with material @a uri. If no environment
 * is defined then @c AE_NONE is returned.
 */
AudioEnvironmentId S_AudioEnvironmentId(de::Uri const *uri);

#endif // DENG_SOUND_ENVIRON
