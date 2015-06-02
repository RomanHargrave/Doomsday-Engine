/** @file dd_main.cpp  Engine core.
 *
 * @todo Much of this should be refactored and merged into the App classes.
 * @todo The rest should be split into smaller, perhaps domain-specific files.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#define DENG_NO_API_MACROS_BASE // functions defined here

#include "de_platform.h"
#include "dd_main.h"

#ifdef WIN32
#  define _WIN32_DCOM
#  include <objbase.h>
#endif
#include <cstring>
#ifdef UNIX
#  include <ctype.h>
#endif

#include <QStringList>
#include <de/charsymbols.h>
#include <de/concurrency.h>
#include <de/findfile.h>
#include <de/memoryzone.h>
#include <de/memory.h>
#include <de/timer.h>
#include <de/ArrayValue>
#include <de/DictionaryValue>
#include <de/game/Session>
#include <de/Log>
#include <de/NativePath>
#ifdef __CLIENT__
#  include <de/DisplayMode>
#endif
#include <doomsday/audio/logical.h>
#include <doomsday/console/alias.h>
#include <doomsday/console/cmd.h>
#include <doomsday/console/exec.h>
#include <doomsday/console/var.h>
#include <doomsday/filesys/fs_main.h>
#include <doomsday/filesys/fs_util.h>
#include <doomsday/filesys/sys_direc.h>
#include <doomsday/help.h>
#include <doomsday/paths.h>

#ifdef __CLIENT__
#  include "clientapp.h"
#endif
#ifdef __SERVER__
#  include "serverapp.h"
#endif
#include "dd_loop.h"
#include "busymode.h"
#include "con_config.h"
#include "library.h"
#include "sys_system.h"
#include "edit_bias.h"
#include "gl/svg.h"

#include "audio/s_main.h"

#include "resource/manifest.h"

#include "world/worldsystem.h"
#include "world/entitydef.h"
#include "world/map.h"
#include "world/p_players.h"

#include "ui/infine/infinesystem.h"
#include "ui/nativeui.h"
#include "ui/progress.h"

#ifdef __CLIENT__
#  include "client/cl_def.h"
#  include "client/cl_infine.h"

#  include "gl/gl_main.h"
#  include "gl/gl_defer.h"
#  include "gl/gl_texmanager.h"

#  include "network/net_demo.h"

#  include "render/rend_main.h"
#  include "render/cameralensfx.h"
#  include "render/r_draw.h" // R_InitViewWindow
#  include "render/r_main.h" // pspOffset
#  include "render/rend_font.h"
#  include "render/rend_particle.h" // Rend_ParticleLoadSystemTextures
#  include "render/vr.h"

#  include "Contact"
#  include "MaterialAnimator"
#  include "Sector"

#  include "ui/ui_main.h"
#  include "ui/busyvisual.h"
#  include "ui/sys_input.h"
#  include "ui/widgets/taskbarwidget.h"

#  include "updater.h"
#  include "updater/downloaddialog.h"
#endif
#ifdef __SERVER__
#  include "network/net_main.h"

#  include "server/sv_def.h"
#endif

using namespace de;

class ZipFileType : public de::NativeFileType
{
public:
    ZipFileType() : NativeFileType("FT_ZIP", RC_PACKAGE)
    {
        addKnownExtension(".pk3");
        addKnownExtension(".zip");
    }

    File1 *interpret(FileHandle &hndl, String path, FileInfo const &info) const
    {
        if(Zip::recognise(hndl))
        {
            LOG_AS("ZipFileType");
            LOG_RES_VERBOSE("Interpreted \"" + NativePath(path).pretty() + "\"");
            return new Zip(hndl, path, info);
        }
        return nullptr;
    }
};

class WadFileType : public de::NativeFileType
{
public:
    WadFileType() : NativeFileType("FT_WAD", RC_PACKAGE)
    {
        addKnownExtension(".wad");
    }

    File1 *interpret(FileHandle &hndl, String path, FileInfo const &info) const
    {
        if(Wad::recognise(hndl))
        {
            LOG_AS("WadFileType");
            LOG_RES_VERBOSE("Interpreted \"" + NativePath(path).pretty() + "\"");
            return new Wad(hndl, path, info);
        }
        return nullptr;
    }
};

static void consoleRegister();
static void initPathMappings();
static dint DD_StartupWorker(void *context);
static dint DD_DummyWorker(void *context);
static void DD_AutoLoad();

#ifndef WIN32
extern GETGAMEAPI GetGameAPI;
#endif

dint isDedicated;
dint verbose;                      ///< For debug messages (-verbose).
dint gameDataFormat;               ///< Game-specific data format identifier/selector.
#ifdef __CLIENT__
dint symbolicEchoMode = false;     ///< @note Mutable via public API.
#endif

static char *startupFiles = (char *) "";  ///< List of file names, whitespace seperating (written to .cfg).

static void registerResourceFileTypes()
{
    FileType *ftype;

    //
    // Packages types:
    //
    ResourceClass &packageClass = App_ResourceClass("RC_PACKAGE");

    ftype = new ZipFileType();
    packageClass.addFileType(ftype);
    DD_AddFileType(*ftype);

    ftype = new WadFileType();
    packageClass.addFileType(ftype);
    DD_AddFileType(*ftype);

    ftype = new FileType("FT_LMP", RC_PACKAGE);  ///< Treat lumps as packages so they are mapped to $App.DataPath.
    ftype->addKnownExtension(".lmp");
    DD_AddFileType(*ftype);
    /// @todo ftype leaks. -jk

    //
    // Definition fileTypes:
    //
    ftype = new FileType("FT_DED", RC_DEFINITION);
    ftype->addKnownExtension(".ded");
    App_ResourceClass("RC_DEFINITION").addFileType(ftype);
    DD_AddFileType(*ftype);

    //
    // Graphic fileTypes:
    //
    ResourceClass &graphicClass = App_ResourceClass("RC_GRAPHIC");

    ftype = new FileType("FT_PNG", RC_GRAPHIC);
    ftype->addKnownExtension(".png");
    graphicClass.addFileType(ftype);
    DD_AddFileType(*ftype);

    ftype = new FileType("FT_TGA", RC_GRAPHIC);
    ftype->addKnownExtension(".tga");
    graphicClass.addFileType(ftype);
    DD_AddFileType(*ftype);

    ftype = new FileType("FT_JPG", RC_GRAPHIC);
    ftype->addKnownExtension(".jpg");
    graphicClass.addFileType(ftype);
    DD_AddFileType(*ftype);

    ftype = new FileType("FT_PCX", RC_GRAPHIC);
    ftype->addKnownExtension(".pcx");
    graphicClass.addFileType(ftype);
    DD_AddFileType(*ftype);

    //
    // Model fileTypes:
    //
    ResourceClass &modelClass = App_ResourceClass("RC_MODEL");

    ftype = new FileType("FT_DMD", RC_MODEL);
    ftype->addKnownExtension(".dmd");
    modelClass.addFileType(ftype);
    DD_AddFileType(*ftype);

    ftype = new FileType("FT_MD2", RC_MODEL);
    ftype->addKnownExtension(".md2");
    modelClass.addFileType(ftype);
    DD_AddFileType(*ftype);

    //
    // Sound fileTypes:
    //
    ftype = new FileType("FT_WAV", RC_SOUND);
    ftype->addKnownExtension(".wav");
    App_ResourceClass("RC_SOUND").addFileType(ftype);
    DD_AddFileType(*ftype);

    //
    // Music fileTypes:
    //
    ResourceClass &musicClass = App_ResourceClass("RC_MUSIC");

    ftype = new FileType("FT_OGG", RC_MUSIC);
    ftype->addKnownExtension(".ogg");
    musicClass.addFileType(ftype);
    DD_AddFileType(*ftype);

    ftype = new FileType("FT_MP3", RC_MUSIC);
    ftype->addKnownExtension(".mp3");
    musicClass.addFileType(ftype);
    DD_AddFileType(*ftype);

    ftype = new FileType("FT_MOD", RC_MUSIC);
    ftype->addKnownExtension(".mod");
    musicClass.addFileType(ftype);
    DD_AddFileType(*ftype);

    ftype = new FileType("FT_MID", RC_MUSIC);
    ftype->addKnownExtension(".mid");
    musicClass.addFileType(ftype);
    DD_AddFileType(*ftype);

    //
    // Font fileTypes:
    //
    ftype = new FileType("FT_DFN", RC_FONT);
    ftype->addKnownExtension(".dfn");
    App_ResourceClass("RC_FONT").addFileType(ftype);
    DD_AddFileType(*ftype);

    //
    // Misc fileTypes:
    //
    ftype = new FileType("FT_DEH", RC_PACKAGE);  ///< Treat DeHackEd patches as packages so they are mapped to $App.DataPath.
    ftype->addKnownExtension(".deh");
    DD_AddFileType(*ftype);
    /// @todo ftype leaks. -jk
}

static void createPackagesScheme()
{
    FS1::Scheme &scheme = App_FileSystem().createScheme("Packages");

    //
    // Add default search paths.
    //
    // Note that the order here defines the order in which these paths are searched
    // thus paths must be added in priority order (newer paths have priority).
    //

#ifdef UNIX
    // There may be an iwaddir specified in a system-level config file.
    filename_t fn;
    if(UnixInfo_GetConfigValue("paths", "iwaddir", fn, FILENAME_T_MAXLEN))
    {
        NativePath path = de::App::commandLine().startupPath() / fn;
        scheme.addSearchPath(SearchPath(de::Uri::fromNativeDirPath(path), SearchPath::NoDescend));
        LOG_RES_NOTE("Using paths.iwaddir: %s") << path.pretty();
    }
#endif

    // Add paths to games bought with/using Steam.
    if(!CommandLine_Check("-nosteamapps"))
    {
        NativePath steamBase = Sys_SteamBasePath();
        if(!steamBase.isEmpty())
        {
            NativePath steamPath = steamBase / "SteamApps/common/";
            LOG_RES_NOTE("Using SteamApps path: %s") << steamPath.pretty();

            static String const appDirs[] =
            {
                "doom 2/base",
                "final doom/base",
                "heretic shadow of the serpent riders/base",
                "hexen/base",
                "hexen deathkings of the dark citadel/base",
                "ultimate doom/base",
                "DOOM 3 BFG Edition/base/wads",
                ""
            };
            for(dint i = 0; !appDirs[i].isEmpty(); ++i)
            {
                scheme.addSearchPath(SearchPath(de::Uri::fromNativeDirPath(steamPath / appDirs[i]),
                                                SearchPath::NoDescend));
            }
        }
    }

    // Add the path from the DOOMWADDIR environment variable.
    if(!CommandLine_Check("-nodoomwaddir") && getenv("DOOMWADDIR"))
    {
        NativePath path = App::commandLine().startupPath() / getenv("DOOMWADDIR");
        scheme.addSearchPath(SearchPath(de::Uri::fromNativeDirPath(path), SearchPath::NoDescend));
        LOG_RES_NOTE("Using DOOMWADDIR: %s") << path.pretty();
    }

    // Add any paths from the DOOMWADPATH environment variable.
    if(!CommandLine_Check("-nodoomwadpath") && getenv("DOOMWADPATH"))
    {
#if WIN32
#  define SEP_CHAR      ';'
#else
#  define SEP_CHAR      ':'
#endif

        QStringList allPaths = String(getenv("DOOMWADPATH")).split(SEP_CHAR, String::SkipEmptyParts);
        for(dint i = allPaths.count(); i--> 0; )
        {
            NativePath path = App::commandLine().startupPath() / allPaths[i];
            scheme.addSearchPath(SearchPath(de::Uri::fromNativeDirPath(path), SearchPath::NoDescend));
            LOG_RES_NOTE("Using DOOMWADPATH: %s") << path.pretty();
        }

#undef SEP_CHAR
    }

    scheme.addSearchPath(SearchPath(de::Uri("$(App.DataPath)/", RC_NULL), SearchPath::NoDescend));
    scheme.addSearchPath(SearchPath(de::Uri("$(App.DataPath)/$(GamePlugin.Name)/", RC_NULL), SearchPath::NoDescend));
}

void DD_CreateFileSystemSchemes()
{
    dint const schemedef_max_searchpaths = 5;
    struct schemedef_s {
        char const *name;
        char const *optOverridePath;
        char const *optFallbackPath;
        FS1::Scheme::Flags flags;
        SearchPath::Flags searchPathFlags;
        /// Priority is right to left.
        char const *searchPaths[schemedef_max_searchpaths];
    } defs[] = {
        { "Defs",         nullptr,           nullptr,     FS1::Scheme::Flag(0), 0,
            { "$(App.DefsPath)/", "$(App.DefsPath)/$(GamePlugin.Name)/", "$(App.DefsPath)/$(GamePlugin.Name)/$(Game.IdentityKey)/" }
        },
        { "Graphics",     "-gfxdir2",     "-gfxdir",      FS1::Scheme::Flag(0), 0,
            { "$(App.DataPath)/graphics/" }
        },
        { "Models",       "-modeldir2",   "-modeldir",    FS1::Scheme::MappedInPackages, 0,
            { "$(App.DataPath)/$(GamePlugin.Name)/models/", "$(App.DataPath)/$(GamePlugin.Name)/models/$(Game.IdentityKey)/" }
        },
        { "Sfx",          "-sfxdir2",     "-sfxdir",      FS1::Scheme::MappedInPackages, SearchPath::NoDescend,
            { "$(App.DataPath)/$(GamePlugin.Name)/sfx/", "$(App.DataPath)/$(GamePlugin.Name)/sfx/$(Game.IdentityKey)/" }
        },
        { "Music",        "-musdir2",     "-musdir",      FS1::Scheme::MappedInPackages, SearchPath::NoDescend,
            { "$(App.DataPath)/$(GamePlugin.Name)/music/", "$(App.DataPath)/$(GamePlugin.Name)/music/$(Game.IdentityKey)/" }
        },
        { "Textures",     "-texdir2",     "-texdir",      FS1::Scheme::MappedInPackages, SearchPath::NoDescend,
            { "$(App.DataPath)/$(GamePlugin.Name)/textures/", "$(App.DataPath)/$(GamePlugin.Name)/textures/$(Game.IdentityKey)/" }
        },
        { "Flats",        "-flatdir2",    "-flatdir",     FS1::Scheme::MappedInPackages, SearchPath::NoDescend,
            { "$(App.DataPath)/$(GamePlugin.Name)/flats/", "$(App.DataPath)/$(GamePlugin.Name)/flats/$(Game.IdentityKey)/" }
        },
        { "Patches",      "-patdir2",     "-patdir",      FS1::Scheme::MappedInPackages, SearchPath::NoDescend,
            { "$(App.DataPath)/$(GamePlugin.Name)/patches/", "$(App.DataPath)/$(GamePlugin.Name)/patches/$(Game.IdentityKey)/" }
        },
        { "LightMaps",    "-lmdir2",      "-lmdir",       FS1::Scheme::MappedInPackages, 0,
            { "$(App.DataPath)/$(GamePlugin.Name)/lightmaps/" }
        },
        { "Fonts",        "-fontdir2",    "-fontdir",     FS1::Scheme::MappedInPackages, SearchPath::NoDescend,
            { "$(App.DataPath)/fonts/", "$(App.DataPath)/$(GamePlugin.Name)/fonts/", "$(App.DataPath)/$(GamePlugin.Name)/fonts/$(Game.IdentityKey)/" }
        }
    };

    createPackagesScheme();

    // Setup the rest...
    for(schemedef_s const &def : defs)
    {
        FS1::Scheme &scheme = App_FileSystem().createScheme(def.name, def.flags);

        dint searchPathCount = 0;
        while(def.searchPaths[searchPathCount] && ++searchPathCount < schemedef_max_searchpaths)
        {}

        for(dint i = 0; i < searchPathCount; ++i)
        {
            scheme.addSearchPath(SearchPath(de::Uri(def.searchPaths[i], RC_NULL), def.searchPathFlags));
        }

        if(def.optOverridePath && CommandLine_CheckWith(def.optOverridePath, 1))
        {
            NativePath path = NativePath(CommandLine_NextAsPath());
            scheme.addSearchPath(SearchPath(de::Uri::fromNativeDirPath(path), def.searchPathFlags), FS1::OverridePaths);
            path = path / "$(Game.IdentityKey)";
            scheme.addSearchPath(SearchPath(de::Uri::fromNativeDirPath(path), def.searchPathFlags), FS1::OverridePaths);
        }

        if(def.optFallbackPath && CommandLine_CheckWith(def.optFallbackPath, 1))
        {
            NativePath path = NativePath(CommandLine_NextAsPath());
            scheme.addSearchPath(SearchPath(de::Uri::fromNativeDirPath(path), def.searchPathFlags), FS1::FallbackPaths);
        }
    }
}

void App_Error(char const *error, ...)
{
    static bool errorInProgress = false;

    LogBuffer_Flush();

    char buff[2048], err[256];
    va_list argptr;

#ifdef __CLIENT__
    ClientWindow::main().canvas().trapMouse(false);
#endif

    // Already in an error?
    if(errorInProgress)
    {
#ifdef __CLIENT__
        DisplayMode_Shutdown();
#endif

        va_start(argptr, error);
        dd_vsnprintf(buff, sizeof(buff), error, argptr);
        va_end(argptr);

        if(!BusyMode_InWorkerThread())
        {
            Sys_MessageBox(MBT_ERROR, DOOMSDAY_NICENAME, buff, 0);
        }

        // Exit immediately, lest we go into an infinite loop.
        exit(1);
    }

    // We've experienced a fatal error; program will be shut down.
    errorInProgress = true;

    // Get back to the directory we started from.
    Dir_SetCurrent(DD_RuntimePath());

    va_start(argptr, error);
    dd_vsnprintf(err, sizeof(err), error, argptr);
    va_end(argptr);

    LOG_CRITICAL("") << err;
    LogBuffer_Flush();

    strcpy(buff, "");
    strcat(buff, "\n");
    strcat(buff, err);

    if(BusyMode_Active())
    {
        BusyMode_WorkerError(buff);
        if(BusyMode_InWorkerThread())
        {
            // We should not continue to execute the worker any more.
            forever Thread_Sleep(10000);
        }
    }
    else
    {
        App_AbnormalShutdown(buff);
    }
}

void App_AbnormalShutdown(char const *message)
{
    // This is a crash landing, better be safe than sorry.
    BusyMode_SetAllowed(false);

    Sys_Shutdown();

#ifdef __CLIENT__
    DisplayMode_Shutdown();
    DENG2_GUI_APP->loop().pause();

    // This is an abnormal shutdown, we cannot continue drawing any of the
    // windows. (Alternatively could hide/disable drawing of the windows.) Note
    // that the app's event loop is running normally while we show the native
    // message box below -- if the app windows are not hidden/closed, they might
    // receive draw events.
    ClientApp::windowSystem().closeAll();
#endif

    if(message) // Only show if a message given.
    {
        // Make sure all the buffered stuff goes into the file.
        LogBuffer_Flush();

        /// @todo Get the actual output filename (might be a custom one).
        Sys_MessageBoxWithDetailsFromFile(MBT_ERROR, DOOMSDAY_NICENAME, message,
                                          "See Details for complete message log contents.",
                                          LogBuffer::get().outputFile().toUtf8());
    }

    //Sys_Shutdown();
    DD_Shutdown();

    // Get outta here.
    exit(1);
}

InFineSystem &App_InFineSystem()
{
    if(App::appExists())
    {
#ifdef __CLIENT__
        return ClientApp::infineSystem();
#endif
#ifdef __SERVER__
        return ServerApp::infineSystem();
#endif
    }
    throw Error("App_InFineSystem", "App not yet initialized");
}

ResourceSystem &App_ResourceSystem()
{
    if(App::appExists())
    {
#ifdef __CLIENT__
        return ClientApp::resourceSystem();
#endif
#ifdef __SERVER__
        return ServerApp::resourceSystem();
#endif
    }
    throw Error("App_ResourceSystem", "App not yet initialized");
}

ResourceClass &App_ResourceClass(String className)
{
    return App_ResourceSystem().resClass(className);
}

ResourceClass &App_ResourceClass(resourceclassid_t classId)
{
    return App_ResourceSystem().resClass(classId);
}

WorldSystem &App_WorldSystem()
{
    if(App::appExists())
    {
#ifdef __CLIENT__
        return ClientApp::worldSystem();
#endif
#ifdef __SERVER__
        return ServerApp::worldSystem();
#endif
    }
    throw Error("App_WorldSystem", "App not yet initialized");
}

static File1 *tryLoadFile(de::Uri const &path, size_t baseOffset = 0);

static void parseStartupFilePathsAndAddFiles(char const *pathString)
{
    static char const *ATWSEPS = ",; \t";

    if(!pathString || !pathString[0]) return;

    size_t len = strlen(pathString);
    char *buffer = (char *) M_Malloc(len + 1);

    strcpy(buffer, pathString);
    char *token = strtok(buffer, ATWSEPS);
    while(token)
    {
        tryLoadFile(de::Uri(token, RC_NULL));
        token = strtok(nullptr, ATWSEPS);
    }
    M_Free(buffer);
}

#undef Con_Open
void Con_Open(dint yes)
{
#ifdef __CLIENT__
    if(yes)
    {
        ClientWindow &win = ClientWindow::main();
        win.taskBar().open();
        win.root().setFocus(&win.console().commandLine());
    }
    else
    {
        ClientWindow::main().console().closeLog();
    }
#endif

#ifdef __SERVER__
    DENG2_UNUSED(yes);
#endif
}

#ifdef __CLIENT__

/**
 * Console command to open/close the console prompt.
 */
D_CMD(OpenClose)
{
    DENG2_UNUSED2(src, argc);

    if(!stricmp(argv[0], "conopen"))
    {
        Con_Open(true);
    }
    else if(!stricmp(argv[0], "conclose"))
    {
        Con_Open(false);
    }
    else
    {
        Con_Open(!ClientWindow::main().console().isLogOpen());
    }
    return true;
}

D_CMD(TaskBar)
{
    DENG2_UNUSED3(src, argc, argv);

    ClientWindow &win = ClientWindow::main();
    if(!win.taskBar().isOpen() || !win.console().commandLine().hasFocus())
    {
        win.taskBar().open();
        win.console().focusOnCommandLine();
    }
    else
    {
        win.taskBar().close();
    }
    return true;
}

D_CMD(Tutorial)
{
    DENG2_UNUSED3(src, argc, argv);
    ClientWindow::main().taskBar().showTutorial();
    return true;
}

#endif // __CLIENT__

/**
 * Find all game data file paths in the auto directory with the extensions
 * wad, lmp, pk3, zip and deh.
 *
 * @param found  List of paths to be populated.
 *
 * @return  Number of paths added to @a found.
 */
static dint findAllGameDataPaths(FS1::PathList &found)
{
    static String const extensions[] = {
        "wad", "lmp", "pk3", "zip", "deh"
#ifdef UNIX
        "WAD", "LMP", "PK3", "ZIP", "DEH" // upper case alternatives
#endif
    };
    dint const numFoundSoFar = found.count();
    for(String const &ext : extensions)
    {
        DENG2_ASSERT(!ext.isEmpty());
        String const searchPath = de::Uri(Path("$(App.DataPath)/$(GamePlugin.Name)/auto/*." + ext)).resolved();
        App_FileSystem().findAllPaths(searchPath, 0, found);
    }
    return found.count() - numFoundSoFar;
}

/**
 * Find and try to load all game data file paths in auto directory.
 *
 * @return Number of new files that were loaded.
 */
static dint loadFilesFromDataGameAuto()
{
    FS1::PathList found;
    findAllGameDataPaths(found);

    dint numLoaded = 0;
    DENG2_FOR_EACH_CONST(FS1::PathList, i, found)
    {
        // Ignore directories.
        if(i->attrib & A_SUBDIR) continue;

        if(tryLoadFile(de::Uri(i->path, RC_NULL)))
        {
            numLoaded += 1;
        }
    }
    return numLoaded;
}

bool DD_ExchangeGamePluginEntryPoints(pluginid_t pluginId)
{
    if(pluginId != 0)
    {
        // Do the API transfer.
        GETGAMEAPI fptAdr;
        if(!(fptAdr = (GETGAMEAPI) DD_FindEntryPoint(pluginId, "GetGameAPI")))
        {
            return false;
        }
        app.GetGameAPI = fptAdr;
        DD_InitAPI();
        Def_GetGameClasses();
    }
    else
    {
        app.GetGameAPI = 0;
        DD_InitAPI();
        Def_GetGameClasses();
    }
    return true;
}

static void loadResource(ResourceManifest &manifest)
{
    DENG2_ASSERT(manifest.resourceClass() == RC_PACKAGE);

    de::Uri path(manifest.resolvedPath(false/*do not locate resource*/), RC_NULL);
    if(path.isEmpty()) return;

    if(File1 *file = tryLoadFile(path))
    {
        // Mark this as an original game resource.
        file->setCustom(false);

        // Print the 'CRC' number of IWADs, so they can be identified.
        if(Wad *wad = file->maybeAs<Wad>())
        {
            LOG_RES_MSG("IWAD identification: %08x") << wad->calculateCRC();
        }
    }
}

struct ddgamechange_params_t
{
    /// @c true iff caller (i.e., App_ChangeGame) initiated busy mode.
    dd_bool initiatedBusyMode;
};

static dint DD_BeginGameChangeWorker(void *context)
{
    ddgamechange_params_t &parms = *static_cast<ddgamechange_params_t *>(context);

    Map::initDummies();
    P_InitMapEntityDefs();

    if(parms.initiatedBusyMode)
    {
        Con_SetProgress(200);
        BusyMode_WorkerEnd();
    }

    return 0;
}

static dint DD_LoadGameStartupResourcesWorker(void *context)
{
    ddgamechange_params_t &parms = *static_cast<ddgamechange_params_t *>(context);

    // Reset file Ids so previously seen files can be processed again.
    App_FileSystem().resetFileIds();
    initPathMappings();
    App_FileSystem().resetAllSchemes();

    if(parms.initiatedBusyMode)
    {
        Con_SetProgress(50);
    }

    if(App_GameLoaded())
    {
        // Create default Auto mappings in the runtime directory.

        // Data class resources.
        App_FileSystem().addPathMapping("auto/", de::Uri("$(App.DataPath)/$(GamePlugin.Name)/auto/", RC_NULL).resolved());

        // Definition class resources.
        App_FileSystem().addPathMapping("auto/", de::Uri("$(App.DefsPath)/$(GamePlugin.Name)/auto/", RC_NULL).resolved());
    }

    /**
     * Open all the files, load headers, count lumps, etc, etc...
     * @note  Duplicate processing of the same file is automatically guarded
     *        against by the virtual file system layer.
     */
    GameManifests const &gameManifests = App_CurrentGame().manifests();
    dint const numPackages = gameManifests.count(RC_PACKAGE);
    if(numPackages)
    {
        LOG_RES_MSG("Loading game resources") << (verbose >= 1? ":" : "...");

        dint packageIdx = 0;
        for(GameManifests::const_iterator i = gameManifests.find(RC_PACKAGE);
            i != gameManifests.end() && i.key() == RC_PACKAGE; ++i, ++packageIdx)
        {
            loadResource(**i);

            // Update our progress.
            if(parms.initiatedBusyMode)
            {
                Con_SetProgress((packageIdx + 1) * (200 - 50) / numPackages - 1);
            }
        }
    }

    if(parms.initiatedBusyMode)
    {
        Con_SetProgress(200);
        BusyMode_WorkerEnd();
    }

    return 0;
}

static dint addListFiles(QStringList const &list, FileType const &ftype)
{
    dint numAdded = 0;
    foreach(QString const &path, list)
    {
        if(&ftype != &DD_GuessFileTypeFromFileName(path))
        {
            continue;
        }

        if(tryLoadFile(de::Uri(path, RC_NULL)))
        {
            numAdded += 1;
        }
    }
    return numAdded;
}

/**
 * (Re-)Initialize the VFS path mappings.
 */
static void initPathMappings()
{
    App_FileSystem().clearPathMappings();

    if(DD_IsShuttingDown()) return;

    // Create virtual directory mappings by processing all -vdmap options.
    dint argC = CommandLine_Count();
    for(dint i = 0; i < argC; ++i)
    {
        if(strnicmp("-vdmap", CommandLine_At(i), 6))
        {
            continue;
        }

        if(i < argC - 1 && !CommandLine_IsOption(i + 1) && !CommandLine_IsOption(i + 2))
        {
            String source      = NativePath(CommandLine_PathAt(i + 1)).expand().withSeparators('/');
            String destination = NativePath(CommandLine_PathAt(i + 2)).expand().withSeparators('/');
            App_FileSystem().addPathMapping(source, destination);
            i += 2;
        }
    }
}

/// Skip all whitespace except newlines.
static inline char const *skipSpace(char const *ptr)
{
    DENG2_ASSERT(ptr != 0);
    while(*ptr && *ptr != '\n' && isspace(*ptr))
    { ptr++; }
    return ptr;
}

static bool parsePathLumpMapping(char lumpName[9/*LUMPNAME_T_MAXLEN*/], ddstring_t *path, char const *buffer)
{
    DENG2_ASSERT(lumpName != 0 && path != 0);

    // Find the start of the lump name.
    char const *ptr = skipSpace(buffer);

    // Just whitespace?
    if(!*ptr || *ptr == '\n') return false;

    // Find the end of the lump name.
    char const *end = (char const *)M_FindWhite((char *)ptr);
    if(!*end || *end == '\n') return false;

    size_t len = end - ptr;
    // Invalid lump name?
    if(len > 8) return false;

    memset(lumpName, 0, 9/*LUMPNAME_T_MAXLEN*/);
    strncpy(lumpName, ptr, len);
    strupr(lumpName);

    // Find the start of the file path.
    ptr = skipSpace(end);
    if(!*ptr || *ptr == '\n') return false; // Missing file path.

    // We're at the file path.
    Str_Set(path, ptr);
    // Get rid of any extra whitespace on the end.
    Str_StripRight(path);
    F_FixSlashes(path, path);
    return true;
}

/**
 * <pre> LUMPNAM0 \\Path\\In\\The\\Base.ext
 * LUMPNAM1 Path\\In\\The\\RuntimeDir.ext
 *  :</pre>
 */
static bool parsePathLumpMappings(char const *buffer)
{
    DENG2_ASSERT(buffer != 0);

    bool successful = false;
    ddstring_t path; Str_Init(&path);
    ddstring_t line; Str_Init(&line);

    char const *ch = buffer;
    char lumpName[9/*LUMPNAME_T_MAXLEN*/];
    do
    {
        ch = Str_GetLine(&line, ch);
        if(!parsePathLumpMapping(lumpName, &path, Str_Text(&line)))
        {
            // Failure parsing the mapping.
            // Ignore errors in individual mappings and continue parsing.
            //goto parseEnded;
        }
        else
        {
            String destination = NativePath(Str_Text(&path)).expand().withSeparators('/');
            App_FileSystem().addPathLumpMapping(lumpName, destination);
        }
    } while(*ch);

    // Success.
    successful = true;

//parseEnded:
    Str_Free(&line);
    Str_Free(&path);
    return successful;
}

/**
 * (Re-)Initialize the path => lump mappings.
 * @note Should be called after WADs have been processed.
 */
static void initPathLumpMappings()
{
    // Free old paths, if any.
    App_FileSystem().clearPathLumpMappings();

    if(DD_IsShuttingDown()) return;

    size_t bufSize = 0;
    uint8_t *buf = 0;

    // Add the contents of all DD_DIREC lumps.
    /// @todo fixme: Enforce scope to the containing package!
    LumpIndex const &lumpIndex = App_FileSystem().nameIndex();
    LumpIndex::FoundIndices foundDirecs;
    lumpIndex.findAll("DD_DIREC.lmp", foundDirecs);
    DENG2_FOR_EACH_CONST(LumpIndex::FoundIndices, i, foundDirecs) // in load order
    {
        File1 &lump          = lumpIndex[*i];
        FileInfo const &lumpInfo = lump.info();

        // Make a copy of it so we can ensure it ends in a null.
        if(bufSize < lumpInfo.size + 1)
        {
            bufSize = lumpInfo.size + 1;
            buf = (uint8_t *) M_Realloc(buf, bufSize);
        }

        lump.read(buf, 0, lumpInfo.size);
        buf[lumpInfo.size] = 0;
        parsePathLumpMappings(reinterpret_cast<char const *>(buf));
    }

    M_Free(buf);
}

static dint DD_LoadAddonResourcesWorker(void *context)
{
    ddgamechange_params_t &parms = *static_cast<ddgamechange_params_t *>(context);

    /**
     * Add additional game-startup files.
     * @note These must take precedence over Auto but not game-resource files.
     */
    if(startupFiles && startupFiles[0])
    {
        parseStartupFilePathsAndAddFiles(startupFiles);
    }

    if(parms.initiatedBusyMode)
    {
        Con_SetProgress(50);
    }

    if(App_GameLoaded())
    {
        /**
         * Phase 3: Add real files from the Auto directory.
         */
        game::Session::Profile &prof = game::Session::profile();

        FS1::PathList found;
        findAllGameDataPaths(found);
        DENG2_FOR_EACH_CONST(FS1::PathList, i, found)
        {
            // Ignore directories.
            if(i->attrib & A_SUBDIR) continue;

            /// @todo Is expansion of symbolics still necessary here?
            prof.resourceFiles << NativePath(i->path).expand().withSeparators('/');
        }

        if(!prof.resourceFiles.isEmpty())
        {
            // First ZIPs then WADs (they may contain WAD files).
            addListFiles(prof.resourceFiles, DD_FileTypeByName("FT_ZIP"));
            addListFiles(prof.resourceFiles, DD_FileTypeByName("FT_WAD"));
        }

        // Final autoload round.
        DD_AutoLoad();
    }

    if(parms.initiatedBusyMode)
    {
        Con_SetProgress(180);
    }

    initPathLumpMappings();

    // Re-initialize the resource locator as there are now new resources to be found
    // on existing search paths (probably that is).
    App_FileSystem().resetAllSchemes();

    if(parms.initiatedBusyMode)
    {
        Con_SetProgress(200);
        BusyMode_WorkerEnd();
    }

    return 0;
}

static dint DD_ActivateGameWorker(void *context)
{
    ddgamechange_params_t &parms = *static_cast<ddgamechange_params_t *>(context);

    ResourceSystem &resSys = App_ResourceSystem();

    // Some resources types are located prior to initializing the game.
    resSys.initTextures();
    resSys.textureScheme("Lightmaps").clear();
    resSys.textureScheme("Flaremaps").clear();
    resSys.initMapDefs();

    if(parms.initiatedBusyMode)
    {
        Con_SetProgress(50);
    }

    // Now that resources have been located we can begin to initialize the game.
    if(App_GameLoaded())
    {
        // Any game initialization hooks?
        DD_CallHooks(HOOK_GAME_INIT, 0, 0);

        if(gx.PreInit)
        {
            DENG2_ASSERT(App_CurrentGame().pluginId() != 0);

            DD_SetActivePluginId(App_CurrentGame().pluginId());
            gx.PreInit(App_Games().id(App_CurrentGame()));
            DD_SetActivePluginId(0);
        }
    }

    if(parms.initiatedBusyMode)
    {
        Con_SetProgress(100);
    }

    if(App_GameLoaded())
    {
        // Parse the game's main config file.
        // If a custom top-level config is specified; let it override.
        Path configFile;
        if(CommandLine_CheckWith("-config", 1))
        {
            configFile = NativePath(CommandLine_NextAsPath()).withSeparators('/');
        }
        else
        {
            configFile = App_CurrentGame().mainConfig();
        }

        LOG_SCR_MSG("Parsing primary config \"%s\"...") << NativePath(configFile).pretty();
        Con_ParseCommands(configFile, CPCF_SET_DEFAULT | CPCF_ALLOW_SAVE_STATE);

#ifdef __CLIENT__
        // Apply default control bindings for this game.
        ClientApp::inputSystem().bindGameDefaults();

        // Read bindings for this game and merge with the working set.
        Con_ParseCommands(App_CurrentGame().bindingConfig(), CPCF_ALLOW_SAVE_BINDINGS);
#endif
    }

    if(parms.initiatedBusyMode)
    {
        Con_SetProgress(120);
    }

    Def_Read();

    if(parms.initiatedBusyMode)
    {
        Con_SetProgress(130);
    }

    resSys.initSprites(); // Fully initialize sprites.
#ifdef __CLIENT__
    resSys.initModels();
#endif

    Def_PostInit();

    DD_ReadGameHelp();

    // Reset the tictimer so than any fractional accumulation is not added to
    // the tic/game timer of the newly-loaded game.
    gameTime = 0;
    DD_ResetTimer();

#ifdef __CLIENT__
    // Make sure that the next frame does not use a filtered viewer.
    R_ResetViewer();
#endif

    // Invalidate old cmds and init player values.
    for(dint i = 0; i < DDMAXPLAYERS; ++i)
    {
        player_t *plr = &ddPlayers[i];

        plr->extraLight = plr->targetExtraLight = 0;
        plr->extraLightCounter = 0;
    }

    if(gx.PostInit)
    {
        DD_SetActivePluginId(App_CurrentGame().pluginId());
        gx.PostInit();
        DD_SetActivePluginId(0);
    }

    if(parms.initiatedBusyMode)
    {
        Con_SetProgress(200);
        BusyMode_WorkerEnd();
    }

    return 0;
}

de::Games &App_Games()
{
    if(App::appExists())
    {
#ifdef __CLIENT__
        return ClientApp::games();
#endif
#ifdef __SERVER__
        return ServerApp::games();
#endif
    }
    throw Error("App_Games", "App not yet initialized");
}

dd_bool App_GameLoaded()
{
    if(!App::appExists()) return false;

    return !App_CurrentGame().isNull();
}

void App_ClearGames()
{
    App_Games().clear();
    App::app().setGame(App_Games().nullGame());
}

static void populateGameInfo(GameInfo &info, de::Game &game)
{
    info.identityKey = AutoStr_FromTextStd(game.identityKey().toUtf8().constData());
    info.title       = AutoStr_FromTextStd(game.title().toUtf8().constData());
    info.author      = AutoStr_FromTextStd(game.author().toUtf8().constData());
}

/// @note Part of the Doomsday public API.
#undef DD_GameInfo
dd_bool DD_GameInfo(GameInfo *info)
{
    LOG_AS("DD_GameInfo");
    if(!info) return false;

    zapPtr(info);

    if(App_GameLoaded())
    {
        populateGameInfo(*info, App_CurrentGame());
        return true;
    }

    LOGDEV_WARNING("No game currently loaded");
    return false;
}

#undef DD_AddGameResource
void DD_AddGameResource(gameid_t gameId, resourceclassid_t classId, dint rflags,
    char const *names, void *params)
{
    if(!VALID_RESOURCECLASSID(classId))
        App_Error("DD_AddGameResource: Unknown resource class %i.", (dint)classId);

    if(!names || !names[0])
        App_Error("DD_AddGameResource: Invalid name argument.");

    // Construct and attach the new resource record.
    Game &game = App_Games().byId(gameId);
    ResourceManifest *manifest = new ResourceManifest(classId, rflags);
    game.addManifest(*manifest);

    // Add the name list to the resource record.
    QStringList nameList = String(names).split(";", QString::SkipEmptyParts);
    foreach(QString const &nameRef, nameList)
    {
        manifest->addName(nameRef);
    }

    if(params && classId == RC_PACKAGE)
    {
        // Add the identityKey list to the resource record.
        QStringList idKeys = String((char const *) params).split(";", QString::SkipEmptyParts);
        foreach(QString const &idKeyRef, idKeys)
        {
            manifest->addIdentityKey(idKeyRef);
        }
    }
}

#undef DD_DefineGame
gameid_t DD_DefineGame(GameDef const *def)
{
    LOG_AS("DD_DefineGame");
    if(!def) return 0; // Invalid id.

    // Game mode identity keys must be unique. Ensure that is the case.
    try
    {
        /*Game &game =*/ App_Games().byIdentityKey(def->identityKey);
        LOGDEV_WARNING("Ignored new game \"%s\", identity key '%s' already in use")
                << def->defaultTitle << def->identityKey;
        return 0; // Invalid id.
    }
    catch(Games::NotFoundError const &)
    {} // Ignore the error.

    Game *game = Game::fromDef(*def);
    if(!game) return 0; // Invalid def.

    // Add this game to our records.
    game->setPluginId(DD_ActivePluginId());
    App_Games().add(*game);
    return App_Games().id(*game);
}

#undef DD_GameIdForKey
gameid_t DD_GameIdForKey(char const *identityKey)
{
    try
    {
        return App_Games().id(App_Games().byIdentityKey(identityKey));
    }
    catch(Games::NotFoundError const &)
    {
        LOG_AS("DD_GameIdForKey");
        LOGDEV_WARNING("Game \"%s\" is not defined, returning 0.") << identityKey;
    }
    return 0; // Invalid id.
}

de::Game &App_CurrentGame()
{
    return App::game().as<de::Game>();
}

bool App_ChangeGame(Game &game, bool allowReload)
{
#ifdef __CLIENT__
    DENG_ASSERT(ClientWindow::mainExists());
#endif

    //LOG_AS("App_ChangeGame");

    bool isReload = false;

    // Ignore attempts to re-load the current game?
    if(&App_CurrentGame() == &game)
    {
        if(!allowReload)
        {
            if(App_GameLoaded())
            {
                LOG_NOTE("%s (%s) is already loaded")
                        << game.title() << game.identityKey();
            }
            return true;
        }
        // We are re-loading.
        isReload = true;
    }

    // The current game will be gone very soon.
    DENG2_FOR_EACH_OBSERVER(App::GameUnloadAudience, i, App::app().audienceForGameUnload())
    {
        i->aboutToUnloadGame(App::game());
    }

    // Quit netGame if one is in progress.
#ifdef __SERVER__
    if(netGame && isServer)
    {
        N_ServerClose();
    }
#else
    if(netGame)
    {
        Con_Execute(CMDS_DDAY, "net disconnect", true, false);
    }
#endif

    S_Reset();

#ifdef __CLIENT__
    Demo_StopPlayback();

    GL_PurgeDeferredTasks();

    //if(!Sys_IsShuttingDown())
    {
        App_ResourceSystem().releaseAllGLTextures();
        App_ResourceSystem().pruneUnusedTextureSpecs();
        GL_LoadLightingSystemTextures();
        GL_LoadFlareTextures();
        Rend_ParticleLoadSystemTextures();
    }

    GL_ResetViewEffects();

    if(!game.isNull())
    {
        ClientWindow &mainWin = ClientWindow::main();
        mainWin.taskBar().close();

        // Trap the mouse automatically when loading a game in fullscreen.
        if(mainWin.isFullScreen())
        {
            mainWin.canvas().trapMouse();
        }
    }
#endif

    // If a game is presently loaded; unload it.
    if(App_GameLoaded())
    {
        if(gx.Shutdown)
        {
            gx.Shutdown();
        }
        Con_SaveDefaults();

#ifdef __CLIENT__
        R_ClearViewData();
        R_DestroyContactLists();
        P_ClearPlayerImpulses();

        Con_Execute(CMDS_DDAY, "clearbindings", true, false);
        ClientApp::inputSystem().bindDefaults();
        ClientApp::inputSystem().initialContextActivations();
#endif
        // Reset the world back to it's initial state (unload the map, reset players, etc...).
        App_WorldSystem().reset();

        Z_FreeTags(PU_GAMESTATIC, PU_PURGELEVEL - 1);

        P_ShutdownMapEntityDefs();

        R_ShutdownSvgs();

        App_ResourceSystem().clearAllRuntimeResources();
        App_ResourceSystem().clearAllAnimGroups();
        App_ResourceSystem().clearAllColorPalettes();

        Sfx_InitLogical();

        Con_ClearDatabases();

        { // Tell the plugin it is being unloaded.
            void *unloader = DD_FindEntryPoint(App_CurrentGame().pluginId(), "DP_Unload");
            LOGDEV_MSG("Calling DP_Unload %p") << unloader;
            DD_SetActivePluginId(App_CurrentGame().pluginId());
            if(unloader) ((pluginfunc_t)unloader)();
            DD_SetActivePluginId(0);
        }

        // We do not want to load session resources specified on the command line again.
        game::Session::profile().resourceFiles.clear();

        // The current game is now the special "null-game".
        App::app().setGame(App_Games().nullGame());

        Con_InitDatabases();
        consoleRegister();

        R_InitSvgs();

#ifdef __CLIENT__
        ClientApp::inputSystem().initAllDevices();
#endif

#ifdef __CLIENT__
        R_InitViewWindow();
#endif

        App_FileSystem().unloadAllNonStartupFiles();

        // Reset file IDs so previously seen files can be processed again.
        /// @todo this releases the IDs of startup files too but given the
        /// only startup file is doomsday.pk3 which we never attempt to load
        /// again post engine startup, this isn't an immediate problem.
        App_FileSystem().resetFileIds();

        // Update the dir/WAD translations.
        initPathLumpMappings();
        initPathMappings();

        App_FileSystem().resetAllSchemes();
    }

    App_InFineSystem().reset();
#ifdef __CLIENT__
    App_InFineSystem().deinitBindingContext();
#endif

    /// @todo The entire material collection should not be destroyed during a reload.
    App_ResourceSystem().clearAllMaterialSchemes();

    if(!game.isNull())
    {
        LOG_MSG("Selecting game '%s'...") << game.id();
    }
    else if(!isReload)
    {
        LOG_MSG("Unloaded game");
    }

    Library_ReleaseGames();

#ifdef __CLIENT__
    ClientWindow::main().setWindowTitle(DD_ComposeMainWindowTitle());
#endif

    if(!DD_IsShuttingDown())
    {
        // Re-initialize subsystems needed even when in ringzero.
        if(!DD_ExchangeGamePluginEntryPoints(game.pluginId()))
        {
            LOG_WARNING("Game plugin for '%s' is invalid") << game.id();
            LOGDEV_WARNING("Failed exchanging entrypoints with plugin %i")
                    << dint(game.pluginId());
            return false;
        }
    }

    // This is now the current game.
    App::app().setGame(game);
    game::Session::profile().gameId = game.id();

#ifdef __CLIENT__
    ClientWindow::main().setWindowTitle(DD_ComposeMainWindowTitle());
#endif

    /*
     * If we aren't shutting down then we are either loading a game or switching
     * to ringzero (the current game will have already been unloaded).
     */
    if(!DD_IsShuttingDown())
    {
#ifdef __CLIENT__
        App_InFineSystem().initBindingContext();
#endif

        /*
         * The bulk of this we can do in busy mode unless we are already busy
         * (which can happen if a fatal error occurs during game load and we must
         * shutdown immediately; Sys_Shutdown will call back to load the special
         * "null-game" game).
         */
        dint const busyMode = BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0);
        ddgamechange_params_t p;
        BusyTask gameChangeTasks[] = {
            // Phase 1: Initialization.
            { DD_BeginGameChangeWorker,          &p, busyMode, "Loading game...",   200, 0.0f, 0.1f, 0 },

            // Phase 2: Loading "startup" resources.
            { DD_LoadGameStartupResourcesWorker, &p, busyMode, nullptr,                200, 0.1f, 0.3f, 0 },

            // Phase 3: Loading "add-on" resources.
            { DD_LoadAddonResourcesWorker,       &p, busyMode, "Loading add-ons...", 200, 0.3f, 0.7f, 0 },

            // Phase 4: Game activation.
            { DD_ActivateGameWorker,             &p, busyMode, "Starting game...",  200, 0.7f, 1.0f, 0 }
        };

        p.initiatedBusyMode = !BusyMode_Active();

        if(App_GameLoaded())
        {
            // Tell the plugin it is being loaded.
            /// @todo Must this be done in the main thread?
            void *loader = DD_FindEntryPoint(App_CurrentGame().pluginId(), "DP_Load");
            LOGDEV_MSG("Calling DP_Load %p") << loader;
            DD_SetActivePluginId(App_CurrentGame().pluginId());
            if(loader) ((pluginfunc_t)loader)();
            DD_SetActivePluginId(0);
        }

        /// @todo Kludge: Use more appropriate task names when unloading a game.
        if(game.isNull())
        {
            gameChangeTasks[0].name = "Unloading game...";
            gameChangeTasks[3].name = "Switching to ringzero...";
        }
        // kludge end

        BusyMode_RunTasks(gameChangeTasks, sizeof(gameChangeTasks)/sizeof(gameChangeTasks[0]));

#ifdef __CLIENT__
        // Process any GL-related tasks we couldn't while Busy.
        Rend_ParticleLoadExtraTextures();
#endif

        if(App_GameLoaded())
        {
            Game::printBanner(App_CurrentGame());
        }
        /*else
        {
            // Lets play a nice title animation.
            DD_StartTitle();
        }*/
    }

    DENG_ASSERT(DD_ActivePluginId() == 0);

#ifdef __CLIENT__
    if(!Sys_IsShuttingDown())
    {
        /**
         * Clear any input events we may have accumulated during this process.
         * @note Only necessary here because we might not have been able to use
         *       busy mode (which would normally do this for us on end).
         */
        ClientApp::inputSystem().clearEvents();

        if(!App_GameLoaded())
        {
            ClientWindow::main().taskBar().open();
        }
        else
        {
            ClientWindow::main().console().zeroLogHeight();
        }
    }
#endif

    // Game change is complete.
    DENG2_FOR_EACH_OBSERVER(App::GameChangeAudience, i, App::app().audienceForGameChange())
    {
        i->currentGameChanged(App::game());
    }

    return true;
}

bool DD_IsShuttingDown()
{
    return Sys_IsShuttingDown();
}

/**
 * Looks for new files to autoload from the auto-load data directory.
 */
static void DD_AutoLoad()
{
    /**
     * Keep loading files if any are found because virtual files may now
     * exist in the auto-load directory.
     */
    dint numNewFiles;
    while((numNewFiles = loadFilesFromDataGameAuto()) > 0)
    {
        LOG_RES_VERBOSE("Autoload round completed with %i new files") << numNewFiles;
    }
}

/**
 * Attempt to determine which game is to be played.
 *
 * @todo Logic here could be much more elaborate but is it necessary?
 */
Game *DD_AutoselectGame()
{
    if(CommandLine_CheckWith("-game", 1))
    {
        char const *identityKey = CommandLine_Next();
        try
        {
            Game &game = App_Games().byIdentityKey(identityKey);
            if(game.allStartupFilesFound())
            {
                return &game;
            }
        }
        catch(Games::NotFoundError const &)
        {} // Ignore the error.
    }

    // If but one lonely game; select it.
    if(App_Games().numPlayable() == 1)
    {
        return App_Games().firstPlayable();
    }

    // We don't know what to do.
    return 0;
}

dint DD_EarlyInit()
{
    // Determine the requested degree of verbosity.
    ::verbose = CommandLine_Exists("-verbose");

#ifdef __SERVER__
    ::isDedicated = true;
#else
    ::isDedicated = false;
#endif

    // Bring the console online as soon as we can.
    DD_ConsoleInit();
    Con_InitDatabases();

    // Register the engine's console commands and variables.
    consoleRegister();

    return true;
}

// Perform basic runtime type size checks.
#ifdef DENG2_DEBUG
static void assertTypeSizes()
{
    void *ptr = 0;
    int32_t int32 = 0;
    int16_t int16 = 0;
    dfloat float32 = 0;

    DENG2_UNUSED(ptr);
    DENG2_UNUSED(int32);
    DENG2_UNUSED(int16);
    DENG2_UNUSED(float32);

    ASSERT_32BIT(int32);
    ASSERT_16BIT(int16);
    ASSERT_32BIT(float32);
#ifdef __64BIT__
    ASSERT_64BIT(ptr);
    ASSERT_64BIT(int64_t);
#else
    ASSERT_NOT_64BIT(ptr);
#endif
}
#endif

/**
 * Engine initialization. Once completed the game loop is ready to be started.
 * Called from the app entrypoint function.
 */
static void initialize()
{
    DENG2_DEBUG_ONLY( assertTypeSizes(); )

    static char const *AUTOEXEC_NAME = "autoexec.cfg";

#ifdef __CLIENT__
    GL_EarlyInit();
#endif

    // Initialize the subsystems needed prior to entering busy mode for the first time.
    Sys_Init();
    ResourceClass::setResourceClassCallback(App_ResourceClass);
    registerResourceFileTypes();
    F_Init();
    DD_CreateFileSystemSchemes();

#ifdef __CLIENT__
    FR_Init();

    // Enter busy mode until startup complete.
    Con_InitProgress2(200, 0, .25f); // First half.
#endif
    BusyMode_RunNewTaskWithName(BUSYF_NO_UPLOADS | BUSYF_STARTUP | BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                DD_StartupWorker, 0, "Starting up...");

    // Engine initialization is complete. Now finish up with the GL.
#ifdef __CLIENT__
    GL_Init();
    GL_InitRefresh();
    App_ResourceSystem().clearAllTextureSpecs();
    App_ResourceSystem().initSystemTextures();
    LensFx_Init();
#endif

#ifdef __CLIENT__
    // Do deferred uploads.
    Con_InitProgress2(200, .25f, .25f); // Stop here for a while.
#endif
    BusyMode_RunNewTaskWithName(BUSYF_STARTUP | BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                DD_DummyWorker, 0, "Buffering...");

    // Add resource paths specified using -iwad on the command line.
    FS1::Scheme &scheme = App_FileSystem().scheme(App_ResourceClass("RC_PACKAGE").defaultScheme());
    for(dint p = 0; p < CommandLine_Count(); ++p)
    {
        if(!CommandLine_IsMatchingAlias("-iwad", CommandLine_At(p)))
        {
            continue;
        }

        while(++p != CommandLine_Count() && !CommandLine_IsOption(p))
        {
            /// @todo Do not add these as search paths, publish them directly to
            ///       the "Packages" scheme.

            // CommandLine_PathAt() always returns an absolute path.
            directory_t *dir = Dir_FromText(CommandLine_PathAt(p));
            de::Uri uri = de::Uri::fromNativeDirPath(Dir_Path(dir), RC_PACKAGE);

            LOG_RES_NOTE("User-supplied IWAD path: \"%s\"") << Dir_Path(dir);

            scheme.addSearchPath(SearchPath(uri, SearchPath::NoDescend));

            Dir_Delete(dir);
        }

        p--;/* For ArgIsOption(p) necessary, for p==Argc() harmless */
    }

    App_ResourceSystem().updateOverrideIWADPathFromConfig();

    //
    // Try to locate all required data files for all registered games.
    //
#ifdef __CLIENT__
    Con_InitProgress2(200, .25f, 1); // Second half.
#endif
    App_Games().locateAllResources();

    // Attempt automatic game selection.
    if(!CommandLine_Exists("-noautoselect") || isDedicated)
    {
        if(de::Game *game = DD_AutoselectGame())
        {
            // An implicit game session profile has been defined.
            // Add all resources specified using -file options on the command line
            // to the list for the session.
            game::Session::Profile &prof = game::Session::profile();

            for(dint p = 0; p < CommandLine_Count(); ++p)
            {
                if(!CommandLine_IsMatchingAlias("-file", CommandLine_At(p)))
                {
                    continue;
                }

                while(++p != CommandLine_Count() && !CommandLine_IsOption(p))
                {
                    prof.resourceFiles << NativePath(CommandLine_PathAt(p)).expand().withSeparators('/');
                }

                p--;/* For ArgIsOption(p) necessary, for p==Argc() harmless */
            }

            // Begin the game session.
            App_ChangeGame(*game);
        }
#ifdef __SERVER__
        else
        {
            // A server is presently useless without a game, as shell
            // connections can only be made after a game is loaded and the
            // server mode started.
            /// @todo Allow shell connections in ringzero mode, too.
            App_Error("No playable games available.");
        }
#endif
    }

    initPathLumpMappings();

    // Re-initialize the filesystem subspace schemes as there are now new
    // resources to be found on existing search paths (probably that is).
    App_FileSystem().resetAllSchemes();

    //
    // One-time execution of various command line features available during startup.
    //
    if(CommandLine_CheckWith("-dumplump", 1))
    {
        String name = CommandLine_Next();
        lumpnum_t lumpNum = App_FileSystem().lumpNumForName(name);
        if(lumpNum >= 0)
        {
            F_DumpFile(App_FileSystem().lump(lumpNum), 0);
        }
        else
        {
            LOG_RES_WARNING("Cannot dump unknown lump \"%s\"") << name;
        }
    }

    if(CommandLine_Check("-dumpwaddir"))
    {
        Con_Executef(CMDS_CMDLINE, false, "listlumps");
    }

    // Try to load the autoexec file. This is done here to make sure everything is
    // initialized: the user can do here anything that s/he'd be able to do in-game
    // provided a game was loaded during startup.
    if(F_FileExists(AUTOEXEC_NAME))
    {
        Con_ParseCommands(AUTOEXEC_NAME);
    }

    // Read additional config files that should be processed post engine init.
    if(CommandLine_CheckWith("-parse", 1))
    {
        LOG_AS("-parse");
        Time begunAt;
        forever
        {
            char const *arg = CommandLine_Next();
            if(!arg || arg[0] == '-') break;

            LOG_MSG("Additional (pre-init) config file \"%s\"") << NativePath(arg).pretty();
            Con_ParseCommands(arg);
        }
        LOGDEV_SCR_VERBOSE("Completed in %.2f seconds") << begunAt.since();
    }

    // A console command on the command line?
    for(dint p = 1; p < CommandLine_Count() - 1; p++)
    {
        if(stricmp(CommandLine_At(p), "-command") &&
           stricmp(CommandLine_At(p), "-cmd"))
        {
            continue;
        }

        for(++p; p < CommandLine_Count(); p++)
        {
            char const *arg = CommandLine_At(p);

            if(arg[0] == '-')
            {
                p--;
                break;
            }
            Con_Execute(CMDS_CMDLINE, arg, false, false);
        }
    }

    //
    // One-time execution of network commands on the command line.
    // Commands are only executed if we have loaded a game during startup.
    //
    if(App_GameLoaded())
    {
        // Client connection command.
        if(CommandLine_CheckWith("-connect", 1))
        {
            Con_Executef(CMDS_CMDLINE, false, "connect %s", CommandLine_Next());
        }

        // Incoming TCP port.
        if(CommandLine_CheckWith("-port", 1))
        {
            Con_Executef(CMDS_CMDLINE, false, "net-ip-port %s", CommandLine_Next());
        }

#ifdef __SERVER__
        // Automatically start the server.
        N_ServerOpen();
#endif
    }
    else
    {
        // No game loaded.
        // Lets get most of everything else initialized.
        // Reset file IDs so previously seen files can be processed again.
        App_FileSystem().resetFileIds();
        initPathLumpMappings();
        initPathMappings();
        App_FileSystem().resetAllSchemes();

        App_ResourceSystem().initTextures();
        App_ResourceSystem().textureScheme("Lightmaps").clear();
        App_ResourceSystem().textureScheme("Flaremaps").clear();
        App_ResourceSystem().initMapDefs();

        Def_Read();

        App_ResourceSystem().initSprites();
#ifdef __CLIENT__
        App_ResourceSystem().initModels();
#endif

        Def_PostInit();

        if(!CommandLine_Exists("-noautoselect"))
        {
            LOG_NOTE("Game could not be selected automatically");
        }
    }
}

/**
 * This gets called when the main window is ready for GL init. The application
 * event loop is already running.
 */
void DD_FinishInitializationAfterWindowReady()
{
    LOGDEV_MSG("Window is ready, finishing initialization");

#ifdef __CLIENT__
# ifdef WIN32
    // Now we can get the color transfer table as the window is available.
    DisplayMode_SaveOriginalColorTransfer();
# endif

    if(!Sys_GLInitialize())
    {
        App_Error("Error initializing OpenGL.\n");
    }
    else
    {
        ClientWindow::main().setWindowTitle(DD_ComposeMainWindowTitle());
    }
#endif

    // Initialize engine subsystems and initial state.
    try
    {
        initialize();

        /// @todo This notification should be done from the app.
        DENG2_FOR_EACH_OBSERVER(App::StartupCompleteAudience, i, App::app().audienceForStartupComplete())
        {
            i->appStartupCompleted();
        }
        return;
    }
    catch(Error const &er)
    {
        Sys_CriticalMessage((er.asText() + ".").toUtf8().constData());
    }
    catch(...)
    {}
    exit(2);  // Cannot continue...
}

static dint DD_StartupWorker(void * /*context*/)
{
#ifdef WIN32
    // Initialize COM for this thread (needed for DirectInput).
    CoInitialize(nullptr);
#endif
    Con_SetProgress(10);

    // Any startup hooks?
    DD_CallHooks(HOOK_STARTUP, 0, 0);
    Con_SetProgress(20);

    // Was the change to userdir OK?
    if(CommandLine_CheckWith("-userdir", 1) && !app.usingUserDir)
    {
        LOG_WARNING("User directory not found (check -userdir)");
    }

    initPathMappings();
    App_FileSystem().resetAllSchemes();

    Con_SetProgress(40);

    Net_Init();
    Sys_HideMouseCursor();

    // Read config files that should be read BEFORE engine init.
    if(CommandLine_CheckWith("-cparse", 1))
    {
        Time begunAt;
        LOG_AS("-cparse")

        forever
        {
            char const *arg = CommandLine_NextAsPath();
            if(!arg || arg[0] == '-') break;

            LOG_MSG("Additional (pre-init) config file \"%s\"") << NativePath(arg).pretty();
            Con_ParseCommands(arg);
        }
        LOGDEV_SCR_VERBOSE("Completed in %.2f seconds") << begunAt.since();
    }

    //
    // Add required engine resource files.
    //
    String foundPath = App_FileSystem().findPath(de::Uri("doomsday.pk3", RC_PACKAGE),
                                                 RLF_DEFAULT, App_ResourceClass(RC_PACKAGE));
    foundPath = App_BasePath() / foundPath;  // Ensure the path is absolute.
    File1 *loadedFile = tryLoadFile(de::Uri(foundPath, RC_NULL));
    DENG2_ASSERT(loadedFile);
    DENG2_UNUSED(loadedFile);

    // No more files or packages will be loaded in "startup mode" after this point.
    App_FileSystem().endStartup();

    // Load engine help resources.
    DD_InitHelp();
    Con_SetProgress(60);

    // Execute the startup script (Startup.cfg).
    char const *startupConfig = "startup.cfg";
    if(F_FileExists(startupConfig))
    {
        Con_ParseCommands(startupConfig);
    }
    Con_SetProgress(90);

    R_BuildTexGammaLut();
#ifdef __CLIENT__
    UI_LoadFonts();
#endif
    R_InitSvgs();
#ifdef __CLIENT__
    R_InitViewWindow();
    R_ResetFrameCount();
#endif
    Con_SetProgress(165);

    Net_InitGame();
#ifdef __CLIENT__
    Demo_Init();
#endif
    Con_SetProgress(190);

    // In dedicated mode the console must be opened, so all input events
    // will be handled by it.
    if(isDedicated)
    {
        Con_Open(true);
    }
    Con_SetProgress(199);

    DD_CallHooks(HOOK_INIT, 0, 0);  // Any initialization hooks?
    Con_SetProgress(200);

#ifdef WIN32
    // This thread has finished using COM.
    CoUninitialize();
#endif

    BusyMode_WorkerEnd();
    return 0;
}

/**
 * This only exists so we have something to call while the deferred uploads of the
 * startup are processed.
 */
static dint DD_DummyWorker(void * /*context*/)
{
    Con_SetProgress(200);
    BusyMode_WorkerEnd();
    return 0;
}

void DD_CheckTimeDemo()
{
    static bool checked = false;

    if(!checked)
    {
        checked = true;
        if(CommandLine_CheckWith("-timedemo", 1) || // Timedemo mode.
           CommandLine_CheckWith("-playdemo", 1))   // Play-once mode.
        {
            Block cmd = String("playdemo %1").arg(CommandLine_Next()).toUtf8();
            Con_Execute(CMDS_CMDLINE, cmd.constData(), false, false);
        }
    }
}

static dint DD_UpdateEngineStateWorker(void *context)
{
    DENG2_ASSERT(context);
    auto const initiatedBusyMode = *static_cast<bool *>(context);

#ifdef __CLIENT__
    if(!novideo)
    {
        GL_InitRefresh();
        App_ResourceSystem().clearAllTextureSpecs();
        App_ResourceSystem().initSystemTextures();
    }
#endif

    if(initiatedBusyMode)
    {
        Con_SetProgress(50);
    }

    // Allow previously seen files to be processed again.
    App_FileSystem().resetFileIds();

    // Re-read definitions.
    Def_Read();

    //
    // Rebuild resource data models (defs might've changed).
    //
    App_ResourceSystem().clearAllRawTextures();
    App_ResourceSystem().initSprites();
#ifdef __CLIENT__
    App_ResourceSystem().initModels();
#endif
    Def_PostInit();

    //
    // Update misc subsystems.
    //
    App_WorldSystem().update();

#ifdef __CLIENT__
    // Recalculate the light range mod matrix.
    Rend_UpdateLightModMatrix();
    // The rendering lists have persistent data that has changed during the
    // re-initialization.
    ClientApp::renderSystem().clearDrawLists();
#endif

    /// @todo fixme: Update the game title and the status.

#ifdef DENG2_DEBUG
    Z_CheckHeap();
#endif

    if(initiatedBusyMode)
    {
        Con_SetProgress(200);
        BusyMode_WorkerEnd();
    }
    return 0;
}

void DD_UpdateEngineState()
{
    LOG_MSG("Updating engine state...");

    BusyMode_FreezeGameForBusyMode();

    // Stop playing sounds and music.
    S_Reset();

#ifdef __CLIENT__
    GL_SetFilter(false);
    Demo_StopPlayback();
#endif

    //App_FileSystem().resetFileIds();

    // Update the dir/WAD translations.
    initPathLumpMappings();
    initPathMappings();
    // Re-build the filesystem subspace schemes as there may be new resources to be found.
    App_FileSystem().resetAllSchemes();

    App_ResourceSystem().initTextures();
    App_ResourceSystem().initMapDefs();

    if(App_GameLoaded() && gx.UpdateState)
    {
        gx.UpdateState(DD_PRE);
    }

#ifdef __CLIENT__
    dd_bool hadFog = usingFog;

    GL_TotalReset();
    GL_TotalRestore(); // Bring GL back online.

    // Make sure the fog is enabled, if necessary.
    if(hadFog)
    {
        GL_UseFog(true);
    }
#endif

    // The bulk of this we can do in busy mode unless we are already busy
    // (which can happen during a runtime game change).
    bool initiatedBusyMode = !BusyMode_Active();
    if(initiatedBusyMode)
    {
#ifdef __CLIENT__
        Con_InitProgress(200);
#endif
        BusyMode_RunNewTaskWithName(BUSYF_ACTIVITY | BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                    DD_UpdateEngineStateWorker, &initiatedBusyMode,
                                    "Updating engine state...");
    }
    else
    {
        /// @todo Update the current task name and push progress.
        DD_UpdateEngineStateWorker(&initiatedBusyMode);
    }

    if(App_GameLoaded() && gx.UpdateState)
    {
        gx.UpdateState(DD_POST);
    }

#ifdef __CLIENT__
    App_ResourceSystem().forAllMaterials([] (Material &material)
    {
        return material.forAllAnimators([] (MaterialAnimator &animator)
        {
            animator.rewind();
            return LoopContinue;
        });
    });
#endif
}

struct ddvalue_t
{
    dint *readPtr;
    dint *writePtr;
};

ddvalue_t ddValues[DD_LAST_VALUE - DD_FIRST_VALUE - 1] = {
    {&netGame, 0},
    {&isServer, 0},                         // An *open* server?
    {&isClient, 0},
#ifdef __SERVER__
    {&allowFrames, &allowFrames},
#else
    {0, 0},
#endif
    {&consolePlayer, &consolePlayer},
    {&displayPlayer, 0 /*&displayPlayer*/}, // use R_SetViewPortPlayer() instead
#ifdef __CLIENT__
    {&mipmapping, 0},
    {&filterUI, 0},
    {0, 0}, // defResX
    {0, 0}, // defResY
#else
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
#endif
    {0, 0},
    {0, 0}, //{&mouseInverseY, &mouseInverseY},
#ifdef __CLIENT__
    {&levelFullBright, &levelFullBright},
#else
    {0, 0},
#endif
    {0, 0}, // &CmdReturnValue
#ifdef __CLIENT__
    {&gameReady, &gameReady},
#else
    {0, 0},
#endif
    {&isDedicated, 0},
    {&novideo, 0},
    {&defs.mobjs.count.num, 0},
    {&gotFrame, 0},
#ifdef __CLIENT__
    {&playback, 0},
#else
    {0, 0},
#endif
    {&defs.sounds.count.num, 0},
    {0, 0},
    {0, 0},
#ifdef __CLIENT__
    {&clientPaused, &clientPaused},
    {&weaponOffsetScaleY, &weaponOffsetScaleY},
#else
    {0, 0},
    {0, 0},
#endif
    {&gameDataFormat, &gameDataFormat},
#ifdef __CLIENT__
    {&gameDrawHUD, 0},
    {&symbolicEchoMode, &symbolicEchoMode},
    {&numTexUnits, 0},
    {&rendLightAttenuateFixedColormap, &rendLightAttenuateFixedColormap}
#else
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0}
#endif
};

#undef DD_GetInteger
/**
 * Get a 32-bit signed integer value.
 */
dint DD_GetInteger(dint ddvalue)
{
    switch(ddvalue)
    {
#ifdef __CLIENT__
    case DD_SHIFT_DOWN:
        return dint( ClientApp::inputSystem().shiftDown() );

    case DD_WINDOW_WIDTH:
        return DENG_GAMEVIEW_WIDTH;

    case DD_WINDOW_HEIGHT:
        return DENG_GAMEVIEW_HEIGHT;

    case DD_CURRENT_CLIENT_FINALE_ID:
        return Cl_CurrentFinale();

    case DD_DYNLIGHT_TEXTURE:
        return dint( GL_PrepareLSTexture(LST_DYNAMIC) );

    case DD_USING_HEAD_TRACKING:
        return vrCfg().mode() == VRConfig::OculusRift && vrCfg().oculusRift().isReady();
#endif

    case DD_MAP_MUSIC:
        if(App_WorldSystem().hasMap())
        {
            return Def_GetMusicNum(App_WorldSystem().map().mapInfo().gets("music").toUtf8().constData());
        }
        return -1;

    default: break;
    }

    if(ddvalue >= DD_LAST_VALUE || ddvalue <= DD_FIRST_VALUE)
    {
        return 0;
    }
    if(ddValues[ddvalue].readPtr == 0)
    {
        return 0;
    }
    return *ddValues[ddvalue].readPtr;
}

#undef DD_SetInteger
/**
 * Set a 32-bit signed integer value.
 */
void DD_SetInteger(dint ddvalue, dint parm)
{
    if(ddvalue <= DD_FIRST_VALUE || ddvalue >= DD_LAST_VALUE)
    {
        return;
    }

    if(ddValues[ddvalue].writePtr)
    {
        *ddValues[ddvalue].writePtr = parm;
    }
}

#undef DD_GetVariable
/**
 * Get a pointer to the value of a variable. Not all variables support
 * this. Added for 64-bit support.
 */
void *DD_GetVariable(dint ddvalue)
{
    static dint value;
    static ddouble valueD;
    static timespan_t valueT;

    switch(ddvalue)
    {
    case DD_GAME_EXPORTS:
        return &gx;

    case DD_POLYOBJ_COUNT:
        value = App_WorldSystem().hasMap()? App_WorldSystem().map().polyobjCount() : 0;
        return &value;

    case DD_MAP_MIN_X:
        valueD = App_WorldSystem().hasMap()? App_WorldSystem().map().bounds().minX : 0;
        return &valueD;

    case DD_MAP_MIN_Y:
        valueD = App_WorldSystem().hasMap()? App_WorldSystem().map().bounds().minY : 0;
        return &valueD;

    case DD_MAP_MAX_X:
        valueD = App_WorldSystem().hasMap()? App_WorldSystem().map().bounds().maxX : 0;
        return &valueD;

    case DD_MAP_MAX_Y:
        valueD = App_WorldSystem().hasMap()? App_WorldSystem().map().bounds().maxY : 0;
        return &valueD;

    /*case DD_CPLAYER_THRUST_MUL:
        return &cplrThrustMul;*/

    case DD_GRAVITY:
        valueD = App_WorldSystem().hasMap()? App_WorldSystem().map().gravity() : 0;
        return &valueD;

#ifdef __CLIENT__
    case DD_PSPRITE_OFFSET_X:
        return &pspOffset[0];

    case DD_PSPRITE_OFFSET_Y:
        return &pspOffset[1];

    case DD_PSPRITE_LIGHTLEVEL_MULTIPLIER:
        return &pspLightLevelMultiplier;

    case DD_TORCH_RED:
        return &torchColor.x;

    case DD_TORCH_GREEN:
        return &torchColor.y;

    case DD_TORCH_BLUE:
        return &torchColor.z;

    case DD_TORCH_ADDITIVE:
        return &torchAdditive;

# ifdef WIN32
    case DD_WINDOW_HANDLE:
        return ClientWindow::main().nativeHandle();
# endif
#endif

    // We have to separately calculate the 35 Hz ticks.
    case DD_GAMETIC:
        valueT = gameTime * TICSPERSEC;
        return &valueT;

    case DD_DEFS:
        return &defs;

    default: break;
    }

    if(ddvalue >= DD_LAST_VALUE || ddvalue <= DD_FIRST_VALUE)
    {
        return 0;
    }

    // Other values not supported.
    return ddValues[ddvalue].writePtr;
}

/**
 * Set the value of a variable. The pointer can point to any data, its
 * interpretation depends on the variable. Added for 64-bit support.
 */
#undef DD_SetVariable
void DD_SetVariable(dint ddvalue, void *parm)
{
    if(ddvalue <= DD_FIRST_VALUE || ddvalue >= DD_LAST_VALUE)
    {
        switch(ddvalue)
        {
        /*case DD_CPLAYER_THRUST_MUL:
            cplrThrustMul = *(dfloat*) parm;
            return;*/

        case DD_GRAVITY:
            if(App_WorldSystem().hasMap())
                App_WorldSystem().map().setGravity(*(coord_t*) parm);
            return;

#ifdef __CLIENT__
        case DD_PSPRITE_OFFSET_X:
            pspOffset[0] = *(dfloat *) parm;
            return;

        case DD_PSPRITE_OFFSET_Y:
            pspOffset[1] = *(dfloat *) parm;
            return;

        case DD_PSPRITE_LIGHTLEVEL_MULTIPLIER:
            pspLightLevelMultiplier = *(dfloat *) parm;
            return;

        case DD_TORCH_RED:
            torchColor.x = de::clamp(0.f, *((dfloat*) parm), 1.f);
            return;

        case DD_TORCH_GREEN:
            torchColor.y = de::clamp(0.f, *((dfloat*) parm), 1.f);
            return;

        case DD_TORCH_BLUE:
            torchColor.z = de::clamp(0.f, *((dfloat*) parm), 1.f);
            return;

        case DD_TORCH_ADDITIVE:
            torchAdditive = (*(dint*) parm)? true : false;
            break;
#endif

        default:
            break;
        }
    }
}

void DD_ReadGameHelp()
{
    LOG_AS("DD_ReadGameHelp");
    try
    {
        if(App_GameLoaded())
        {
            de::Uri uri(Path("$(App.DataPath)/$(GamePlugin.Name)/conhelp.txt"));
            Help_ReadStrings(App::fileSystem().find(uri.resolved()));
        }
    }
    catch(Error const &er)
    {
        LOG_RES_WARNING("") << er.asText();
    }
}

/// @note Part of the Doomsday public API.
fontschemeid_t DD_ParseFontSchemeName(char const *str)
{
#ifdef __CLIENT__
    try
    {
        FontScheme &scheme = App_ResourceSystem().fontScheme(str);
        if(!scheme.name().compareWithoutCase("System"))
        {
            return FS_SYSTEM;
        }
        if(!scheme.name().compareWithoutCase("Game"))
        {
            return FS_GAME;
        }
    }
    catch(ResourceSystem::UnknownSchemeError const &)
    {}
#endif
    qDebug() << "Unknown font scheme:" << String(str) << ", returning 'FS_INVALID'";
    return FS_INVALID;
}

String DD_MaterialSchemeNameForTextureScheme(String textureSchemeName)
{
    if(!textureSchemeName.compareWithoutCase("Textures"))
    {
        return "Textures";
    }
    if(!textureSchemeName.compareWithoutCase("Flats"))
    {
        return "Flats";
    }
    if(!textureSchemeName.compareWithoutCase("Sprites"))
    {
        return "Sprites";
    }
    if(!textureSchemeName.compareWithoutCase("System"))
    {
        return "System";
    }
    return "";
}

AutoStr *DD_MaterialSchemeNameForTextureScheme(ddstring_t const *textureSchemeName)
{
    if(!textureSchemeName)
    {
        return AutoStr_FromTextStd("");
    }
    else
    {
        QByteArray schemeNameUtf8 = DD_MaterialSchemeNameForTextureScheme(String(Str_Text(textureSchemeName))).toUtf8();
        return AutoStr_FromTextStd(schemeNameUtf8.constData());
    }
}

D_CMD(Load)
{
    DENG2_UNUSED(src);

    bool didLoadGame = false, didLoadResource = false;
    dint arg = 1;

    AutoStr *searchPath = AutoStr_NewStd();
    Str_Set(searchPath, argv[arg]);
    Str_Strip(searchPath);
    if(Str_IsEmpty(searchPath)) return false;

    F_FixSlashes(searchPath, searchPath);

    // Ignore attempts to load directories.
    if(Str_RAt(searchPath, 0) == '/')
    {
        LOG_WARNING("Directories cannot be \"loaded\" (only files and/or known games).");
        return true;
    }

    // Are we loading a game?
    try
    {
        Game &game = App_Games().byIdentityKey(Str_Text(searchPath));
        if(!game.allStartupFilesFound())
        {
            LOG_WARNING("Failed to locate all required startup resources:");
            Game::printFiles(game, FF_STARTUP);
            LOG_MSG("%s (%s) cannot be loaded.")
                    << game.title() << game.identityKey();
            return true;
        }

        BusyMode_FreezeGameForBusyMode();

        if(!App_ChangeGame(game))
        {
            return false;
        }

        didLoadGame = true;
        ++arg;
    }
    catch(Games::NotFoundError const &)
    {} // Ignore the error.

    // Try the resource locator.
    for(; arg < argc; ++arg)
    {
        try
        {
            String foundPath = App_FileSystem().findPath(de::Uri::fromNativePath(argv[arg], RC_PACKAGE),
                                                         RLF_MATCH_EXTENSION, App_ResourceClass(RC_PACKAGE));
            foundPath = App_BasePath() / foundPath; // Ensure the path is absolute.

            if(tryLoadFile(de::Uri(foundPath, RC_NULL)))
            {
                didLoadResource = true;
            }
        }
        catch(FS1::NotFoundError const &)
        {} // Ignore this error.
    }

    if(didLoadResource)
    {
        DD_UpdateEngineState();
    }

    return didLoadGame || didLoadResource;
}

/**
 * Attempt to load the (logical) resource indicated by the @a search term.
 *
 * @param path        Path to the resource to be loaded. Either a "real" file in
 *                    the local file system, or a "virtual" file.
 * @param baseOffset  Offset from the start of the file in bytes to begin.
 *
 * @return  @c true if the referenced resource was loaded.
 */
static File1 *tryLoadFile(de::Uri const &search, size_t baseOffset)
{
    try
    {
        FileHandle &hndl = App_FileSystem().openFile(search.path(), "rb", baseOffset, false /* no duplicates */);

        de::Uri foundFileUri = hndl.file().composeUri();
        LOG_VERBOSE("Loading \"%s\"...") << NativePath(foundFileUri.asText()).pretty().toUtf8().constData();

        App_FileSystem().index(hndl.file());

        return &hndl.file();
    }
    catch(FS1::NotFoundError const&)
    {
        if(App_FileSystem().accessFile(search))
        {
            // Must already be loaded.
            LOG_RES_XVERBOSE("\"%s\" already loaded") << NativePath(search.asText()).pretty();
        }
    }
    return nullptr;
}

/**
 * Attempt to unload the (logical) resource indicated by the @a search term.
 *
 * @return  @c true if the referenced resource was loaded and successfully unloaded.
 */
static bool tryUnloadFile(de::Uri const &search)
{
    try
    {
        File1 &file = App_FileSystem().find(search);
        de::Uri foundFileUri = file.composeUri();
        NativePath nativePath(foundFileUri.asText());

        // Do not attempt to unload a resource required by the current game.
        if(App_CurrentGame().isRequiredFile(file))
        {
            LOG_RES_NOTE("\"%s\" is required by the current game."
                         " Required game files cannot be unloaded in isolation.")
                    << nativePath.pretty();
            return false;
        }

        LOG_RES_VERBOSE("Unloading \"%s\"...") << nativePath.pretty();

        App_FileSystem().deindex(file);
        delete &file;

        return true;
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore.
    return false;
}

D_CMD(Unload)
{
    DENG2_UNUSED(src);

    BusyMode_FreezeGameForBusyMode();

    // No arguments; unload the current game if loaded.
    if(argc == 1)
    {
        if(!App_GameLoaded())
        {
            LOG_MSG("No game is currently loaded.");
            return true;
        }
        return App_ChangeGame(App_Games().nullGame());
    }

    AutoStr *searchPath = AutoStr_NewStd();
    Str_Set(searchPath, argv[1]);
    Str_Strip(searchPath);
    if(Str_IsEmpty(searchPath)) return false;

    F_FixSlashes(searchPath, searchPath);

    // Ignore attempts to unload directories.
    if(Str_RAt(searchPath, 0) == '/')
    {
        LOG_MSG("Directories cannot be \"unloaded\" (only files and/or known games).");
        return true;
    }

    // Unload the current game if specified.
    if(argc == 2)
    {
        try
        {
            Game &game = App_Games().byIdentityKey(Str_Text(searchPath));
            if(App_GameLoaded())
            {
                return App_ChangeGame(App_Games().nullGame());
            }

            LOG_MSG("%s is not currently loaded.") << game.identityKey();
            return true;
        }
        catch(Games::NotFoundError const &)
        {} // Ignore the error.
    }

    // Try the resource locator.
    bool didUnloadFiles = false;
    for(dint i = 1; i < argc; ++i)
    {
        try
        {
            String foundPath = App_FileSystem().findPath(de::Uri::fromNativePath(argv[1], RC_PACKAGE),
                                                         RLF_MATCH_EXTENSION, App_ResourceClass(RC_PACKAGE));
            foundPath = App_BasePath() / foundPath; // Ensure the path is absolute.

            if(tryUnloadFile(de::Uri(foundPath, RC_NULL)))
            {
                didUnloadFiles = true;
            }
        }
        catch(FS1::NotFoundError const&)
        {} // Ignore this error.
    }

    if(didUnloadFiles)
    {
        // A changed file list may alter the main lump directory.
        DD_UpdateEngineState();
    }

    return didUnloadFiles;
}

D_CMD(Reset)
{
    DENG2_UNUSED3(src, argc, argv);

    DD_UpdateEngineState();
    return true;
}

D_CMD(ReloadGame)
{
    DENG2_UNUSED3(src, argc, argv);

    if(!App_GameLoaded())
    {
        LOG_MSG("No game is presently loaded.");
        return true;
    }
    App_ChangeGame(App_CurrentGame(), true/* allow reload */);
    return true;
}

#ifdef __CLIENT__

D_CMD(CheckForUpdates)
{
    DENG2_UNUSED3(src, argc, argv);

    LOG_MSG("Checking for available updates...");
    ClientApp::updater().checkNow(Updater::OnlyShowResultIfUpdateAvailable);
    return true;
}

D_CMD(CheckForUpdatesAndNotify)
{
    DENG2_UNUSED3(src, argc, argv);

    LOG_MSG("Checking for available updates...");
    ClientApp::updater().checkNow(Updater::AlwaysShowResult);
    return true;
}

D_CMD(LastUpdated)
{
    DENG2_UNUSED3(src, argc, argv);

    ClientApp::updater().printLastUpdated();
    return true;
}

D_CMD(ShowUpdateSettings)
{
    DENG2_UNUSED3(src, argc, argv);

    ClientApp::updater().showSettings();
    return true;
}

#endif // __CLIENT__

D_CMD(Version)
{
    DENG2_UNUSED3(src, argc, argv);

    LOG_NOTE(_E(D) DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_FULLTEXT);
    LOG_MSG(_E(l) "Homepage: " _E(.) _E(i) DOOMSDAY_HOMEURL _E(.)
            "\n" _E(l) "Project: " _E(.) _E(i) DENGPROJECT_HOMEURL);
    
    // Print the version info of the current game if loaded.
    if(App_GameLoaded())
    {
        LOG_MSG(_E(l) "Game: " _E(.) "%s") << (char const *) gx.GetVariable(DD_PLUGIN_VERSION_LONG);
    }

    // Additional information for developers.
    Version const ver;
    if(!ver.gitDescription.isEmpty())
    {
        LOGDEV_MSG(_E(l) "Git revision: " _E(.) "%s") << ver.gitDescription;
    }
    return true;
}

D_CMD(Quit)
{
    DENG2_UNUSED2(src, argc);

#ifdef __CLIENT__
    if(DownloadDialog::isDownloadInProgress())
    {
        LOG_WARNING("Cannot quit while downloading an update");
        ClientWindow::main().taskBar().openAndPauseGame();
        DownloadDialog::currentDownload().open();
        return false;
    }
#endif

    if(argv[0][4] == '!' || isDedicated || !App_GameLoaded() ||
       gx.TryShutdown == 0)
    {
        // No questions asked.
        Sys_Quit();
        return true; // Never reached.
    }

#ifdef __CLIENT__
    // Dismiss the taskbar if it happens to be open, we are expecting
    // the game to handle this from now on.
    ClientWindow::main().taskBar().close();
#endif

    // Defer this decision to the loaded game.
    return gx.TryShutdown();
}

#ifdef _DEBUG
D_CMD(DebugError)
{
    DENG2_UNUSED3(src, argv, argc);

    App_Error("Fatal error!\n");
    return true;
}
#endif

D_CMD(Help)
{
    DENG2_UNUSED3(src, argc, argv);

    /*
#ifdef __CLIENT__
    char actKeyName[40];
    strcpy(actKeyName, B_ShortNameForKey(consoleActiveKey));
    actKeyName[0] = toupper(actKeyName[0]);
#endif
*/

    LOG_SCR_NOTE(_E(b) DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_TEXT " Console");

#define TABBED(A, B) "\n" _E(Ta) _E(b) "  " << A << " " _E(.) _E(Tb) << B

#ifdef __CLIENT__
    LOG_SCR_MSG(_E(D) "Keys:" _E(.))
            << TABBED(DENG2_CHAR_SHIFT_KEY "Esc", "Open the taskbar and console")
            << TABBED("Tab", "Autocomplete the word at the cursor")
            << TABBED(DENG2_CHAR_UP_DOWN_ARROW, "Move backwards/forwards through the input command history, or up/down one line inside a multi-line command")
            << TABBED("PgUp/Dn", "Scroll up/down in the history, or expand the history to full height")
            << TABBED(DENG2_CHAR_SHIFT_KEY "PgUp/Dn", "Jump to the top/bottom of the history")
            << TABBED("Home", "Move the cursor to the start of the command line")
            << TABBED("End", "Move the cursor to the end of the command line")
            << TABBED(DENG2_CHAR_CONTROL_KEY "K", "Clear everything on the line right of the cursor position")
            << TABBED("F5", "Clear the console message history");
#endif
    LOG_SCR_MSG(_E(D) "Getting started:");
    LOG_SCR_MSG("  " _E(>) "Enter " _E(b) "help (what)" _E(.) " for information about " _E(l) "(what)");
    LOG_SCR_MSG("  " _E(>) "Enter " _E(b) "listcmds" _E(.) " to list available commands");
    LOG_SCR_MSG("  " _E(>) "Enter " _E(b) "listgames" _E(.) " to list installed games and their status");
    LOG_SCR_MSG("  " _E(>) "Enter " _E(b) "listvars" _E(.) " to list available variables");

#undef TABBED

    return true;
}

static void printHelpAbout(char const *query)
{
    // Try the console commands first.
    if(ccmd_t *ccmd = Con_FindCommand(query))
    {
        LOG_SCR_MSG(_E(b) "%s" _E(.) " (Command)") << ccmd->name;

        HelpId help = DH_Find(ccmd->name);
        if(char const *description = DH_GetString(help, HST_DESCRIPTION))
        {
            LOG_SCR_MSG("") << description;
        }

        Con_PrintCommandUsage(ccmd);  // For all overloaded variants.

        // Any extra info?
        if(char const *info = DH_GetString(help, HST_INFO))
        {
            LOG_SCR_MSG("  " _E(>) _E(l)) << info;
        }
        return;
    }

    if(cvar_t *var = Con_FindVariable(query))
    {
        AutoStr *path = CVar_ComposePath(var);
        LOG_SCR_MSG(_E(b) "%s" _E(.) " (Variable)") << Str_Text(path);

        HelpId help = DH_Find(Str_Text(path));
        if(char const *description = DH_GetString(help, HST_DESCRIPTION))
        {
            LOG_SCR_MSG("") << description;
        }
        return;
    }

    if(calias_t *calias = Con_FindAlias(query))
    {
        LOG_SCR_MSG(_E(b) "%s" _E(.) " alias of:\n")
                << calias->name << calias->command;
        return;
    }

    // Perhaps a game?
    try
    {
        Game &game = App_Games().byIdentityKey(query);
        LOG_SCR_MSG(_E(b) "%s" _E(.) " (IdentityKey)") << game.identityKey();

        LOG_SCR_MSG("Unique identifier of the " _E(b) "%s" _E(.) " game mode.") << game.title();
        LOG_SCR_MSG("An 'IdentityKey' is used when referencing a game unambiguously from the console and on the command line.");
        LOG_SCR_MSG(_E(D) "Related commands:");
        LOG_SCR_MSG("  " _E(>) "Enter " _E(b) "inspectgame %s" _E(.) " for information and status of this game") << game.identityKey();
        LOG_SCR_MSG("  " _E(>) "Enter " _E(b) "listgames" _E(.) " to list all installed games and their status");
        LOG_SCR_MSG("  " _E(>) "Enter " _E(b) "load %s" _E(.) " to load the " _E(l) "%s" _E(.) " game mode") << game.identityKey() << game.title();
        return;
    }
    catch(Games::NotFoundError const &)
    {}  // Ignore this error.

    LOG_SCR_NOTE("There is no help about '%s'") << query;
}

D_CMD(HelpWhat)
{
    DENG2_UNUSED2(argc, src);

    if(!String(argv[1]).compareWithoutCase("(what)"))
    {
        LOG_SCR_MSG("You've got to be kidding!");
        return true;
    }

    printHelpAbout(argv[1]);
    return true;
}

#ifdef __CLIENT__
D_CMD(Clear)
{
    DENG2_UNUSED3(src, argc, argv);

    ClientWindow::main().console().clearLog();
    return true;
}
#endif

static void consoleRegister()
{
    C_VAR_CHARPTR("file-startup", &startupFiles, 0, 0, 0);

    C_CMD("help",           "",     Help);
    C_CMD("help",           "s",    HelpWhat);
    C_CMD("version",        "",     Version);
    C_CMD("quit",           "",     Quit);
    C_CMD("quit!",          "",     Quit);
    C_CMD("load",           "s*",   Load);
    C_CMD("reset",          "",     Reset);
    C_CMD("reload",         "",     ReloadGame);
    C_CMD("unload",         "*",    Unload);
    C_CMD("listmobjtypes",  "",     ListMobjs);
    C_CMD("write",          "s",    WriteConsole);

#ifdef DENG2_DEBUG
    C_CMD("fatalerror",     nullptr,   DebugError);
#endif

    DD_RegisterLoop();
    FS1::consoleRegister();
    Con_Register();
    Games::consoleRegister();
    DH_Register();
    S_Register();

#ifdef __CLIENT__
    C_CMD("clear",           "", Clear);
    C_CMD("update",          "", CheckForUpdates);
    C_CMD("updateandnotify", "", CheckForUpdatesAndNotify);
    C_CMD("updatesettings",  "", ShowUpdateSettings);
    C_CMD("lastupdated",     "", LastUpdated);

    C_CMD_FLAGS("conclose",       "",     OpenClose,    CMDF_NO_DEDICATED);
    C_CMD_FLAGS("conopen",        "",     OpenClose,    CMDF_NO_DEDICATED);
    C_CMD_FLAGS("contoggle",      "",     OpenClose,    CMDF_NO_DEDICATED);
    C_CMD      ("taskbar",        "",     TaskBar);
    C_CMD      ("tutorial",       "",     Tutorial);

    /// @todo Move to UI module.
    Con_TransitionRegister();

    InputSystem::consoleRegister();
    SBE_Register();  // for bias editor
    RenderSystem::consoleRegister();
    GL_Register();
    UI_Register();
    Demo_Register();
    P_ConsoleRegister();
    I_Register();
#endif

    ResourceSystem::consoleRegister();
    Net_Register();
    WorldSystem::consoleRegister();
    InFineSystem::consoleRegister();
}

// dd_loop.c
DENG_EXTERN_C dd_bool DD_IsSharpTick(void);

// net_main.c
DENG_EXTERN_C void Net_SendPacket(dint to_player, dint type, const void* data, size_t length);

#undef R_SetupMap
DENG_EXTERN_C void R_SetupMap(dint mode, dint flags)
{
    DENG2_UNUSED2(mode, flags);

    if(!App_WorldSystem().hasMap()) return; // Huh?

    // Perform map setup again. Its possible that after loading we now
    // have more HOMs to fix, etc..
    Map &map = App_WorldSystem().map();

#ifdef __CLIENT__
    map.initSkyFix();
#endif

#ifdef __CLIENT__
    // Update all sectors.
    /// @todo Refactor away.
    map.forAllSectors([] (Sector &sector)
    {
        sector.forAllSides([] (LineSide &side)
        {
            side.fixMissingMaterials();
            return LoopContinue;
        });
        return LoopContinue;
    });
#endif

    // Re-initialize polyobjs.
    /// @todo Still necessary?
    map.initPolyobjs();

    // Reset the timer so that it will appear that no time has passed.
    DD_ResetTimer();
}

// sys_system.c
DENG_EXTERN_C void Sys_Quit();

DENG_DECLARE_API(Base) =
{
    { DE_API_BASE },

    Sys_Quit,
    DD_GetInteger,
    DD_SetInteger,
    DD_GetVariable,
    DD_SetVariable,
    DD_DefineGame,
    DD_GameIdForKey,
    DD_AddGameResource,
    DD_GameInfo,
    DD_IsSharpTick,
    Net_SendPacket,
    R_SetupMap
};
