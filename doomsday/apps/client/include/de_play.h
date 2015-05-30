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
 * de_play.h: Game World Events (Playsim)
 */

#ifndef __DOOMSDAY_PLAYSIM__
#define __DOOMSDAY_PLAYSIM__

#include "api_thinker.h"
#include "BspNode"
#ifdef __CLIENT__
#  include "Contact"
#endif
#include "Generator"
#include "Line"
#include "Plane"
#include "Polyobj"
#include "Sector"
#include "Surface"
#include "Vertex"
#include "world/dmuargs.h"
#include "world/linesighttest.h"
#include "world/p_object.h"
#include "world/p_ticker.h"
#include "world/p_players.h"
#include "world/thinkers.h"
#include "Material"
#include "r_util.h"

#include "api_map.h"

#endif
