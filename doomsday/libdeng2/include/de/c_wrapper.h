/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_C_WRAPPER_H
#define LIBDENG2_C_WRAPPER_H

#include "libdeng2.h"

/**
 * @file Defines the C wrapper API for libdeng2 classes. Legacy code can use
 * this wrapper API to access libdeng2 functionality.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * LegacyCore
 */
struct LegacyCore_s;
typedef LegacyCore_s LegacyCore;

LegacyCore* LIBDENG2_PUBLIC LegacyCore_New(int* argc, char** argv);
void        LIBDENG2_PUBLIC LegacyCore_Delete(LegacyCore* lc);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG2_C_WRAPPER_H
