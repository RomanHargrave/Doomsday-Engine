/** @file m_mus2midi.cpp MUS to MIDI conversion.
 * @ingroup audio
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2007-2013 Daniel Swanson <danij@dengine.net>
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
#include "audio/m_mus2midi.h"

#include <doomsday/filesys/fs_util.h>
#include <de/Log>
#include <de/NativePath>
#include <stdio.h>
#include <string.h>

// MUS event types.
enum {
    MUS_EV_RELEASE_NOTE,
    MUS_EV_PLAY_NOTE,
    MUS_EV_PITCH_WHEEL,
    MUS_EV_SYSTEM,      ///< Valueless controller.
    MUS_EV_CONTROLLER,
    MUS_EV_FIVE,
    MUS_EV_SCORE_END,
    MUS_EV_SEVEN
};

// MUS controllers.
enum {
    MUS_CTRL_INSTRUMENT,
    MUS_CTRL_BANK,
    MUS_CTRL_MODULATION,
    MUS_CTRL_VOLUME,
    MUS_CTRL_PAN,
    MUS_CTRL_EXPRESSION,
    MUS_CTRL_REVERB,
    MUS_CTRL_CHORUS,
    MUS_CTRL_SUSTAIN_PEDAL,
    MUS_CTRL_SOFT_PEDAL,

    // The valueless controllers.
    MUS_CTRL_SOUNDS_OFF,
    MUS_CTRL_NOTES_OFF,
    MUS_CTRL_MONO,
    MUS_CTRL_POLY,
    MUS_CTRL_RESET_ALL,
    NUM_MUS_CTRLS
};

#pragma pack(1)
struct mus_header {
    char            ID[4]; ///< Identifier "MUS" 0x1A.
    ushort          scoreLen;
    ushort          scoreStart;
    ushort          channels; ///< Number of primary channels.
    ushort          secondaryChannels; ///< Number of secondary channels.
    ushort          instrCnt;
    ushort          padding;
    // The instrument list begins here.
};
#pragma pack()

typedef struct mus_event_s {
    byte            channel;
    byte            ev; // event.
    byte            last;
} mus_event_t;

typedef struct midi_event_s {
    uint            deltaTime;
    byte            command;
    byte            size;
    byte            parms[2];
} midi_event_t;

static int readTime; // In ticks.
static byte* readPos;

static byte chanVols[16]; // Last volume for each channel.

static char ctrlMus2Midi[NUM_MUS_CTRLS] = {
     0, ///< Not used.
     0, ///< Bank select.
     1, ///< Modulation.
     7, ///< Volume.
    10, ///< Pan.
    11, ///< Expression.
    91, ///< Reverb.
    93, ///< Chorus.
    64, ///< Sustain pedal.
    67, ///< Soft pedal.

    // The valueless controllers:
    120, ///< All sounds off.
    123, ///< All notes off.
    126, ///< Mono.
    127, ///< Poly.
    121  ///< Reset all controllers.
};

static dd_bool getNextEvent(midi_event_t* ev)
{
    int i;
    mus_event_t evDesc;
    byte musEvent;

    ev->deltaTime = readTime;
    readTime = 0;

    musEvent = *readPos++;
    evDesc.channel = musEvent & 0xf;
    evDesc.ev = (musEvent >> 4) & 0x7;
    evDesc.last = (musEvent >> 7) & 0x1;
    ev->command = 0;
    ev->size = 0;
    memset(ev->parms, 0, sizeof(ev->parms));

    // Construct the MIDI event.
    switch(evDesc.ev)
    {
    case MUS_EV_PLAY_NOTE:
        ev->command = 0x90;
        ev->size = 2;
        // Which note?
        ev->parms[0] = *readPos++;
        // Is the volume there, too?
        if(ev->parms[0] & 0x80)
            chanVols[evDesc.channel] = *readPos++;
        ev->parms[0] &= 0x7f;
        if((i = chanVols[evDesc.channel]) > 127)
            i = 127;
        ev->parms[1] = i;
        break;

    case MUS_EV_RELEASE_NOTE:
        ev->command = 0x80;
        ev->size = 2;
        // Which note?
        ev->parms[0] = *readPos++;
        break;

    case MUS_EV_CONTROLLER:
        ev->command = 0xb0;
        ev->size = 2;
        ev->parms[0] = *readPos++;
        ev->parms[1] = *readPos++;
        // The instrument control is mapped to another kind of MIDI event.
        if(ev->parms[0] == MUS_CTRL_INSTRUMENT)
        {
            ev->command = 0xc0;
            ev->size = 1;
            ev->parms[0] = ev->parms[1];
        }
        else
        {
            // Use the conversion table.
            ev->parms[0] = ctrlMus2Midi[ev->parms[0]];
        }
        break;

        // 2 bytes, 14 bit value. 0x2000 is the center.
        // First seven bits go to parm1, the rest to parm2.
    case MUS_EV_PITCH_WHEEL:
        ev->command = 0xe0;
        ev->size = 2;
        i = *readPos++ << 6;
        ev->parms[0] = i & 0x7f;
        ev->parms[1] = i >> 7;
        break;

    case MUS_EV_SYSTEM: // Is this ever used?
        ev->command = 0xb0;
        ev->size = 2;
        ev->parms[0] = ctrlMus2Midi[*readPos++];
        break;

    case MUS_EV_SCORE_END:
        // We're done.
        return false;

    default:
        LOG_RES_WARNING("Invalid MUS format music data");
        LOGDEV_RES_WARNING("Unknown MUS event %d while converting MUS to MIDI") << evDesc.ev;
        return false;
    }

    // Choose the channel.
    i = evDesc.channel;
    // Redirect MUS channel 16 to MIDI channel 10 (percussion).
    if(i == 15)
        i = 9;
    else if(i == 9)
        i = 15;
    ev->command |= i;

    // Check if this was the last event in a group.
    if(!evDesc.last)
        return true;

    // Read the time delta.
    readTime = 0;
    do
    {
        i = *readPos++;
        readTime = (readTime << 7) + (i & 0x7f);
    } while(i & 0x80);

    return true;
}

dd_bool M_Mus2Midi(void* data, size_t length, const char* outFile)
{
    unsigned char buffer[80];
    int i, trackSizeOffset, trackSize;
    struct mus_header* header;
    ddstring_t nativePath;
    midi_event_t ev;
    FILE* file;

    DENG_UNUSED(length);
    LOG_AS("M_Mus2Midi");

    if(!outFile || !outFile[0]) return false;

    Str_Init(&nativePath); Str_Set(&nativePath, outFile);
    F_ToNativeSlashes(&nativePath, &nativePath);

    /// @todo Reimplement using higher level methods for file IO.
    file = fopen(Str_Text(&nativePath), "wb");
    if(!file)
    {
        LOG_RES_WARNING("Failed opening output file \"%s\"") << de::NativePath(Str_Text(&nativePath)).pretty();
        Str_Free(&nativePath);
        return false;
    }
    Str_Free(&nativePath);

    // Start with the MIDI header.
    strcpy((char*)buffer, "MThd");
    fwrite(buffer, 4, 1, file);

    // Header size.
    memset(buffer, 0, 3);
    buffer[3] = 6;
    fwrite(buffer, 4, 1, file);

    // Format (single track).
    buffer[0] = 0;
    buffer[1] = 0;

    // Number of tracks.
    buffer[2] = 0;
    buffer[3] = 1;

    // Delta ticks per quarter note (140).
    buffer[4] = 0;
    buffer[5] = 140;

    fwrite(buffer, 6, 1, file);

    // Track header.
    strcpy((char*)buffer, "MTrk");
    fwrite(buffer, 4, 1, file);

    // Length of the track in bytes.
    memset(buffer, 0, 4);
    trackSizeOffset = ftell(file);
    fwrite(buffer, 4, 1, file); // Updated later.

    // The first MIDI ev sets the tempo.
    buffer[0] = 0; // No delta ticks.
    buffer[1] = 0xff;
    buffer[2] = 0x51;
    buffer[3] = 3;
    buffer[4] = 0xf; // Exactly one second per quarter note.
    buffer[5] = 0x42;
    buffer[6] = 0x40;
    fwrite(buffer, 7, 1, file);

    header = (struct mus_header *) data;
    readPos = (byte*)data + DD_USHORT(header->scoreStart);
    readTime = 0;
    // Init channel volumes.
    for(i = 0; i < 16; ++i)
        chanVols[i] = 64;

    while(getNextEvent(&ev))
    {
        // Delta time. Split into 7-bit segments.
        if(ev.deltaTime == 0)
        {
            buffer[0] = 0;
            fwrite(buffer, 1, 1, file);
        }
        else
        {
            i = -1;
            while(ev.deltaTime > 0)
            {
                buffer[++i] = ev.deltaTime & 0x7f;
                if(i > 0) buffer[i] |= 0x80;
                ev.deltaTime >>= 7;
            }

            // The bytes are written starting from the MSB.
            for(; i >= 0; --i)
                fwrite(&buffer[i], 1, 1, file);
        }

        // The ev data.
        fwrite(&ev.command, 1, 1, file);
        fwrite(&ev.parms, 1, ev.size, file);
    }

    // End of track.
    buffer[0] = 0;
    buffer[1] = 0xff;
    buffer[2] = 0x2f;
    buffer[3] = 0;
    fwrite(buffer, 4, 1, file);

    // All the MIDI data has now been written. Update the track length.
    trackSize = ftell(file) - trackSizeOffset - 4;
    fseek(file, trackSizeOffset, SEEK_SET);

    buffer[3] = trackSize & 0xff;
    buffer[2] = (trackSize >> 8) & 0xff;
    buffer[1] = (trackSize >> 16) & 0xff;
    buffer[0] = trackSize >> 24;
    fwrite(buffer, 4, 1, file);

    fclose(file);

    return true; // Success!
}
