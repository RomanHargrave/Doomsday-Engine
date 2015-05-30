/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_GUARD_H
#define LIBDENG2_GUARD_H

#include "../libcore.h"

namespace de {

/**
 * Locks the variable @a varName until the end of the current scope.
 *
 * @param varName  Name of the variable to guard. Must be just a single
 *                 identifier with no operators or anything else.
 */
#define DENG2_GUARD(varName) \
    de::Guard _guarding_##varName(varName); \
    DENG2_UNUSED(_guarding_##varName);

#define DENG2_GUARD_READ(varName) \
    de::Guard _guarding_##varName(varName, de::Guard::Reading); \
    DENG2_UNUSED(_guarding_##varName);

#define DENG2_GUARD_WRITE(varName) \
    de::Guard _guarding_##varName(varName, de::Guard::Writing); \
    DENG2_UNUSED(_guarding_##varName);

/**
 * Locks the target @a targetName until the end of the current scope.
 *
 * @param targetName  Target to be guarded.
 * @param varName     Name of the variable to guard. Must be just a single
 *                    identifier with no operators or anything else.
 */
#define DENG2_GUARD_FOR(targetName, varName) \
    de::Guard varName(targetName); \
    DENG2_UNUSED(varName);

#define DENG2_GUARD_READ_FOR(targetName, varName) \
    de::Guard varName(targetName, de::Guard::Reading); \
    DENG2_UNUSED(varName);

#define DENG2_GUARD_WRITE_FOR(targetName, varName) \
    de::Guard varName(targetName, de::Guard::Writing); \
    DENG2_UNUSED(varName);

class Lockable;
class ReadWriteLockable;

/**
 * Utility for locking a Lockable or ReadWriteLockable object for the lifetime
 * of the Guard. Note that using this is preferable to manual locking and
 * unlocking: if an exception occurs while the target is locked, unlocking will
 * be taken care of automatically when the Guard goes out of scope.
 *
 * @ingroup data
 */
class DENG2_PUBLIC Guard
{
public:
    enum LockMode
    {
        Reading,
        Writing
    };

public:
    /**
     * The target object is locked.
     */
    Guard(Lockable const &target);

    /**
     * The target object is locked.
     */
    Guard(Lockable const *target);

    Guard(ReadWriteLockable const &target, LockMode mode);
    Guard(ReadWriteLockable const *target, LockMode mode);

    /**
     * The target object is unlocked.
     */
    ~Guard();

private:
    Lockable const *_target;
    ReadWriteLockable const *_rwTarget;
};

} // namespace de

#endif // LIBDENG2_GUARD_H
