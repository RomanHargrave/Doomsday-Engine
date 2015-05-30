/** @file
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_SYSTEM_H
#define LIBDENG_SYSTEM_H

#include <de/concurrency.h>
#include <de/timer.h>

#include "sys_system.h"
#include "network/masterserver.h"

#ifdef __CLIENT__
#  include "network/sys_network.h"
#  include "gl/sys_opengl.h"
#  include "ui/sys_input.h"
#  include "ui/clientwindow.h"
#endif

#ifdef __SERVER__
#  include "serversystem.h"
#endif

#endif /* LIBDENG_SYSTEM_H */
