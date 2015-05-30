/** @file main.cpp Application startup and shutdown.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/libcore.h>
#include <de/Counted>
#include "shellapp.h"

int main(int argc, char *argv[])
{
    int result;
    {
        ShellApp a(argc, argv);
        result = a.exec();
    }
#ifdef DENG2_DEBUG
    DENG2_ASSERT(de::Counted::totalCount == 0);
#endif
    return result;
}
