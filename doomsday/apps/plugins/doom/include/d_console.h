/** @file d_console.h  Doom specific console settings and commands.
 * @ingroup libdoom
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOM_DCONSOLE_H
#define LIBDOOM_DCONSOLE_H

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Add the console variables and commands.
 */
void G_ConsoleRegistration(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDOOM_DCONSOLE_H
