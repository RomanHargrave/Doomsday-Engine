/**
 * @file displaymode.h
 * Changing and enumerating available display modes. @ingroup gl
 *
 * High-level logic for enumerating, selecting, and changing display modes. See
 * displaymode_native.h for the platform-specific low-level routines.
 *
 * @todo This is C because it was relocated from the client. It should be
 * converted to a C++.
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_DISPLAYMODE_H
#define LIBGUI_DISPLAYMODE_H

#include "libgui.h"
#include "de/libcore.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct displaymode_s {
    int width;
    int height;
    float refreshRate; // might be zero
    int depth;

    // Calculated automatically:
    int ratioX;
    int ratioY;
} DisplayMode;

typedef struct displaycolortransfer_s {
    unsigned short table[3 * 256]; // 0-255:red, 256-511:green, 512-767:blue (range: 0..ffff)
} DisplayColorTransfer;

/**
 * Initializes the DisplayMode class. Enumerates all available display modes and
 * saves the current display mode. Must be called at engine startup.
 *
 * @return @c true, if display modes were initialized successfully.
 */
LIBGUI_PUBLIC int DisplayMode_Init(void);

/**
 * Gets the current color transfer function and saves it as the one that will be
 * restored at shutdown.
 */
LIBGUI_PUBLIC void DisplayMode_SaveOriginalColorTransfer(void);

/**
 * Shuts down the DisplayMode class. The current display mode is restored to what
 * it was at initialization time.
 */
LIBGUI_PUBLIC void DisplayMode_Shutdown(void);

/**
 * Returns the display mode that was in use when DisplayMode_Init() was called.
 */
LIBGUI_PUBLIC DisplayMode const *DisplayMode_OriginalMode(void);

/**
 * Returns the current display mode.
 */
LIBGUI_PUBLIC DisplayMode const *DisplayMode_Current(void);

/**
 * Returns the number of available display modes.
 */
LIBGUI_PUBLIC int DisplayMode_Count(void);

/**
 * Returns one of the available display modes. Use DisplayMode_Count() to
 * determine how many modes are available.
 *
 * @param index  Index of the mode, must be between 0 and DisplayMode_Count() - 1.
 */
LIBGUI_PUBLIC DisplayMode const *DisplayMode_ByIndex(int index);

/**
 * Finds the closest available mode to the given criteria.
 *
 * @param width  Width in pixels.
 * @param height  Height in pixels.
 * @param depth  Color depth (bits per color component).
 * @param freq  Refresh rate. If zero, prefers rates closest to the mode at startup time.
 *
 * @return Mode that most closely matches the criteria. Always returns one of
 * the available modes; returns @c NULL only if DisplayMode_Init() has not yet
 * been called.
 */
LIBGUI_PUBLIC DisplayMode const *DisplayMode_FindClosest(int width, int height, int depth, float freq);

/**
 * Determines if two display modes are equivalent.
 *
 * @param a  DisplayMode instance.
 * @param b  DisplayMode instance.
 *
 * @return  @c true or @c false.
 */
LIBGUI_PUBLIC int DisplayMode_IsEqual(DisplayMode const *a, DisplayMode const *b);

/**
 * Changes the display mode.
 *
 * @param mode  Mode to change to. Must be one of the modes returned by the functions in displaymode.h.
 * @param shouldCapture  @c true, if the mode is intended to capture the entire display.
 *
 * @return @c true, if a mode change occurred. @c false, otherwise (bad mode or
 * when attempting to change to the current mode).
 */
LIBGUI_PUBLIC int DisplayMode_Change(DisplayMode const *mode, int shouldCapture);

/**
 * Gets the current color transfer table.
 *
 * @param colors  Color transfer.
 */
LIBGUI_PUBLIC void DisplayMode_GetColorTransfer(DisplayColorTransfer *colors);

/**
 * Sets the color transfer table.
 *
 * @param colors  Color transfer.
 */
LIBGUI_PUBLIC void DisplayMode_SetColorTransfer(DisplayColorTransfer const *colors);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBGUI_DISPLAYMODE_H
