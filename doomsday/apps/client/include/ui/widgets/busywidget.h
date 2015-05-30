/** @file busywidget.h
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

#ifndef CLIENT_BUSYWIDGET_H
#define CLIENT_BUSYWIDGET_H

#include <de/ProgressWidget>
#include <de/GLTexture>

/**
 * Widget that takes care of the UI while busy mode is active.
 */
class BusyWidget : public de::GuiWidget
{
public:
    BusyWidget(de::String const &name = "");

    de::ProgressWidget &progress();

    void renderTransitionFrame();
    void releaseTransitionFrame();
    de::GLTexture const *transitionFrame() const;

    // Events.
    void viewResized();
    void update();
    void drawContent();
    bool handleEvent(de::Event const &event);

protected:
    void glInit();
    void glDeinit();

private:
    DENG2_PRIVATE(d)
};

#endif // CLIENT_BUSYWIDGET_H
