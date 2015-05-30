/** @file server_dummies.h Dummy functions for the server.
 * @ingroup server
 *
 * Empty dummy functions that replace certain client-only functionality on
 * engine-side. Ideally none of these would be needed; each one represents a
 * client-only function call that should not be done in common/shared code.
 * (There should be no shared code outside libcore/liblegacy.)
 *
 * @todo Add a @c libdeng_gui for UI/graphics code. Many of these belong there
 * instead of being exported out of the client executable for game plugins'
 * needs.
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef SERVER_DUMMIES_H
#define SERVER_DUMMIES_H

#include <de/liblegacy.h>
#include <doomsday/defs/ded.h>
#include "world/map.h"

#ifndef __SERVER__
#  error "Attempted to include server's header in a non-server build"
#endif

DENG_EXTERN_C void Con_TransitionTicker(timespan_t t);
DENG_EXTERN_C void Con_SetProgress(int progress);

DENG_EXTERN_C void GL_Shutdown();

DENG_EXTERN_C void R_RenderPlayerView(int num);
DENG_EXTERN_C void R_SetBorderGfx(Uri const *const *paths);
DENG_EXTERN_C void R_SkyParams(int layer, int param, void *data);
DENG_EXTERN_C void R_InitSvgs(void);
DENG_EXTERN_C void R_ShutdownSvgs(void);
DENG_EXTERN_C struct font_s* R_CreateFontFromDef(ded_compositefont_t* def);

DENG_EXTERN_C void Rend_CacheForMobjType(int num);
DENG_EXTERN_C void Rend_ConsoleInit();
DENG_EXTERN_C void Rend_ConsoleResize(int force);
DENG_EXTERN_C void Rend_ConsoleOpen(int yes);
DENG_EXTERN_C void Rend_ConsoleToggleFullscreen();
DENG_EXTERN_C void Rend_ConsoleMove(int y);
DENG_EXTERN_C void Rend_ConsoleCursorResetBlink();

DENG_EXTERN_C void Cl_InitPlayers(void);

DENG_EXTERN_C void UI_Ticker(timespan_t t);
DENG_EXTERN_C void UI_Shutdown();

#endif // SERVER_DUMMIES_H
