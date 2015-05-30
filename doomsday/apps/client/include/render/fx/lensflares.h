/** @file lensflares.h
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_FX_LENSFLARES_H
#define DENG_CLIENT_FX_LENSFLARES_H

#include "render/consoleeffect.h"
#include "render/ilightsource.h"

namespace fx {

/**
 * Draws lens flares for all visible light sources in the current frame.
 */
class LensFlares : public ConsoleEffect
{
public:
    LensFlares(int console);

    void clearLights();
    void markLightPotentiallyVisibleForCurrentFrame(IPointLightSource const *lightSource);

    void glInit();
    void glDeinit();

    void beginFrame();
    void draw();

    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

} // namespace fx

#endif // DENG_CLIENT_FX_LENSFLARES_H
