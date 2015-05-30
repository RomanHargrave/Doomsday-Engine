/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Waitable"
#include "de/Time"

using namespace de;

Waitable::Waitable(duint initialValue) : _semaphore(initialValue)
{}
    
Waitable::~Waitable()
{}

void Waitable::reset()
{
    _semaphore.tryAcquire(_semaphore.available());
}

void Waitable::wait() const
{
    wait(0.0);
}

void Waitable::wait(TimeDelta const &timeOut) const
{
    if(timeOut <= 0.0)
    {
        _semaphore.acquire();
    }
    else
    {
        // Wait until the resource becomes available.
        if(!_semaphore.tryAcquire(1, int(timeOut.asMilliSeconds())))
        {
            /// @throw WaitError Failed to secure the resource due to an error.
            throw WaitError("Waitable::wait", "Timed out");
        }
    }
}

void Waitable::post() const
{
    _semaphore.release();
}
