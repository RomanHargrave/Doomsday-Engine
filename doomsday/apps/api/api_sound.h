/** @file api_sound.h Public API for the audio subsystem.
 * @ingroup audio
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DOOMSDAY_API_SOUND_H
#define DOOMSDAY_API_SOUND_H

#include "apis.h"
#include <de/types.h>

struct mobj_s;

DENG_API_TYPEDEF(S)
{
    de_api_t api;

    /**
     * Must be called before the map is changed.
     */
    void (*MapChange)(void);

    /**
     * Play a sound on the local system. A public interface.
     *
     * If @a origin and @a point are both @c NULL, the sound is played in 2D and
     * centered.
     *
     * @param soundIdAndFlags ID of the sound to play. Flags can be included (DDSF_*).
     * @param origin        Mobj where the sound originates. May be @c NULL.
     * @param point         World coordinates where the sound originate. May be @c NULL.
     * @param volume        Volume for the sound (0...1).
     *
     * @return  Non-zero if a sound was started.
     */
    int (*LocalSoundAtVolumeFrom)(int sound_id, struct mobj_s *origin, coord_t *pos, float volume);

    /**
     * Plays a sound on the local system at the given volume.
     * This is a public sound interface.
     *
     * @return  Non-zero if a sound was started.
     */
    int (*LocalSoundAtVolume)(int soundID, struct mobj_s *origin, float volume);

    /**
     * Plays a sound on the local system from the given origin.
     * This is a public sound interface.
     *
     * @return  Non-zero if a sound was started.
     */
    int (*LocalSound)(int soundID, struct mobj_s *origin);

    /**
     * Plays a sound on the local system at a give distance from listener.
     * This is a public sound interface.
     *
     * @return  Non-zero if a sound was started.
     */
    int (*LocalSoundFrom)(int soundID, coord_t *fixedpos);

    /**
     * Play a world sound. All players in the game will hear it.
     *
     * @return  Non-zero if a sound was started.
     */
    int (*StartSound)(int soundId, struct mobj_s *origin);

    /**
     * Play a world sound. The sound is sent to all players except the one who
     * owns the origin mobj. The server assumes that the owner of the origin plays
     * the sound locally, which is done here, in the end of S_StartSoundEx().
     *
     * @param soundID       Id of the sound.
     * @param origin        Origin mobj for the sound.
     *
     * @return  Non-zero if a sound was successfully started.
     */
    int (*StartSoundEx)(int soundId, struct mobj_s *origin);

    /**
     * Play a world sound. All players in the game will hear it.
     *
     * @return  Non-zero if a sound was started.
     */
    int (*StartSoundAtVolume)(int soundID, struct mobj_s *origin, float volume);

    /**
     * Play a player sound. Only the specified player will hear it.
     *
     * @return  Non-zero if a sound was started (always).
     */
    int (*ConsoleSound)(int soundID, struct mobj_s *origin, int targetConsole);

    /**
     * Stop playing sound(s), either by their unique identifier or by their
     * emitter(s).
     *
     * @param soundId       @c 0: stops all sounds emitted from the targeted origin(s).
     * @param origin        @c NULL: stops all sounds with the ID.
     *                      Otherwise both ID and origin must match.
     */
    void (*StopSound)(int soundID, struct mobj_s *origin/*, flags = 0*/);

    /**
     * @copydoc StopSound()
     * @param flags         @ref soundStopFlags
     */
    void (*StopSound2)(int soundID, struct mobj_s *origin, int flags);

    /**
     * Is an instance of the sound being played using the given emitter?
     * If sound_id is zero, returns true if the source is emitting any sounds.
     * An exported function.
     *
     * @return  Non-zero if a sound is playing.
     */
    int (*IsPlaying)(int soundID, struct mobj_s *origin);

    /**
     * @return  @c NULL, if the song is found.
     */
    int (*StartMusic)(char const *musicID, dd_bool looped);

    /**
     * Start a song based on its number.
     *
     * @return  @c NULL, if the ID exists.
     */
    int (*StartMusicNum)(int id, dd_bool looped);

    /**
     * Stops playing a song.
     */
    void (*StopMusic)(void);

    /**
     * Change paused state of the current music.
     */
    void (*PauseMusic)(dd_bool doPause);
}
DENG_API_T(S);

#ifndef DENG_NO_API_MACROS_SOUND
#define S_MapChange                 _api_S.MapChange
#define S_LocalSoundAtVolumeFrom    _api_S.LocalSoundAtVolumeFrom
#define S_LocalSoundAtVolume        _api_S.LocalSoundAtVolume
#define S_LocalSound                _api_S.LocalSound
#define S_LocalSoundFrom            _api_S.LocalSoundFrom
#define S_StartSound                _api_S.StartSound
#define S_StartSoundEx              _api_S.StartSoundEx
#define S_StartSoundAtVolume        _api_S.StartSoundAtVolume
#define S_ConsoleSound              _api_S.ConsoleSound
#define S_StopSound2                _api_S.StopSound2
#define S_StopSound                 _api_S.StopSound
#define S_IsPlaying                 _api_S.IsPlaying
#define S_StartMusic                _api_S.StartMusic
#define S_StartMusicNum             _api_S.StartMusicNum
#define S_StopMusic                 _api_S.StopMusic
#define S_PauseMusic                _api_S.PauseMusic
#endif

#ifdef __DOOMSDAY__
DENG_USING_API(S);
#endif

#endif // DOOMSDAY_API_SOUND_H
