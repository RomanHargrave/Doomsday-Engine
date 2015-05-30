/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * Lookup tables.
 */

#ifndef __TABLES__
#define __TABLES__

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

#include "doomsday.h"

#define FINEANGLES      8192
#define FINEMASK        (FINEANGLES-1)

// 0x100000000 to 0x2000
#define ANGLETOFINESHIFT    19

// Binary Angle Measument, BAM.
#define ANG45       0x20000000
#define ANG90       0x40000000
#define ANG180      0x80000000
#define ANG270      0xc0000000

#define SLOPERANGE      2048
#define SLOPEBITS       11
#define DBITS           (FRACBITS-SLOPEBITS)

// Effective size is 10240.
DENG_EXTERN_C fixed_t finesine[5 * FINEANGLES / 4];

// Re-use data, is just PI/2 pahse shift.
DENG_EXTERN_C fixed_t *finecosine;

// Effective size is 4096.
DENG_EXTERN_C fixed_t finetangent[FINEANGLES / 2];

// Effective size is 2049;
// The +1 size is to handle the case when x==y
//  without additional checking.
DENG_EXTERN_C angle_t tantoangle[SLOPERANGE + 1];

#ifdef __cplusplus
extern "C" {
#endif

// Utility function, called by R_PointToAngle.
int SlopeDiv(unsigned num, unsigned den);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
