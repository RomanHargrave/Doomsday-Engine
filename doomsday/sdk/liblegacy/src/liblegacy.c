/**
 * @file liblegacy.c
 * Main interface.
 *
 * @authors Copyright (c) 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/liblegacy.h"
#include "de/binangle.h"
#include "de/concurrency.h"
#include "de/timer.h"
#include "memoryzone_private.h"

#include <stdlib.h>
#include <de/Garbage>

void Libdeng_Init(void)
{
    bamsInit();
    Timer_Init();
    Z_Init();
    Sys_MarkAsMainThread();
}

void Libdeng_Shutdown(void)
{
    Z_Shutdown();
    Timer_Shutdown();
}

void Libdeng_BadAlloc(void)
{
    exit(-1);
}
