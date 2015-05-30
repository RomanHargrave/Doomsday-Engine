/** @file displaymode_windows.cpp  Windows implementation of the DisplayMode native functionality.
 * @ingroup gl
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include <QDebug>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <icm.h>
#include <math.h>
#include <assert.h>

#include <vector>

#include "de/gui/displaymode_native.h"
#include "de/PersistentCanvasWindow"

static std::vector<DEVMODE> devModes;
static DEVMODE currentDevMode;

static DisplayMode devToDisplayMode(const DEVMODE& d)
{
    DisplayMode m;
    m.width = d.dmPelsWidth;
    m.height = d.dmPelsHeight;
    m.depth = d.dmBitsPerPel;
    m.refreshRate = d.dmDisplayFrequency;
    return m;
}

void DisplayMode_Native_Init(void)
{
    // Let's see which modes are available.
    for(int i = 0; ; i++)
    {
        DEVMODE mode;
        de::zap(mode);
        mode.dmSize = sizeof(mode);
        if(!EnumDisplaySettings(NULL, i, &mode))
            break; // That's all.

        devModes.push_back(mode);
    }

    // And which is the current mode?
    de::zap(currentDevMode);
    currentDevMode.dmSize = sizeof(currentDevMode);
    EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &currentDevMode);
}

void DisplayMode_Native_Shutdown(void)
{
    devModes.clear();
}

int DisplayMode_Native_Count(void)
{
    return devModes.size();
}

void DisplayMode_Native_GetMode(int index, DisplayMode* mode)
{
    DENG2_ASSERT(index >= 0 && index < DisplayMode_Native_Count());
    *mode = devToDisplayMode(devModes[index]);
}

void DisplayMode_Native_GetCurrentMode(DisplayMode* mode)
{
    *mode = devToDisplayMode(currentDevMode);
}

static int findMode(const DisplayMode* mode)
{
    for(int i = 0; i < DisplayMode_Native_Count(); ++i)
    {
        DisplayMode d = devToDisplayMode(devModes[i]);
        if(DisplayMode_IsEqual(&d, mode))
        {
            return i;
        }
    }
    return -1;
}

int DisplayMode_Native_Change(const DisplayMode* mode, int shouldCapture)
{
    DENG2_ASSERT(mode);
    DENG2_ASSERT(findMode(mode) >= 0);

    DEVMODE m = devModes[findMode(mode)];
    m.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;

    if(ChangeDisplaySettings(&m, shouldCapture? CDS_FULLSCREEN : 0) != DISP_CHANGE_SUCCESSFUL)
        return false;

    currentDevMode = m;
    return true;
}

void DisplayMode_Native_SetColorTransfer(DisplayColorTransfer const *colors)
{
    if(!de::CanvasWindow::mainExists()) return;

    HWND hWnd = (HWND) de::CanvasWindow::main().nativeHandle();
    DENG2_ASSERT(hWnd != 0);

    HDC hDC = GetDC(hWnd);
    if(hDC)
    {
        SetDeviceGammaRamp(hDC, (void*) colors->table);
        ReleaseDC(hWnd, hDC);
    }
}

void DisplayMode_Native_GetColorTransfer(DisplayColorTransfer *colors)
{
    HWND hWnd = (HWND) de::CanvasWindow::main().nativeHandle();
    DENG2_ASSERT(hWnd != 0);

    HDC hDC = GetDC(hWnd);
    if(hDC)
    {
        GetDeviceGammaRamp(hDC, (void *) colors->table);
        ReleaseDC(hWnd, hDC);
    }
}
