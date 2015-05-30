/** @file monitor.cpp Implementation of network traffic monitoring. 
 * @ingroup network
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "de_console.h"

#if _DEBUG

static uint monitor[256];
static uint monitoredBytes;
static uint monitoredPackets;
static size_t monitorMaxSize;

static void Monitor_Start(int maxPacketSize)
{
    monitorMaxSize = maxPacketSize;
    monitoredBytes = monitoredPackets = 0;
    memset(&monitor, 0, sizeof(monitor));
}

static void Monitor_Stop(void)
{
    monitorMaxSize = 0;
}

void Monitor_Add(const uint8_t* bytes, size_t size)
{
    if(size <= monitorMaxSize)
    {
        uint i;
        monitoredPackets++;
        monitoredBytes += size;
        for(i = 0; i < size; ++i) monitor[bytes[i]]++;
    }
}

static void Monitor_Print(void)
{
    if(!monitoredBytes)
    {
        LOGDEV_NET_MSG("Nothing has been sent yet");
        return;
    }
    LOGDEV_NET_MSG("%i bytes sent (%i packets)") << monitoredBytes << monitoredPackets;
    int i, k;
    for(i = 0, k = 0; i < 256; ++i)
    {
        // LogBuffer_Printf uses manual newlines.

        if(!k) LogBuffer_Printf(DE2_LOG_DEV, "    ");

        LogBuffer_Printf(DE2_LOG_DEV, "%10.10f", (double)(monitor[i]) / (double)monitoredBytes);

        // Break lines.
        if(++k == 4)
        {
            k = 0;
            LogBuffer_Printf(DE2_LOG_DEV, ",\n");
        }
        else
        {
            LogBuffer_Printf(DE2_LOG_DEV, ", ");
        }
    }
    if(k) LogBuffer_Printf(DE2_LOG_DEV, "\n");
}

D_CMD(NetFreqs)
{
    DENG2_UNUSED(src);

    if(argc == 1) // No args?
    {
        LOG_SCR_NOTE("Usage:\n  %s start (maxsize)\n  %s stop\n  %s print/show") << argv[0] << argv[0] << argv[0];
        return true;
    }
    if(argc == 3 && !strcmp(argv[1], "start"))
    {
        Monitor_Start(strtoul(argv[2], 0, 10));
        return true;
    }
    if(argc == 2 && !strcmp(argv[1], "stop"))
    {
        Monitor_Stop();
        return true;
    }
    if(argc == 2 && (!strcmp(argv[1], "print") || !strcmp(argv[1], "show")))
    {
        Monitor_Print();
        return true;
    }
    return false;
}

#endif // _DEBUG
