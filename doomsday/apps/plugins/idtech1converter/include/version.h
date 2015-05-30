/** @file version.h  Version information for the IdTech1Converter plugin.
 *
 * @ingroup idtech1converter
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#ifndef IDTECH1CONVERTER_VERSION_H
#define IDTECH1CONVERTER_VERSION_H

#ifndef IDTECH1CONVERTER_VER_ID
#  ifdef _DEBUG
#    define IDTECH1CONVERTER_VER_ID "+D Doomsday"
#  else
#    define IDTECH1CONVERTER_VER_ID "Doomsday"
#  endif
#endif

// Used to derive filepaths.
#define PLUGIN_NAME         "dpidtech1converter"

// Presented to the user in dialogs, messages etc.
#define PLUGIN_NICENAME     "Id Tech 1 Converter"
#define PLUGIN_DETAILS      "Doomsday plugin for loading id Tech 1 resources."

#define PLUGIN_VERSION_TEXT "1.0.0"
#define PLUGIN_VERSION_TEXTLONG "Version " PLUGIN_VERSION_TEXT " " __DATE__ " (" IDTECH1CONVERTER_VER_ID ")"
#define PLUGIN_VERSION_NUMBER 1,0,0,0 // For WIN32 version info.

#endif // IDTECH1CONVERTER_VERSION_H
