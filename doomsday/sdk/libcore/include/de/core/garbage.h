/** @file garbage.h  Garbage collector. @ingroup core
 *
 * Stores pointers to unnecessary areas of memory and frees them later. Garbage
 * collection must be requested manually, e.g., at the end of the frame once
 * per second. Garbage is also thread-specific; recycling must be done
 * separately in each thread.
 *
 * @authors Copyright © 2012-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_GARBAGE_COLLECTOR_H
#define LIBDENG2_GARBAGE_COLLECTOR_H

#include "../libcore.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Puts a region of allocated memory up for garbage collection in the current
 * thread. The memory will be available for use until Garbage_Recycle() is
 * called.
 *
 * @param ptr  Pointer to memory allocated with malloc().
 */
DENG2_PUBLIC void Garbage_TrashMalloc(void *ptr);

/**
 * Pointer to an instance destructor;
 */
typedef void (*GarbageDestructor)(void *);

/**
 * Puts an object up for garbage collection in the current thread. The object
 * will be available for use until Garbage_Recycle() is called in the thread.
 *
 * @param ptr  Pointer to the object.
 * @param destructor  Function that destroys the object.
 */
DENG2_PUBLIC void Garbage_TrashInstance(void *ptr, GarbageDestructor destructor);

/**
 * Determines whether a memory pointer has been trashed. Only the current
 * thread's collector is searched.
 *
 * @param ptr  Pointer to memory.
 *
 * @return @c true if the pointer is in the trash.
 */
DENG2_PUBLIC int Garbage_IsTrashed(void const *ptr);

/**
 * Removes a region from the current thread's collector, if it is still there.
 * @warning Do not call this if there is a chance that the pointer has already
 * been freed.
 *
 * @param ptr  Pointer to memory previously put in the trash.
 */
DENG2_PUBLIC void Garbage_Untrash(void *ptr);

/**
 * Removes a pointer from the garbage. This needs to be called if the
 * previously trashed memory was manually freed.
 *
 * @param ptr  Pointer to memory.
 */
DENG2_PUBLIC void Garbage_RemoveIfTrashed(void *ptr);

/**
 * Frees all pointers given over to the current thread's garbage collector.
 * Every thread that uses garbage collection must call this periodically
 * or the trashed memory will not be freed.
 */
DENG2_PUBLIC void Garbage_Recycle(void);

/**
 * Frees all pointers in every thread's garbage if they are using a specific
 * destructor function.
 *
 * @param destructor  Destructor function.
 */
DENG2_PUBLIC void Garbage_RecycleAllWithDestructor(GarbageDestructor destructor);

/**
 * Recycles all garbage of the current thread and deletes the thread's garbage
 * collector. Should be called right before the thread ends.
 *
 * This differs from Garbage_Recycle() in that all the thread-specific internal
 * resources are freed, not just the thread's trashed memory. The thread can
 * safely end after this.
 */
DENG2_PUBLIC void Garbage_ClearForThread(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG2_GARBAGE_COLLECTOR_H
