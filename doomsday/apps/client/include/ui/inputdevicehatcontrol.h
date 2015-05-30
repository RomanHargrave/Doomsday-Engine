/** @file inputdevicehatcontrol.h  Hat control for a logical input device.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef CLIENT_INPUTSYSTEM_INPUTDEVICEHATCONTROL_H
#define CLIENT_INPUTSYSTEM_INPUTDEVICEHATCONTROL_H

#include "inputdevice.h"

/**
 * Models a hat control on a "physical" input device (such as that found on joysticks).
 *
 * @ingroup ui
 */
class InputDeviceHatControl : public InputDeviceControl
{
public:
    explicit InputDeviceHatControl(de::String const &name = "");
    virtual ~InputDeviceHatControl();

    /**
     * Returns the current position of the hat.
     */
    de::dint position() const;

    /**
     * @param newPosition  @c -1= centered.
     */
    void setPosition(de::dint newPosition);

    /**
     * When the state of the control last changed, in milliseconds since app init.
     */
    de::duint time() const;

    de::String description() const;
    bool inDefaultState() const;

private:
    de::dint _pos   = -1;  ///< Current position. @c -1= centered.
    de::duint _time = 0;   ///< Timestamp of the latest change.
};

#endif // CLIENT_INPUTSYSTEM_INPUTDEVICEHATCONTROL_H
