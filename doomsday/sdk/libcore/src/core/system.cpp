/** @file system.cpp  Base class for application subsystems.
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

#include "de/System"

namespace de {

DENG2_PIMPL(System)
{
    Flags behavior;

    Instance(Public *i) : Base(*i)
    {}
};

System::System(Flags const &behavior) : d(new Instance(this))
{
    d->behavior = behavior;
}

void System::setBehavior(Flags const &behavior, FlagOp operation)
{
    applyFlagOperation(d->behavior, behavior, operation);
}

System::Flags System::behavior() const
{
    return d->behavior;
}

bool System::processEvent(Event const &)
{
    return false;
}

void System::timeChanged(Clock const &)
{}

} // namespace de
