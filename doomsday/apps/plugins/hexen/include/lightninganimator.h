/** @file lightninganimator.h  Animator for map-wide lightning effects.
 *
 * @authors Copyright © 2004-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
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

#ifndef LIBHEXEN_PLAY_LIGHTNINGANIMATOR_H
#define LIBHEXEN_PLAY_LIGHTNINGANIMATOR_H

#include "jhexen.h"

/**
 * Animator for the map-wide lightning effects.
 */
class LightningAnimator
{
public:
    LightningAnimator();

    /**
     * Determines whether lightning is enabled for the current map.
     */
    bool enabled() const;

    /**
     * Manually trigger a lightning flash which will begin animating on the next game tic.
     * Can be used to trigger a flash at specific times.
     */
    void triggerFlash();

    /**
     * Animate lightning for the current map. To be called once per GAMETIC.
     */
    void advanceTime();

    /**
     * Initialize the lightning animator for the current map.
     *
     * @return  @c true, if lightning is enabled for the current map, for caller convenience.
     */
    bool initForMap();

private:
    DENG2_PRIVATE(d)
};

#endif // LIBHEXEN_PLAY_LIGHTNINGANIMATOR_H
