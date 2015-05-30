/** @file
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
 * Console Subsystem.
 */

#ifndef LIBDENG_CONSOLE
#define LIBDENG_CONSOLE

#ifdef WIN32
#  include "de_platform.h"
#endif

#include <doomsday/console/exec.h>
#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>
#include <doomsday/console/knownword.h>
#include "con_config.h"

#ifdef __CLIENT__
#  include "ui/progress.h"
#  include "ui/b_main.h"
#  include "BindContext"
#endif

#include "api_console.h"

#endif /* LIBDENG_CONSOLE */
