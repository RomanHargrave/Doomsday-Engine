/** @file sys_system.cpp  Abstract interfaces for platform specific services.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#ifdef WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <process.h>
#endif

#include <signal.h>
#ifdef MACOSX
#  include <QDir>
#endif
#ifdef WIN32
#  include <QSettings>
#endif

#include <de/App>
#include <de/Loop>
#include "de_console.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_misc.h"
#ifdef __CLIENT__
#  include "clientapp.h"
#endif

#include "dd_main.h"
#include "dd_loop.h"
#ifdef __CLIENT__
#  include "gl/gl_main.h"
#endif
#include "ui/nativeui.h"
#include "network/net_main.h"
#include "network/net_buf.h"
#include "audio/s_main.h"

#if defined(WIN32) && !defined(_DEBUG)
#  define DENG_CATCH_SIGNALS
#endif

int novideo;                // if true, stay in text mode for debugging

static dd_bool appShutdown = false; ///< Set to true when we should exit (normally).

#ifdef DENG_CATCH_SIGNALS
/**
 * Borrowed from Lee Killough.
 */
static void C_DECL handler(int s)
{
    signal(s, SIG_IGN);  // Ignore future instances of this signal.

    App_Error(s==SIGSEGV ? "Segmentation Violation\n" :
              s==SIGINT  ? "Interrupted by User\n" :
              s==SIGILL  ? "Illegal Instruction\n" :
              s==SIGFPE  ? "Floating Point Exception\n" :
              s==SIGTERM ? "Killed\n" : "Terminated by signal\n");
}
#endif

/**
 * Initialize platform level services.
 *
 * \note This must be called from the main thread due to issues with the devices
 * we use via the WINAPI, MCI (cdaudio, mixer etc) on the WIN32 platform.
 */
void Sys_Init()
{
    de::Time begunAt;

    LOG_VERBOSE("Setting up platform state...");

    LOG_AUDIO_VERBOSE("Initializing Audio subsystem...");
    S_Init();

#ifdef DENG_CATCH_SIGNALS
    // Register handler for abnormal situations (in release build).
    signal(SIGSEGV, handler);
    signal(SIGTERM, handler);
    signal(SIGILL, handler);
    signal(SIGFPE, handler);
    signal(SIGILL, handler);
    signal(SIGABRT, handler);
#endif

#ifndef WIN32
    // We are not worried about broken pipes. When a TCP connection closes,
    // we prefer to receive an error code instead of a signal.
    signal(SIGPIPE, SIG_IGN);
#endif

    LOG_NET_VERBOSE("Initializing Network subsystem...");
    N_Init();

    LOGDEV_VERBOSE("Sys_Init completed in %.2f seconds") << begunAt.since();
}

bool Sys_IsShuttingDown()
{
    return CPP_BOOL(appShutdown);
}

/**
 * Return to default system state.
 */
void Sys_Shutdown()
{
    // We are now shutting down.
    appShutdown = true;

    // Time to unload *everything*.
    if(App_GameLoaded())
        Con_Execute(CMDS_DDAY, "unload", true, false);

    Net_Shutdown();
    // Let's shut down sound first, so Windows' HD-hogging doesn't jam
    // the MUS player (would produce horrible bursts of notes).
    S_Shutdown();
#ifdef __CLIENT__
    GL_Shutdown();
    ClientApp::inputSystem().clearEvents();
#endif

    App_ClearGames();
}

static int showCriticalMessage(const char* msg)
{
    // This is going to be the end, I'm afraid.
    de::Loop::get().stop();

    Sys_MessageBox(MBT_WARNING, DOOMSDAY_NICENAME, msg, 0);
    return 0;

#if 0
#ifdef WIN32
#ifdef UNICODE
    wchar_t buf[256];
#else
    char buf[256];
#endif
    int ret;
    Window *wnd = Window::main();
    DENG_ASSERT(wnd != 0);
    HWND hWnd = (HWND) wnd->nativeHandle();

    if(!hWnd)
    {
        DD_Win32_SuspendMessagePump(true);
        MessageBox(HWND_DESKTOP, TEXT("Sys_CriticalMessage: Main window not available."),
                   NULL, MB_ICONERROR | MB_OK);
        DD_Win32_SuspendMessagePump(false);
        return false;
    }

    ShowCursor(TRUE);
    ShowCursor(TRUE);
    DD_Win32_SuspendMessagePump(true);
    GetWindowText(hWnd, buf, 255);
    ret = (MessageBox(hWnd, WIN_STRING(msg), buf, MB_OK | MB_ICONEXCLAMATION) == IDYES);
    DD_Win32_SuspendMessagePump(false);
    ShowCursor(FALSE);
    ShowCursor(FALSE);
    return ret;
#else
    fprintf(stderr, "--- %s\n", msg);
    return 0;
#endif
#endif
}

int Sys_CriticalMessage(const char* msg)
{
    return showCriticalMessage(msg);
}

int Sys_CriticalMessagef(const char* format, ...)
{
    static const char* unknownMsg = "Unknown critical issue occured.";
    const size_t BUF_SIZE = 655365;
    const char* msg;
    char* buf = 0;
    va_list args;
    int result;

    if(format && format[0])
    {
        va_start(args, format);
        buf = (char*) calloc(1, BUF_SIZE);
        dd_vsnprintf(buf, BUF_SIZE, format, args);
        msg = buf;
        va_end(args);
    }
    else
    {
        msg = unknownMsg;
    }

    result = showCriticalMessage(msg);

    if(buf) free(buf);
    return result;
}

void Sys_Sleep(int millisecs)
{
    /*
#ifdef WIN32
    Sleep(millisecs);
#endif
*/
    Thread_Sleep(millisecs);
}

void Sys_BlockUntilRealTime(uint realTimeMs)
{
    uint remaining = realTimeMs - Timer_RealMilliseconds();
    if(remaining > 50)
    {
        // Target time is in the past; or the caller is attempting to wait for
        // too long a time.
        return;
    }

    while(Timer_RealMilliseconds() < realTimeMs)
    {
        // Do nothing; don't yield execution. We want to exit here at the
        // precise right moment.
    }
}

void Sys_HideMouseCursor()
{
#ifdef WIN32
    if(novideo) return;

    ShowCursor(FALSE);
#else
    // The cursor is controlled using Qt in Canvas.
#endif
}

de::NativePath Sys_SteamBasePath()
{
#ifdef WIN32
    // The path to Steam can be queried from the registry.
    {
    QSettings st("HKEY_CURRENT_USER\\Software\\Valve\\Steam\\", QSettings::NativeFormat);
    de::String path = st.value("SteamPath").toString();
    if(!path.isEmpty()) return path;
    }

    {
    QSettings st("HKEY_LOCAL_MACHINE\\Software\\Valve\\Steam\\", QSettings::NativeFormat);
    de::String path = st.value("InstallPath").toString();
    if(!path.isEmpty()) return path;
    }
#elif MACOSX
    return de::NativePath(QDir::homePath()) / "Library/Application Support/Steam/";
#endif
    /// @todo Where are steam apps located on Ubuntu?
    return "";
}

/**
 * Called when Doomsday should quit (will be deferred until convenient).
 */
DENG_EXTERN_C void Sys_Quit(void)
{
    if(BusyMode_Active())
    {
        // The busy worker is running; we cannot just stop it abruptly.
        Sys_MessageBox2(MBT_WARNING, DOOMSDAY_NICENAME, "Cannot quit while in busy mode.",
                        "Try again later after the current operation has finished.", 0);
        return;
    }

    appShutdown = true;

    // It's time to stop the main loop.
    DENG2_APP->stopLoop(DD_GameLoopExitCode());
}
