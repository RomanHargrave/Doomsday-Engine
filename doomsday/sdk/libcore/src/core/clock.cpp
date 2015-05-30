/** @file clock.cpp Time source.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Clock"

namespace de {

DENG2_PIMPL_NOREF(Clock)
{
    Time startedAt;
    Time time;
    duint32 tickCount { 0 };

    DENG2_PIMPL_AUDIENCE(TimeChange)
};

DENG2_AUDIENCE_METHOD(Clock, TimeChange)

Clock *Clock::_appClock = 0;

Clock::Clock() : d(new Instance)
{}

Clock::~Clock()
{}

void Clock::setTime(Time const &currentTime)
{
    bool changed = (d->time != currentTime);

    d->time = currentTime;

    if(changed)
    {
        d->tickCount++;

        DENG2_FOR_EACH_OBSERVER(PriorityTimeChangeAudience, i, audienceForPriorityTimeChange)
        {
            i->timeChanged(*this);
        }
        DENG2_FOR_AUDIENCE2(TimeChange, i) i->timeChanged(*this);
    }
}

void Clock::advanceTime(TimeDelta const &span)
{
    setTime(d->time + span);
}

TimeDelta Clock::elapsed() const
{
    return d->time - d->startedAt;
}

Time const &Clock::time() const
{
    return d->time;
}

duint32 Clock::tickCount() const
{
    return d->tickCount;
}

void Clock::setAppClock(Clock *c)
{
    _appClock = c;
}

Clock &Clock::get()
{
    DENG2_ASSERT(_appClock != 0);
    return *_appClock;
}

Time const &Clock::appTime()
{
    return get().time();
}

} // namespace de
