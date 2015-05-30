/** @file s_sfx.cpp  Sound Effects.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_play.h"
#include "de_defs.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_misc.h"
#include "de_render.h"

#include "Sector"
#include "SectorCluster"

#include "audio/sys_audio.h"
#include "api_fontrender.h"

#include "audio/s_sfx.h"

#ifdef __CLIENT__
#  include "ui/ui_main.h"
#endif

#include <de/Log>

#define SFX_MAX_CHANNELS        (256)
#define SFX_LOWEST_PRIORITY     (-1000)

void Sfx_3DMode(dd_bool activate);
void Sfx_SampleFormat(int newBits, int newRate);

dd_bool sfxAvail = false;

int sfxMaxChannels = 16;
int sfxDedicated2D = 4;
float sfxReverbStrength = .5f;

// Console variables:
int sfx3D;
int sfx16Bit;
int sfxSampleRate = 11025;

static int numChannels;
static sfxchannel_t *channels;
static mobj_t *listener;
static SectorCluster *listenerCluster;

static thread_t refreshHandle;
static volatile dd_bool allowRefresh, refreshing;

static byte refMonitor;

void Sfx_UpdateReverb()
{
    listenerCluster = nullptr;
}

#ifdef __CLIENT__

/**
 * This is a high-priority thread that periodically checks if the channels
 * need to be updated with more data. The thread terminates when it notices
 * that the channels have been destroyed. The Sfx audio driver maintains a 250ms
 * buffer for each channel, which means the refresh must be done often
 * enough to keep them filled.
 *
 * @todo Use a real mutex, will you?
 */
int C_DECL Sfx_ChannelRefreshThread(void *parm)
{
    DENG_UNUSED(parm);

    // We'll continue looping until the Sfx module is shut down.
    while(sfxAvail && channels)
    {
        // The bit is swapped on each refresh (debug info).
        refMonitor ^= 1;

        if(allowRefresh)
        {
            // Do the refresh.
            refreshing = true;
            sfxchannel_t *ch = channels;
            for(int i = 0; i < numChannels; ++i, ch++)
            {
                if(!ch->buffer || !(ch->buffer->flags & SFXBF_PLAYING))
                    continue;

                AudioDriver_SFX()->Refresh(ch->buffer);
            }

            refreshing = false;
            // Let's take a nap.
            Sys_Sleep(200);
        }
        else
        {
            // Refreshing is not allowed, so take a shorter nap while
            // waiting for allowRefresh.
            Sys_Sleep(150);
        }
    }

    // Time to end this thread.
    return 0;
}

#endif // __CLIENT__

void Sfx_AllowRefresh(dd_bool allow)
{
    if(!sfxAvail) return;
    if(allowRefresh == allow) return; // No change.

    allowRefresh = allow;

    // If we're denying refresh, let's make sure that if it's currently
    // running, we don't continue until it has stopped.
    if(!allow)
    {
        while(refreshing)
        {
            Sys_Sleep(0);
        }
    }

    // Sys_SuspendThread(refreshHandle, !allow);
}

void Sfx_StopSoundGroup(int group, mobj_t *emitter)
{
    if(!sfxAvail)
        return;

    sfxchannel_t *ch = channels;
    for(int i = 0; i < numChannels; ++i, ch++)
    {
        if(!ch->buffer || !(ch->buffer->flags & SFXBF_PLAYING))
            continue;
        if(ch->buffer->sample->group != group || (emitter && ch->emitter != emitter))
            continue;

        // This channel must stop.
        AudioDriver_SFX()->Stop(ch->buffer);
    }
}

int Sfx_StopSound(int id, mobj_t *emitter)
{
    return Sfx_StopSoundWithLowerPriority(id, emitter, -1);
}

int Sfx_StopSoundWithLowerPriority(int id, mobj_t *emitter, int defPriority)
{
    if(!sfxAvail)
        return false;

    int stopCount = 0;
    sfxchannel_t *ch = channels;
    for(int i = 0; i < numChannels; ++i, ch++)
    {
        if(!ch->buffer || !(ch->buffer->flags & SFXBF_PLAYING))
            continue;
        if((id && ch->buffer->sample->id != id) || (emitter && ch->emitter != emitter))
            continue;

        // Can it be stopped?
        if(ch->buffer->flags & SFXBF_DONT_STOP)
        {
            // The emitter might get destroyed...
            ch->emitter = NULL;
            ch->flags |= SFXCF_NO_UPDATE | SFXCF_NO_ORIGIN;
            continue;
        }

        // Check the priority.
        if(defPriority >= 0)
        {
            int oldPrio = defs.sounds[ch->buffer->sample->id].priority;
            if(oldPrio < defPriority) // Old is more important.
                return -1;
        }

        // This channel must be stopped!
        AudioDriver_SFX()->Stop(ch->buffer);
        ++stopCount;
    }

    return stopCount;
}

#if 0 // Currently unused.
int Sfx_IsPlaying(int id, mobj_t* emitter)
{
    int                 i;
    sfxchannel_t*       ch;

    if(!sfxAvail)
        return false;

    for(i = 0, ch = channels; i < numChannels; ++i, ch++)
    {
        if(!ch->buffer || !(ch->buffer->flags & SFXBF_PLAYING) ||
           ch->emitter != emitter || id && ch->buffer->sample->id != id)
            continue;

        // Once playing, repeating sounds don't stop.
        if(ch->buffer->flags & SFXBF_REPEAT) return true;

        // Check time. The flag is updated after a slight delay
        // (only at refresh).
        if(Sys_GetTime() - ch->starttime <
           ch->buffer->sample->numsamples / (float) ch->buffer->freq *
            TICSPERSEC) return true;
    }

    return false;
}
#endif

void Sfx_UnloadSoundID(int id)
{
    if(!sfxAvail) return;

    BEGIN_COP;

    sfxchannel_t *ch = channels;
    for(int i = 0; i < numChannels; ++i, ch++)
    {
        if(!ch->buffer || !ch->buffer->sample || ch->buffer->sample->id != id)
            continue;

        // Stop and unload.
        AudioDriver_SFX()->Reset(ch->buffer);
    }

    END_COP;
}

int Sfx_CountPlaying(int id)
{
    if(!sfxAvail)
        return 0;

    int count = 0;
    sfxchannel_t *ch = channels;
    for(int i = 0; i < numChannels; i++, ch++)
    {
        if(!ch->buffer || !ch->buffer->sample)
            continue;
        if(ch->buffer->sample->id != id || !(ch->buffer->flags & SFXBF_PLAYING))
            continue;

        count++;
    }

    return count;
}

/**
 * The priority of a sound is affected by distance, volume and age.
 */
float Sfx_Priority(mobj_t *emitter, coord_t *point, float volume, int startTic)
{
    // In five seconds all priority of a sound is gone.
    float timeoff = 1000 * (Timer_Ticks() - startTic) / (5.0f * TICSPERSEC);
    coord_t *origin;

    if(!listener || (!emitter && !point))
    {
        // The sound does not have an origin.
        return 1000 * volume - timeoff;
    }

    // The sound has an origin, base the points on distance.
    if(emitter)
    {
        origin = emitter->origin;
    }
    else
    {
        // No emitter mobj, use the fixed source position.
        origin = point;
    }

    return 1000 * volume - Mobj_ApproxPointDistance(listener, origin) / 2 - timeoff;
}

/**
 * Calculate priority points for a sound playing on a channel.
 * They are used to determine which sounds can be cancelled by new sounds.
 * Zero is the lowest priority.
 */
float Sfx_ChannelPriority(sfxchannel_t* ch)
{
    if(!ch->buffer || !(ch->buffer->flags & SFXBF_PLAYING))
        return SFX_LOWEST_PRIORITY;

    if(ch->flags & SFXCF_NO_ORIGIN)
        return Sfx_Priority(0, 0, ch->volume, ch->startTime);

    // ch->pos is set to emitter->xyz during updates.
    return Sfx_Priority(0, ch->origin, ch->volume, ch->startTime);
}

/**
 * @return  The actual 3D coordinates of the listener.
 */
void Sfx_GetListenerXYZ(float* origin)
{
    if(!listener) return;

    /// @todo Make it exactly eye-level! (viewheight).
    origin[VX] = listener->origin[VX];
    origin[VY] = listener->origin[VY];
    origin[VZ] = listener->origin[VZ] + listener->height - 5;
}

/**
 * Updates the channel buffer's properties based on 2D/3D position
 * calculations. Listener might be NULL. Sounds emitted from the listener
 * object are considered to be inside the listener's head.
 */
void Sfx_ChannelUpdate(sfxchannel_t* ch)
{
    sfxbuffer_t* buf = ch->buffer;
    float normdist, dist, pan, angle, vec[3];

    if(!buf || (ch->flags & SFXCF_NO_UPDATE))
        return;

    // Copy the emitter's position (if any), to the pos coord array.
    if(ch->emitter)
    {
        ch->origin[VX] = ch->emitter->origin[VX];
        ch->origin[VY] = ch->emitter->origin[VY];
        ch->origin[VZ] = ch->emitter->origin[VZ];

        // If this is a mobj, center the Z pos.
        if(Thinker_IsMobjFunc(ch->emitter->thinker.function))
        {
            // Sounds originate from the center.
            ch->origin[VZ] += ch->emitter->height / 2;
        }
    }

    // Frequency is common to both 2D and 3D sounds.
    AudioDriver_SFX()->Set(buf, SFXBP_FREQUENCY, ch->frequency);

    if(buf->flags & SFXBF_3D)
    {
        // Volume is affected only by maxvol.
        AudioDriver_SFX()->Set(buf, SFXBP_VOLUME, ch->volume * sfxVolume / 255.0f);
        if(ch->emitter && ch->emitter == listener)
        {
            // Emitted by the listener object. Go to relative position mode
            // and set the position to (0,0,0).
            vec[VX] = vec[VY] = vec[VZ] = 0;
            AudioDriver_SFX()->Set(buf, SFXBP_RELATIVE_MODE, true);
            AudioDriver_SFX()->Setv(buf, SFXBP_POSITION, vec);
        }
        else
        {
            // Use the channel's map space origin.
            float origin[3];
            V3f_Copyd(origin, ch->origin);
            AudioDriver_SFX()->Set(buf, SFXBP_RELATIVE_MODE, false);
            AudioDriver_SFX()->Setv(buf, SFXBP_POSITION, origin);
        }

        // If the sound is emitted by the listener, speed is zero.
        if(ch->emitter && ch->emitter != listener &&
           Thinker_IsMobjFunc(ch->emitter->thinker.function))
        {
            vec[VX] = ch->emitter->mom[MX] * TICSPERSEC;
            vec[VY] = ch->emitter->mom[MY] * TICSPERSEC;
            vec[VZ] = ch->emitter->mom[MZ] * TICSPERSEC;
            AudioDriver_SFX()->Setv(buf, SFXBP_VELOCITY, vec);
        }
        else
        {
            // Not moving.
            vec[VX] = vec[VY] = vec[VZ] = 0;
            AudioDriver_SFX()->Setv(buf, SFXBP_VELOCITY, vec);
        }
    }
    else
    {   // This is a 2D buffer.
        if((ch->flags & SFXCF_NO_ORIGIN) ||
           (ch->emitter && ch->emitter == listener))
        {
            dist = 1;
            pan = 0;
        }
        else
        {
            // Calculate roll-off attenuation. [.125/(.125+x), x=0..1]
            dist = Mobj_ApproxPointDistance(listener, ch->origin);
            if(dist < soundMinDist || (ch->flags & SFXCF_NO_ATTENUATION))
            {
                // No distance attenuation.
                dist = 1;
            }
            else if(dist > soundMaxDist)
            {
                // Can't be heard.
                dist = 0;
            }
            else
            {
                normdist = (dist - soundMinDist) / (soundMaxDist - soundMinDist);

                // Apply the linear factor so that at max distance there
                // really is silence.
                dist = .125f / (.125f + normdist) * (1 - normdist);
            }

            // And pan, too. Calculate angle from listener to emitter.
            if(listener)
            {
                angle = (M_PointToAngle2(listener->origin, ch->origin) - listener->angle) / (float) ANGLE_MAX *360;

                // We want a signed angle.
                if(angle > 180)
                    angle -= 360;

                // Front half.
                if(angle <= 90 && angle >= -90)
                {
                    pan = -angle / 90;
                }
                else
                {
                    // Back half.
                    pan = (angle + (angle > 0 ? -180 : 180)) / 90;
                    // Dampen sounds coming from behind.
                    dist *= (1 + (pan > 0 ? pan : -pan)) / 2;
                }
            }
            else
            {
                // No listener mobj? Can't pan, then.
                pan = 0;
            }
        }

        AudioDriver_SFX()->Set(buf, SFXBP_VOLUME, ch->volume * dist * sfxVolume / 255.0f);
        AudioDriver_SFX()->Set(buf, SFXBP_PAN, pan);
    }
}

void Sfx_SetListener(mobj_t *mobj)
{
    listener = mobj;
}

void Sfx_ListenerUpdate()
{
    // No volume means no sound.
    if(!sfxAvail || !sfx3D || !sfxVolume)
        return;

    // Update the listener mobj.
    Sfx_SetListener(S_GetListenerMobj());

    if(listener)
    {
        // Position. At eye-level.
        float vec[4]; Sfx_GetListenerXYZ(vec);
        AudioDriver_SFX()->Listenerv(SFXLP_POSITION, vec);

        // Orientation. (0,0) will produce front=(1,0,0) and up=(0,0,1).
        vec[VX] = listener->angle / (float) ANGLE_MAX *360;

        vec[VY] = (listener->dPlayer? LOOKDIR2DEG(listener->dPlayer->lookDir) : 0);
        AudioDriver_SFX()->Listenerv(SFXLP_ORIENTATION, vec);

        // Velocity. The unit is world distance units per second.
        vec[VX] = listener->mom[MX] * TICSPERSEC;
        vec[VY] = listener->mom[MY] * TICSPERSEC;
        vec[VZ] = listener->mom[MZ] * TICSPERSEC;
        AudioDriver_SFX()->Listenerv(SFXLP_VELOCITY, vec);

        // Reverb effects. Has the current sector cluster changed?
        SectorCluster *newCluster = Mobj_ClusterPtr(*listener);
        if(newCluster && (!listenerCluster || listenerCluster != newCluster))
        {
            listenerCluster = newCluster;

            // It may be necessary to recalculate the reverb properties...
            AudioEnvironmentFactors const &envFactors = listenerCluster->reverb();

            for(int i = 0; i < NUM_REVERB_DATA; ++i)
            {
                vec[i] = envFactors[i];
                if(i == SRD_VOLUME)
                {
                    vec[i] *= sfxReverbStrength;
                }
            }

            AudioDriver_SFX()->Listenerv(SFXLP_REVERB, vec);
        }
    }

    // Update all listener properties.
    AudioDriver_SFX()->Listener(SFXLP_UPDATE, 0);
}

void Sfx_ListenerNoReverb(void)
{
    float rev[4] = { 0, 0, 0, 0 };

    if(!sfxAvail)
        return;

    listenerCluster = NULL;
    AudioDriver_SFX()->Listenerv(SFXLP_REVERB, rev);
    AudioDriver_SFX()->Listener(SFXLP_UPDATE, 0);
}

/**
 * Stops the sound playing on the channel.
 * \note Just stopping a buffer doesn't affect refresh.
 */
void Sfx_ChannelStop(sfxchannel_t* ch)
{
    if(!ch->buffer)
        return;

    AudioDriver_SFX()->Stop(ch->buffer);
}

void Sfx_GetChannelPriorities(float* prios)
{
    int                 i;

    for(i = 0; i < numChannels; ++i)
        prios[i] = Sfx_ChannelPriority(channels + i);
}

sfxchannel_t* Sfx_ChannelFindVacant(dd_bool use3D, int bytes, int rate,
                                    int sampleID)
{
    int                 i;
    sfxchannel_t*       ch;

    for(i = 0, ch = channels; i < numChannels; ++i, ch++)
    {
        if(!ch->buffer || (ch->buffer->flags & SFXBF_PLAYING) ||
           use3D != ((ch->buffer->flags & SFXBF_3D) != 0) ||
           ch->buffer->bytes != bytes || ch->buffer->rate != rate)
            continue;

        // What about the sample?
        if(sampleID > 0)
        {
            if(!ch->buffer->sample || ch->buffer->sample->id != sampleID)
                continue;
        }
        else if(sampleID == 0)
        {
            // We're trying to find a channel with no sample already loaded.
            if(ch->buffer->sample)
                continue;
        }

        // This is perfect, take this!
        return ch;
    }

    return NULL;
}

int Sfx_StartSound(sfxsample_t *sample, float volume, float freq, mobj_t *emitter,
    coord_t *fixedOrigin, int flags)
{
    bool const play3D = sfx3D && (emitter || fixedOrigin);

    LOG_AS("Sfx_StartSound");

    if(!sfxAvail) return false;

    if(sample->id < 1 || sample->id >= defs.sounds.size()) return false;
    if(volume <= 0 || !sample->size) return false;

    if(emitter && sfxOneSoundPerEmitter)
    {
        // Stop any other sounds from the same emitter.
        if(Sfx_StopSoundWithLowerPriority(0, emitter, defs.sounds[sample->id].priority) < 0)
        {
            // Something with a higher priority is playing, can't start now.
            LOG_AUDIO_MSG("Cannot start ID %i (prio%i), overridden (emitter %i)")
                << sample->id << defs.sounds[sample->id].priority << emitter->thinker.id;

            return false;
        }
    }

    // Calculate the new sound's priority.
    int const nowTime = Timer_Ticks();
    float const myPrio = Sfx_Priority(emitter, fixedOrigin, volume, nowTime);

    bool haveChannelPrios = false;
    float channelPrios[SFX_MAX_CHANNELS];
    float lowPrio = 0;

    // Ensure there aren't already too many channels playing this sample.
    sfxinfo_t *info = &runtimeDefs.sounds[sample->id];
    if(info->channels > 0)
    {
        // The decision to stop channels is based on priorities.
        Sfx_GetChannelPriorities(channelPrios);
        haveChannelPrios = true;

        int count = Sfx_CountPlaying(sample->id);
        while(count >= info->channels)
        {
            // Stop the lowest priority sound of the playing instances, again
            // noting sounds that are more important than us.
            sfxchannel_t *ch = channels;
            sfxchannel_t *selCh = 0;
            for(int i = 0; i < numChannels; ++i, ch++)
            {
                if(!ch->buffer) continue;
                if(!(ch->buffer->flags & SFXBF_PLAYING)) continue;

                DENG2_ASSERT(ch->buffer->sample != 0);
                if(ch->buffer->sample->id != sample->id) continue;

                if(myPrio >= channelPrios[i] && (!selCh || channelPrios[i] <= lowPrio))
                {
                    selCh = ch;
                    lowPrio = channelPrios[i];
                }
            }

            if(!selCh)
            {
                // The new sound can't be played because we were unable to stop
                // enough channels to accommodate the limitation.
                LOG_AUDIO_XVERBOSE("Not playing #%i because all channels are busy")
                        << sample->id;
                return false;
            }

            // Stop this one.
            count--;
            Sfx_ChannelStop(selCh);
        }
    }

    // Hit count tells how many times the cached sound has been used.
    Sfx_CacheHit(sample->id);

    /*
     * Pick a channel for the sound. We will do our best to play the sound,
     * cancelling existing ones if need be. The ideal choice is a free channel
     * that is already loaded with the sample, in the correct format and mode.
     */
    sfxchannel_t *selCh = 0;

    BEGIN_COP;

    // First look through the stopped channels. At this stage we're very picky:
    // only the perfect choice will be good enough.
    selCh = Sfx_ChannelFindVacant(play3D, sample->bytesPer, sample->rate,
                                  sample->id);

    if(!selCh)
    {
        // Perhaps there is a vacant channel (with any sample, but preferably one
        // with no sample already loaded).
        selCh = Sfx_ChannelFindVacant(play3D, sample->bytesPer, sample->rate, 0);
    }

    if(!selCh)
    {
        // Try any non-playing channel in the correct format.
        selCh = Sfx_ChannelFindVacant(play3D, sample->bytesPer, sample->rate, -1);
    }

    if(!selCh)
    {
        // A perfect channel could not be found.
        // We must use a channel with the wrong format or decide which one of the
        // playing ones gets stopped.

        if(!haveChannelPrios)
        {
            Sfx_GetChannelPriorities(channelPrios);
        }

        // All channels with a priority less than or equal to ours can be stopped.
        sfxchannel_t *ch = channels;
        sfxchannel_t *prioCh = 0;
        for(int i = 0; i < numChannels; ++i, ch++)
        {
            // No buffer?
            if(!ch->buffer) continue;
            // Wrong mode?
            if(play3D != ((ch->buffer->flags & SFXBF_3D) != 0)) continue;

            if(!(ch->buffer->flags & SFXBF_PLAYING))
            {
                // This channel is not playing, just take it!
                selCh = ch;
                break;
            }

            // Are we more important than this sound?
            // We want to choose the lowest priority sound.
            if(myPrio >= channelPrios[i] &&
               (!prioCh || channelPrios[i] <= lowPrio))
            {
                prioCh = ch;
                lowPrio = channelPrios[i];
            }
        }

        // If a good low-priority channel was found, use it.
        if(prioCh)
        {
            selCh = prioCh;
            Sfx_ChannelStop(selCh);
        }
    }

    if(!selCh)
    {
        // A suitable channel was not found.
        END_COP;
        LOG_AUDIO_XVERBOSE("Failed to find suitable channel for sample %i") << sample->id;
        return false;
    }

    // Does our channel need to be reformatted?
    if(selCh->buffer->rate != sample->rate ||
       selCh->buffer->bytes != sample->bytesPer)
    {
        AudioDriver_SFX()->Destroy(selCh->buffer);
        // Create a new buffer with the correct format.
        selCh->buffer = AudioDriver_SFX()->Create(play3D ? SFXBF_3D : 0, sample->bytesPer * 8, sample->rate);
    }

    // Clear flags.
    selCh->buffer->flags &= ~(SFXBF_REPEAT | SFXBF_DONT_STOP);

    // Set buffer flags.
    if(flags & SF_REPEAT)
        selCh->buffer->flags |= SFXBF_REPEAT;
    if(flags & SF_DONT_STOP)
        selCh->buffer->flags |= SFXBF_DONT_STOP;

    // Init the channel information.
    selCh->flags &= ~(SFXCF_NO_ORIGIN | SFXCF_NO_ATTENUATION | SFXCF_NO_UPDATE);
    selCh->volume = volume;
    selCh->frequency = freq;
    if(!emitter && !fixedOrigin)
    {
        selCh->flags |= SFXCF_NO_ORIGIN;
        selCh->emitter = NULL;
    }
    else
    {
        selCh->emitter = emitter;
        if(fixedOrigin)
            memcpy(selCh->origin, fixedOrigin, sizeof(selCh->origin));
    }

    if(flags & SF_NO_ATTENUATION)
    {   // The sound can be heard from any distance.
        selCh->flags |= SFXCF_NO_ATTENUATION;
    }

    /**
     * Load in the sample. Must load prior to setting properties, because
     * the audio driver might actually create the real buffer only upon loading.
     *
     * @note The sample is not reloaded if a sample with the same ID is already
     * loaded on the channel.
     */
    if(!selCh->buffer->sample || selCh->buffer->sample->id != sample->id)
    {
        AudioDriver_SFX()->Load(selCh->buffer, sample);
    }

    // Update channel properties.
    Sfx_ChannelUpdate(selCh);

    // 3D sounds need a few extra properties set up.
    if(play3D)
    {
        // Init the buffer's min/max distances.
        // This is only done once, when the sound is started (i.e., here).
        AudioDriver_SFX()->Set(selCh->buffer, SFXBP_MIN_DISTANCE,
                               (selCh->flags & SFXCF_NO_ATTENUATION)? 10000 :
                               soundMinDist);

        AudioDriver_SFX()->Set(selCh->buffer, SFXBP_MAX_DISTANCE,
                               (selCh->flags & SFXCF_NO_ATTENUATION)? 20000 :
                               soundMaxDist);
    }

    // This'll commit all the deferred properties.
    AudioDriver_SFX()->Listener(SFXLP_UPDATE, 0);

    // Start playing.
    AudioDriver_SFX()->Play(selCh->buffer);

    END_COP;

    // Take note of the start time.
    selCh->startTime = nowTime;

    // Sound successfully started.
    return true;
}

/**
 * Update channel and listener properties.
 */
void Sfx_Update(void)
{
    int                 i;
    sfxchannel_t*       ch;

    // If the display player doesn't have a mobj, no positioning is done.
    listener = S_GetListenerMobj();

    // Update channels.
    for(i = 0, ch = channels; i < numChannels; ++i, ch++)
    {
        if(!ch->buffer || !(ch->buffer->flags & SFXBF_PLAYING))
            continue; // Not playing sounds on this...

        Sfx_ChannelUpdate(ch);
    }

    // Update listener.
    Sfx_ListenerUpdate();
}

void Sfx_StartFrame()
{
    LOG_AS("Sfx_StartFrame");

    static int old16Bit = false;
    static int oldRate = 11025;

    if(!sfxAvail)
        return;

    // Tell the audio driver that the sound frame begins.
    AudioDriver_Interface(AudioDriver_SFX())->Event(SFXEV_BEGIN);

    // Have there been changes to the cvar settings?
    Sfx_3DMode(sfx3D);

    // Check that the rate is valid.
    if(sfxSampleRate != 11025 && sfxSampleRate != 22050 && sfxSampleRate != 44100)
    {
        LOG_AUDIO_WARNING("\"sound-rate\" corrected to 11025 from invalid value (%i)") << sfxSampleRate;
        sfxSampleRate = 11025;
    }

    // Do we need to change the sample format?
    if(old16Bit != sfx16Bit || oldRate != sfxSampleRate)
    {
        Sfx_SampleFormat(sfx16Bit ? 16 : 8, sfxSampleRate);
        old16Bit = sfx16Bit;
        oldRate = sfxSampleRate;
    }

    // Should we purge the cache (to conserve memory)?
    Sfx_PurgeCache();
}

void Sfx_EndFrame(void)
{
    if(!sfxAvail)
        return;

    if(!BusyMode_Active())
    {
        Sfx_Update();
    }

    // The sound frame ends.
    AudioDriver_Interface(AudioDriver_SFX())->Event(SFXEV_END);
}

/**
 * Creates the buffers for the channels.
 *
 * @param num2D     Number of 2D the rest will be 3D.
 * @param bits      Bits per sample.
 * @param rate      Sample rate (Hz).
 */
static void createChannels(int num2D, int bits, int rate)
{
    LOG_AS("Sfx_CreateChannels");

    // Change the primary buffer's format to match the channel format.
    float parm[2] = { float(bits), float(rate) };
    AudioDriver_SFX()->Listenerv(SFXLP_PRIMARY_FORMAT, parm);

    // Try to create a buffer for each channel.
    sfxchannel_t *ch = channels;
    for(int i = 0; i < numChannels; ++i, ch++)
    {
        ch->buffer = AudioDriver_SFX()->Create(num2D-- > 0 ? 0 : SFXBF_3D, bits, rate);
        if(!ch->buffer)
        {
            LOG_AUDIO_WARNING("Failed to create buffer for #%i") << i;
            continue;
        }
    }
}

/**
 * Stop all channels and destroy their buffers.
 */
void Sfx_DestroyChannels()
{
    BEGIN_COP;
    for(int i = 0; i < numChannels; ++i)
    {
        Sfx_ChannelStop(channels + i);

        if(channels[i].buffer)
            AudioDriver_SFX()->Destroy(channels[i].buffer);
        channels[i].buffer = 0;
    }
    END_COP;
}

void Sfx_InitChannels()
{
    numChannels = sfxMaxChannels;

    // The -sfxchan option can be used to change the number of channels.
    if(CommandLine_CheckWith("-sfxchan", 1))
    {
        numChannels = strtol(CommandLine_Next(), 0, 0);
        if(numChannels < 1)
            numChannels = 1;
        if(numChannels > SFX_MAX_CHANNELS)
            numChannels = SFX_MAX_CHANNELS;

        LOG_AUDIO_NOTE("Initialized %i sound effect channels") << numChannels;
    }

    // Allocate and init the channels.
    channels = (sfxchannel_t *) Z_Calloc(sizeof(*channels) * numChannels, PU_APPSTATIC, 0);

    // Create channels according to the current mode.
    createChannels(sfx3D? sfxDedicated2D : numChannels, sfxBits, sfxRate);
}

/**
 * Frees all memory allocated for the channels.
 */
void Sfx_ShutdownChannels(void)
{
    Sfx_DestroyChannels();

    if(channels)
        Z_Free(channels);
    channels = NULL;
    numChannels = 0;
}

/**
 * Start the channel refresh thread. It will stop on its own when it
 * notices that the rest of the sound system is going down.
 */
void Sfx_StartRefresh()
{
    LOG_AS("Sfx_StartRefresh");

    int disableRefresh = false;

    refreshing = false;
    allowRefresh = true;

    if(!AudioDriver_SFX()) goto noRefresh; // Nothing to refresh.

    if(AudioDriver_SFX()->Getv) AudioDriver_SFX()->Getv(SFXIP_DISABLE_CHANNEL_REFRESH, &disableRefresh);
    if(!disableRefresh)
    {
        // Start the refresh thread. It will run until the Sfx module is shut down.
        refreshHandle = Sys_StartThread(Sfx_ChannelRefreshThread, NULL);
        if(!refreshHandle)
        {
            throw de::Error("Sfx_StartRefresh", "Failed to start refresh thread.");
        }
    }
    else
    {
noRefresh:
        LOGDEV_AUDIO_NOTE("Audio driver does not require a refresh thread");
    }
}

dd_bool Sfx_Init()
{
    // Already initialized?
    if(sfxAvail) return true;

    // Check if sound has been disabled with a command line option.
    if(CommandLine_Exists("-nosfx"))
    {
        LOG_AUDIO_NOTE("Sound Effects disabled");
        return true;
    }

    LOG_AUDIO_VERBOSE("Initializing Sound Effects subsystem...");

    // No interface for SFX playback?
    if(!AudioDriver_SFX()) return false;

    // This is based on the scientific calculations that if the DOOM marine
    // is 56 units tall, 60 is about two meters.
    //// @todo Derive from the viewheight.
    AudioDriver_SFX()->Listener(SFXLP_UNITS_PER_METER, 30);
    AudioDriver_SFX()->Listener(SFXLP_DOPPLER, 1.5f);

    // The audio driver is working, let's create the channels.
    Sfx_InitChannels();

    // Init the sample cache.
    Sfx_InitCache();

    // The Sfx module is now available.
    sfxAvail = true;

    // Initialize reverb effects to off.
    Sfx_ListenerNoReverb();

    // Finally, start the refresh thread.
    Sfx_StartRefresh();
    return true;
}

void Sfx_Shutdown(void)
{
    if(!sfxAvail)
        return; // Not initialized.

    // These will stop further refreshing.
    sfxAvail = false;
    allowRefresh = false;

    if(refreshHandle)
    {
        // Wait for the sfx refresh thread to stop.
        Sys_WaitThread(refreshHandle, 2000, NULL);
        refreshHandle = 0;
    }

    // Destroy the sample cache.
    Sfx_ShutdownCache();

    // Destroy channels.
    Sfx_ShutdownChannels();
}

void Sfx_Reset(void)
{
    if(!sfxAvail) return;

    listenerCluster = 0;

    // Stop all channels.
    for(int i = 0; i < numChannels; ++i)
    {
        Sfx_ChannelStop(&channels[i]);
    }

    // Free all samples.
    Sfx_ShutdownCache();
}

/**
 * Destroys all channels and creates them again.
 */
void Sfx_RecreateChannels(void)
{
    Sfx_DestroyChannels();
    createChannels(sfx3D ? sfxDedicated2D : numChannels, sfxBits, sfxRate);
}

/**
 * Swaps between 2D and 3D sound modes. Called automatically by
 * Sfx_StartFrame when cvar changes.
 */
void Sfx_3DMode(dd_bool activate)
{
    static int old3DMode = false;

    if(old3DMode == activate)
        return; // No change; do nothing.

    sfx3D = old3DMode = activate;
    // To make the change effective, re-create all channels.
    Sfx_RecreateChannels();

    // If going to 2D, make sure the reverb is off.
    if(!sfx3D)
    {
        Sfx_ListenerNoReverb();
    }
}

/**
 * Reconfigures the sample bits and rate. Called automatically by
 * Sfx_StartFrame when changes occur.
 */
void Sfx_SampleFormat(int newBits, int newRate)
{
    if(sfxBits == newBits && sfxRate == newRate)
        return; // No change; do nothing.

    // Set the new buffer format.
    sfxBits = newBits;
    sfxRate = newRate;
    Sfx_RecreateChannels();

    // The cache just became useless, clear it.
    Sfx_ShutdownCache();
}

void Sfx_MapChange(void)
{
    sfxchannel_t *ch = channels;
    for(int i = 0; i < numChannels; ++i, ch++)
    {
        if(ch->emitter)
        {
            // Mobjs are about to be destroyed.
            ch->emitter = 0;

            // Stop all channels with an origin.
            Sfx_ChannelStop(ch);
        }
    }

    // Sectors, too, for that matter.
    listenerCluster = 0;
}

void Sfx_DebugInfo(void)
{
#ifdef __CLIENT__
    int i, lh;
    sfxchannel_t* ch;
    char buf[200];
    uint cachesize, ccnt;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glEnable(GL_TEXTURE_2D);

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(1, 1, 0, 1);

    lh = FR_SingleLineHeight("Q");
    if(!sfxAvail)
    {
        FR_DrawTextXY("Sfx disabled", 0, 0);
        glDisable(GL_TEXTURE_2D);
        return;
    }

    if(refMonitor)
        FR_DrawTextXY("!", 0, 0);

    // Sample cache information.
    Sfx_GetCacheInfo(&cachesize, &ccnt);
    sprintf(buf, "Cached:%i (%i)", cachesize, ccnt);
    FR_SetColor(1, 1, 1);
    FR_DrawTextXY(buf, 10, 0);

    // Print a line of info about each channel.
    for(i = 0, ch = channels; i < numChannels; ++i, ch++)
    {
        if(ch->buffer && (ch->buffer->flags & SFXBF_PLAYING))
        {
            FR_SetColor(1, 1, 1);
        }
        else
        {
            FR_SetColor(1, 1, 0);
        }

        sprintf(buf, "%02i: %c%c%c v=%3.1f f=%3.3f st=%i et=%u mobj=%i", i,
                !(ch->flags & SFXCF_NO_ORIGIN) ? 'O' : '.',
                !(ch->flags & SFXCF_NO_ATTENUATION) ? 'A' : '.',
                ch->emitter ? 'E' : '.', ch->volume, ch->frequency,
                ch->startTime, ch->buffer ? ch->buffer->endTime : 0,
                ch->emitter? ch->emitter->thinker.id : 0);
        FR_DrawTextXY(buf, 5, lh * (1 + i * 2));

        if(!ch->buffer) continue;

        sprintf(buf,
                "    %c%c%c%c id=%03i/%-8s ln=%05i b=%i rt=%2i bs=%05i "
                "(C%05i/W%05i)", (ch->buffer->flags & SFXBF_3D) ? '3' : '.',
                (ch->buffer->flags & SFXBF_PLAYING) ? 'P' : '.',
                (ch->buffer->flags & SFXBF_REPEAT) ? 'R' : '.',
                (ch->buffer->flags & SFXBF_RELOAD) ? 'L' : '.',
                ch->buffer->sample ? ch->buffer->sample->id : 0,
                ch->buffer->sample ? defs.sounds[ch->buffer->sample->id].
                id : "", ch->buffer->sample ? ch->buffer->sample->size : 0,
                ch->buffer->bytes, ch->buffer->rate / 1000, ch->buffer->length,
                ch->buffer->cursor, ch->buffer->written);
        FR_DrawTextXY(buf, 5, lh * (2 + i * 2));
    }

    glDisable(GL_TEXTURE_2D);
#endif
}
