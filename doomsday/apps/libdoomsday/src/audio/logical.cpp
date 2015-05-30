/**\file logical.cpp
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

/**
 * @par The Logical Sound Manager
 *
 * The Logical Sound Manager. Tracks all currently playing sounds
 * in the world, regardless of whether Sfx is available or if the
 * sounds are actually audible to anyone.
 *
 * Uses PU_MAP, so this has to be inited for every map.
 * (Done via S_MapChange()).
 *
 * @todo This should be part of an audio system base class that can be used
 * both by the client and the server. -jk
 */

#include "doomsday/audio/logical.h"
#include <de/timer.h>
#include <de/memoryzone.h>

// The logical sounds hash table uses sound IDs as keys.
#define LOGIC_HASH_SIZE     64

#define PURGE_INTERVAL      2000    // 2 seconds

typedef struct logicsound_s {
    struct logicsound_s *next, *prev;
    int     id;
    mobj_t *origin;
    uint    endTime;
    byte    isRepeating;
} logicsound_t;

typedef struct logichash_s {
    logicsound_t *first, *last;
} logichash_t;

static logichash_t logicHash[LOGIC_HASH_SIZE];
static dd_bool logicalOneSoundPerEmitter;
static uint (*logicalSoundLengthCallback)(int);

void Sfx_Logical_SetOneSoundPerEmitter(dd_bool enabled)
{
    logicalOneSoundPerEmitter = enabled;
}

void Sfx_Logical_SetSampleLengthCallback(uint (*callback)(int))
{
    logicalSoundLengthCallback = callback;
}

/*
 * Initialize the Logical Sound Manager for a new map.
 */
void Sfx_InitLogical(void)
{
    // Memory in the hash table is PU_MAP, so this is acceptable.
    memset(logicHash, 0, sizeof(logicHash));
}

/*
 * Return a pointer the logical sounds hash.
 */
logichash_t *Sfx_LogicHash(int id)
{
    return &logicHash[(uint) id % LOGIC_HASH_SIZE];
}

/*
 * Create and link a new logical sound hash table node.
 */
logicsound_t *Sfx_CreateLogical(int id)
{
    logichash_t *hash = Sfx_LogicHash(id);
    logicsound_t *node = reinterpret_cast<logicsound_t *>(Z_Calloc(sizeof(logicsound_t), PU_MAP, 0));

    if(hash->last)
    {
        hash->last->next = node;
        node->prev = hash->last;
    }
    hash->last = node;
    if(!hash->first)
        hash->first = node;

    node->id = id;
    return node;
}

/*
 * Unlink and destroy a logical sound hash table node.
 */
void Sfx_DestroyLogical(logicsound_t * node)
{
    logichash_t *hash = Sfx_LogicHash(node->id);

    if(hash->first == node)
        hash->first = node->next;
    if(hash->last == node)
        hash->last = node->prev;
    if(node->next)
        node->next->prev = node->prev;
    if(node->prev)
        node->prev->next = node->next;

#ifdef _DEBUG
    memset(node, 0xDD, sizeof(*node));
#endif
    Z_Free(node);
}

/*
 * The sound is entered into the list of playing sounds. Called when a
 * 'world class' sound is started, regardless of whether it's actually
 * started on the local system.
 */
void Sfx_StartLogical(int id, mobj_t *origin, dd_bool isRepeating)
{
    DENG_ASSERT(logicalSoundLengthCallback != 0);

    logicsound_t *node;
    uint length = (isRepeating ? 1 : logicalSoundLengthCallback(id));

    if(!length)
    {
        // This is not a valid sound.
        return;
    }

    if(origin && logicalOneSoundPerEmitter)
    {
        // Stop all previous sounds from this origin (only one per origin).
        Sfx_StopLogical(0, origin);
    }

    id &= ~DDSF_FLAG_MASK;
    node = Sfx_CreateLogical(id);
    node->origin = origin;
    node->isRepeating = isRepeating;
    node->endTime = Timer_RealMilliseconds() + length;
}

/*
 * The sound is removed from the list of playing sounds. Called whenever
 * a sound is stopped, regardless of whether it was actually playing on
 * the local system. Returns the number of sounds stopped.
 *
 * id=0, origin=0: stop everything
 */
int Sfx_StopLogical(int id, mobj_t *origin)
{
    logicsound_t *it, *next;
    int     stopCount = 0;
    int     i;

    if(id)
    {
        for(it = Sfx_LogicHash(id)->first; it; it = next)
        {
            next = it->next;
            if(it->id == id && it->origin == origin)
            {
                Sfx_DestroyLogical(it);
                stopCount++;
            }
        }
    }
    else
    {
        // Browse through the entire hash.
        for(i = 0; i < LOGIC_HASH_SIZE; i++)
        {
            for(it = logicHash[i].first; it; it = next)
            {
                next = it->next;
                if(!origin || it->origin == origin)
                {
                    Sfx_DestroyLogical(it);
                    stopCount++;
                }
            }
        }
    }

    return stopCount;
}

/*
 * Remove stopped logical sounds from the hash.
 */
void Sfx_PurgeLogical(void)
{
    static uint lastTime = 0;
    uint    i, nowTime = Timer_RealMilliseconds();
    logicsound_t *it, *next;

    if(nowTime - lastTime < PURGE_INTERVAL)
    {
        // It's too early.
        return;
    }
    lastTime = nowTime;

    // Check all sounds in the hash.
    for(i = 0; i < LOGIC_HASH_SIZE; i++)
    {
        for(it = logicHash[i].first; it; it = next)
        {
            next = it->next;
            if(!it->isRepeating && it->endTime < nowTime)
            {
                // This has stopped.
                Sfx_DestroyLogical(it);
            }
        }
    }
}

/*
 * Returns true if the sound is currently playing somewhere in the world.
 * It doesn't matter if it's audible or not.
 *
 * id=0: true if any sounds are playing using the specified origin
 */
dd_bool Sfx_IsPlaying(int id, mobj_t *origin)
{
    uint    nowTime = Timer_RealMilliseconds();
    logicsound_t *it;
    int     i;

    if(id)
    {
        for(it = Sfx_LogicHash(id)->first; it; it = it->next)
        {
            if(it->id == id && it->origin == origin &&
               (it->endTime > nowTime || it->isRepeating))
            {
                // This one is still playing.
                return true;
            }
        }
    }
    else if(origin)
    {
        // Check if the origin is playing any sound.
        for(i = 0; i < LOGIC_HASH_SIZE; i++)
        {
            for(it = logicHash[i].first; it; it = it->next)
            {
                if(it->origin == origin &&
                   (it->endTime > nowTime || it->isRepeating))
                {
                    // This one is playing.
                    return true;
                }
            }
        }
    }

    // The sound was not found.
    return false;
}
