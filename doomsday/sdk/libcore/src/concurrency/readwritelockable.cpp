/** @file readwritelockable.cpp  Read-write lock.
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

#include "de/ReadWriteLockable"

#include <QReadWriteLock>

namespace de {

DENG2_PIMPL_NOREF(ReadWriteLockable)
{
    QReadWriteLock lock;

    Instance() : lock(QReadWriteLock::Recursive)
    {}

    ~Instance()
    {
        // Make sure ongoing access has ended.
        lock.lockForWrite();
        lock.unlock();
    }
};

ReadWriteLockable::ReadWriteLockable() : d(new Instance)
{}

ReadWriteLockable::~ReadWriteLockable()
{}

void ReadWriteLockable::lockForRead() const
{
    d->lock.lockForRead();
}

void ReadWriteLockable::lockForWrite() const
{
    d->lock.lockForWrite();
}

void ReadWriteLockable::unlock() const
{
    d->lock.unlock();
}

} // namespace de
