/** @file mouseeventsource.h  Object that produces mouse events.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_MOUSEEVENTSOURCE_H
#define LIBGUI_MOUSEEVENTSOURCE_H

#include "../gui/libgui.h"
#include "mouseevent.h"
#include <de/Vector>
#include <de/Observers>

namespace de {

/**
 * Object that produces mouse events.
 */
class LIBGUI_PUBLIC MouseEventSource
{
public:
    enum State
    {
        Untrapped,
        Trapped
    };

    DENG2_DEFINE_AUDIENCE2(MouseStateChange, void mouseStateChanged(State))
    DENG2_DEFINE_AUDIENCE2(MouseEvent,       void mouseEvent(MouseEvent const &))

public:
    MouseEventSource();
    virtual ~MouseEventSource() {}

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_MOUSEEVENTSOURCE_H
