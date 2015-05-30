/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2010-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_APP_H
#define LIBDENG2_APP_H

#include "../libcore.h"
#include "../Clock"
#include "../CommandLine"
#include "../NativePath"
#include "../LogBuffer"
#include "../LogFilter"
#include "../System"
#include "../FileSystem"
#include "../PackageLoader"
#include "../ScriptSystem"
#include "../Module"
#include "../Config"
#include "../UnixInfo"

/**
 * Macro for conveniently accessing the de::App singleton instance.
 */
#define DENG2_APP   (&de::App::app())

#define DENG2_ASSERT_IN_MAIN_THREAD()   DENG2_ASSERT(de::App::inMainThread())

namespace de {

class Archive;

namespace game { class Game; }

/**
 * Represents the application and its subsystems. This is the common
 * denominator (and abstract base class) for GUI and non-GUI apps. de::App is
 * not usable on its own; instead you must use one of the derived variants.
 * @ingroup core
 *
 * @see GuiApp, TextApp
 */
class DENG2_PUBLIC App : DENG2_OBSERVES(Clock, TimeChange)
{
public:
    enum SubsystemInitFlag {
        DefaultSubsystems     = 0x0,
        DisablePlugins        = 0x1,
        DisablePersistentData = 0x2
    };
    Q_DECLARE_FLAGS(SubsystemInitFlags, SubsystemInitFlag)

    /// Attempting to access persistent data when it has been disabled at init.
    /// @ingroup errors
    DENG2_ERROR(PersistentDataNotAvailable);

    /// Asset with given identifier was not found. @ingroup errors
    DENG2_ERROR(AssetNotFoundError);

    /**
     * Notified when application startup has been fully completed.
     */
    DENG2_DEFINE_AUDIENCE2(StartupComplete, void appStartupCompleted())

    /**
     * Notified before the current game is unloaded.
     */
    DENG2_DEFINE_AUDIENCE2(GameUnload, void aboutToUnloadGame(game::Game const &gameBeingUnloaded))

    /**
     * Notified after the current game has been changed.
     */
    DENG2_DEFINE_AUDIENCE2(GameChange, void currentGameChanged(game::Game const &newGame))

public:
    /**
     * Construct an App instance. The application will not be fully usable
     * until initSubsystems() has been called -- you should call
     * initSubsystems() as soon as possible after construction. Never throws an
     * exception.
     *
     * @param appFilePath  Path of the application binary.
     * @param args         Arguments.
     */
    App(NativePath const &appFilePath, QStringList args);

    virtual ~App();

    /**
     * Defines metadata about the application.
     *
     * @param appName     Name of the application, as presented to humans.
     * @param appVersion  Version of the application.
     * @param orgName     Name of the author/organization.
     * @param orgDomain   Network domain of the author/organization.
     */
    virtual void setMetadata(String const &orgName, String const &orgDomain,
                             String const &appName, String const &appVersion) = 0;

    /**
     * Add a new package to be loaded at initialization time. Call this before
     * initSubsystems().
     *
     * @param identifier  Package identifier.
     */
    void addInitPackage(String const &identifier);

    /**
     * Sets the path of the configuration script that will be automatically run if needed
     * during application launch. The script governs the contents of the special
     * persistent Config module. @see Config
     *
     * This method must be called before initSubsystems().
     *
     * @param path  Location of the @em Config.de script file. The default path of the
     *              script is "/modules/Config.de".
     */
    void setConfigScript(Path const &path);

    /**
     * Sets the name of the application. Derived classes should call this from their
     * implementation of setMetadata().
     *
     * @param appName  Application name. Defaults to "Doomsday Engine".
     */
    void setName(String const &appName);

    /**
     * Sets the Unix-style home folder name. For instance, ".doomsday" could be used.
     *
     * @param name  Name of the (usually hidden) user-specific home data folder.
     */
    void setUnixHomeFolderName(String const &name);

    String unixHomeFolderName() const;

    /**
     * Returns the home folder name without the possible dot in the beginning.
     */
    String unixEtcFolderName() const;

    /**
     * Sets a callback to be called when an uncaught exception occurs.
     */
    void setTerminateFunc(void (*func)(char const *msg));

    /**
     * Finishes App construction by initializing all the application's
     * subsystems. This includes Config and FS. Has to be called manually in
     * the application's initialization routine. An exception will be thrown if
     * initialization cannot be successfully completed.
     *
     * @param flags  How to/which subsystems to initialize.
     */
    virtual void initSubsystems(SubsystemInitFlags flags = DefaultSubsystems);

    /**
     * Adds a system to the application. The order of systems is preserved; the
     * system added last will be notified of time changes last and will receive
     * input events last (if others don't eat them).
     *
     * @param system  System. Ownership kept by caller. The caller is
     *                responsible for making sure the system has been
     *                initialized properly.
     */
    void addSystem(System &system);

    /**
     * Removes a system from the application.
     *
     * @param system  System to remove.
     */
    void removeSystem(System &system);

    /**
     * Determines if an instance of App currently exists.
     */
    static bool appExists();

    static App &app();

    static LogFilter &logFilter();

    /**
     * Returns the command line used to start the application.
     */
    static CommandLine &commandLine();

    /**
     * Returns the absolute native path of the application executable.
     */
    static NativePath executablePath();

#ifdef MACOSX
    /**
     * Returns the native path of the application bundle contents.
     */
    NativePath nativeAppContentsPath();
#endif

    /**
     * Returns the native path of the data base folder.
     *
     * The base folder is the location where all the common data files are located, e.g.,
     * /usr/share/doomsday on Linux.
     */
    NativePath nativeBasePath();

    /**
     * Returns the native path of where to load binaries (plugins). This
     * is where "/bin" points to.
     */
    NativePath nativePluginBinaryPath();

    /**
     * Returns the native path where user-specific runtime files should be
     * placed (this is where "/home" points to). The user can override the
     * location using the @em -userdir command line option.
     */
    NativePath nativeHomePath();

    /**
     * Returns the archive for storing persistent engine state into. Typically
     * the contents are updated when subsystems are being shut down. When the
     * file system is being shut down, the <code>/home/persist.pack</code>
     * package is written to disk.
     *
     * @return Persistent data archive.
     */
    static Archive const &persistentData();

    /**
     * Returns the persistent data as a mutable archive. Accessing the entries in 
     * a mutable archive will automatically update their timestamps.
     *
     * @return Persistent data archive (allowing changes).
     */
    static Archive &mutablePersistentData();
    
    static bool hasPersistentData();

    /**
     * Returns the application's current native working directory.
     */
    static NativePath currentWorkPath();

    /**
     * Changes the application's current native working directory.
     *
     * @param cwd  New working directory for the application.
     *
     * @return @c true, if the current working directory was changed,
     * otherwise @c false.
     */
    static bool setCurrentWorkPath(NativePath const &cwd);

    /**
     * Returns the application's file system.
     */
    static FileSystem &fileSystem();

    /**
     * Returns the root folder of the file system.
     */
    static Folder &rootFolder();

    /**
     * Returns the /home folder.
     */
    static Folder &homeFolder();

    /**
     * Returns the application's package loader.
     */
    static PackageLoader &packageLoader();

    /**
     * Convenience method for finding all files matching a certain name or partial path
     * from all loaded packages.
     *
     * @param partialPath  File name or partial path.
     * @param files        Resulting list of found files.
     *
     * @return Number of files found.
     */
    static int findInPackages(String const &partialPath, FS::FoundFiles &files);

    /**
     * Checks if an asset exists.
     *
     * @param identifier  Identifier.
     *
     * @return @c true, if assetInfo() can be called.
     */
    static bool assetExists(String const &identifier);

    /**
     * Retrieves the namespace of an asset.
     *
     * @param identifier  Identifier.
     *
     * @return Asset namespace accessor.
     */
    static Package::Asset asset(String const &identifier);

    /**
     * Returns the application's script system.
     */
    static ScriptSystem &scriptSystem();

    /**
     * Returns the configuration.
     */
    static Config &config();

    /**
     * Returns a configuration variable.
     *
     * @param name  Name of the variable.
     *
     * @return Variable.
     */
    static Variable &config(String const &name);

    /**
     * Returns the Unix system-level configuration preferences.
     */
    static UnixInfo &unixInfo();

    /**
     * Starts the application's main loop.
     *
     * @return Return code after the loop exits.
     */
    virtual int execLoop() = 0;

    /**
     * Stops the application's main loop.
     *
     * @param code  Return code from the loop.
     */
    virtual void stopLoop(int code) = 0;

    /**
     * Requests engine shutdown by calling the specified termination callback
     * (see setTerminateFunc()). Called when an exception is caught at the
     * de::App level, at which point there is no way to gracefully handle it
     * and the application has to be shut down.
     *
     * This should not be called directly. From C++ code, one should throw an
     * exception in unrecoverable error situations. From C code, one should
     * call the App_FatalError() function.
     *
     * @param message  Error message to be shown to the user.
     */
    void handleUncaughtException(String message);

    /**
     * Events received from the operating system should be passed here; the
     * application will make sure all subsystems get a chance to process them.
     */
    virtual bool processEvent(Event const &);

    /**
     * Informs all the subsystems about advancement of time. Subsystems will be
     * notified in the order they were added with addSystem(). This will be
     * automatically called by the application clock when time changes.
     */
    void timeChanged(Clock const &);

    /**
     * Sets the currently active game. App does not take ownership of the
     * provided Game instance.
     *
     * @param game  Game instance. Must not be deleted until another Game is
     *              used as the current one.
     */
    void setGame(game::Game &game);

    /**
     * Returns the currently active game.
     */
    static game::Game &game();

public:
    /**
     * Determines if the currently executing thread is the application's main
     * (UI) thread.
     */
    static bool inMainThread();

protected:
    /**
     * Returns the native path of the directory where the application can store
     * user-specific data. This is usually not the same as the user's native
     * home folder.
     *
     * @return Application data path.
     */
    virtual NativePath appDataPath() const = 0;

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(App::SubsystemInitFlags)

} // namespace de

#endif // LIBDENG2_APP_H
