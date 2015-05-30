/** @file dd_uinit.cpp  Engine Initialization (Unix).
 *
 * @authors Copyright © 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <locale.h>

#include <doomsday/paths.h>
#include <de/c_wrapper.h>
#include <de/App>

#ifdef UNIX
#  include "library.h"
#endif

#include "de_base.h"
#include "de_graphics.h"
#include "de_console.h"
#include "de_system.h"
#include "de_play.h"
#include "de_network.h"
#include "de_misc.h"

#include <doomsday/filesys/fs_util.h>
#include <doomsday/filesys/sys_direc.h>
#include "dd_uinit.h"

#include <de/App>

#ifdef __CLIENT__
#  include <de/DisplayMode>
#endif

application_t app;

#ifdef __CLIENT__
static int initDGL(void)
{
    return (int) Sys_GLPreInit();
}
#endif

static void determineGlobalPaths(application_t* app)
{
    assert(app);

    // By default, make sure the working path is the home folder.
    de::App::setCurrentWorkPath(de::App::app().nativeHomePath());

#ifndef MACOSX
    if(getenv("HOME"))
    {
        filename_t homePath;
        directory_t* temp;
        dd_snprintf(homePath, FILENAME_T_MAXLEN, "%s/%s/runtime/", getenv("HOME"),
                    DENG2_APP->unixHomeFolderName().toLatin1().constData());
        temp = Dir_New(homePath);
        Dir_mkpath(Dir_Path(temp));
        app->usingHomeDir = Dir_SetCurrent(Dir_Path(temp));
        if(app->usingHomeDir)
        {
            DD_SetRuntimePath(Dir_Path(temp));
        }
        Dir_Delete(temp);
    }
#endif

    // The -userdir option sets the working directory.
    if(CommandLine_CheckWith("-userdir", 1))
    {
        filename_t runtimePath;
        directory_t* temp;

        strncpy(runtimePath, CommandLine_NextAsPath(), FILENAME_T_MAXLEN);
        Dir_CleanPath(runtimePath, FILENAME_T_MAXLEN);
        // Ensure the path is closed with a directory separator.
        F_AppendMissingSlashCString(runtimePath, FILENAME_T_MAXLEN);

        temp = Dir_New(runtimePath);
        app->usingUserDir = Dir_SetCurrent(Dir_Path(temp));
        if(app->usingUserDir)
        {
            DD_SetRuntimePath(Dir_Path(temp));
#ifndef MACOSX
            app->usingHomeDir = false;
#endif
        }
        Dir_Delete(temp);
    }

#ifndef MACOSX
    if(!app->usingHomeDir && !app->usingUserDir)
#else
    if(!app->usingUserDir)
#endif
    {
        // The current working directory is the runtime dir.
        directory_t* temp = Dir_NewFromCWD();
        DD_SetRuntimePath(Dir_Path(temp));
        Dir_Delete(temp);
    }

    // libcore has determined the native base path, so let FS1 know about it.
    DD_SetBasePath(DENG2_APP->nativeBasePath().toUtf8());
}

dd_bool DD_Unix_Init(void)
{
    dd_bool failed = true;

    memset(&app, 0, sizeof(app));

    // We wish to use U.S. English formatting for time and numbers.
    setlocale(LC_ALL, "en_US.UTF-8");

    DD_InitCommandLine();

    Library_Init();

    // Determine our basedir and other global paths.
    determineGlobalPaths(&app);

    if(!DD_EarlyInit())
    {
        Sys_MessageBox(MBT_ERROR, DOOMSDAY_NICENAME, "Error during early init.", 0);
    }
#ifdef __CLIENT__
    else if(!initDGL())
    {
        Sys_MessageBox(MBT_ERROR, DOOMSDAY_NICENAME, "Error initializing DGL.", 0);
    }
#endif
    else
    {
        // Everything okay so far.
        failed = false;
    }

    return !failed;
}

/**
 * Shuts down the engine.
 */
void DD_Shutdown(void)
{
    // Shutdown all subsystems.
    DD_ShutdownAll();

    Plug_UnloadAll();
    Library_Shutdown();
#ifdef __CLIENT__
    DisplayMode_Shutdown();
#endif
}
