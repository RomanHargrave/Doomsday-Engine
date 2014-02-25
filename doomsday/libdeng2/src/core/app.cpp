/*
 * The Doomsday Engine Project -- libdeng2
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

#include "de/Animation"
#include "de/App"
#include "de/ArchiveFeed"
#include "de/ArrayValue"
#include "de/Block"
#include "de/DictionaryValue"
#include "de/DirectoryFeed"
#include "de/Log"
#include "de/LogBuffer"
#include "de/LogFilter"
#include "de/Module"
#include "de/NumberValue"
#include "de/PackageFolder"
#include "de/Record"
#include "de/ScriptSystem"
#include "de/Version"
#include "de/Version"
#include "de/Writer"
#include "de/ZipArchive"
#include "de/game/Game"
#include "de/math.h"

#include <QDir>
#include <QThread>

namespace de {

static App *singletonApp;

DENG2_PIMPL(App)
{
    QThread *mainThread;

    CommandLine cmdLine;

    LogFilter logFilter;
    LogBuffer logBuffer;

    /// Path of the application executable.
    NativePath appPath;

    NativePath cachedBasePath;
    NativePath cachedPluginBinaryPath;
    NativePath cachedHomePath;

    /// Primary (wall) clock.
    Clock clock;

    /// Subsystems (not owned).
    QList<System *> systems;

    FileSystem fs;
    ScriptSystem scriptSys;
    Record appModule;

    /// Archive where persistent data should be stored. Written to /home/persist.pack.
    /// The archive is owned by the file system.
    Archive *persistentData;

    QScopedPointer<UnixInfo> unixInfo;

    /// The configuration.
    Config *config;

    game::Game *currentGame;

    void (*terminateFunc)(char const *);

    /**
     * Delegates game change notifications to scripts.
     */
    class GameChangeScriptAudience : DENG2_OBSERVES(App, GameChange)
    {
    public:
        void currentGameChanged(game::Game const &newGame)
        {
            ArrayValue args;
            args << DictionaryValue() << TextValue(newGame.id());
            App::scriptSystem().nativeModule("App")["audienceForGameChange"]
                    .value<ArrayValue>().callElements(args);
        }
    };

    GameChangeScriptAudience scriptAudienceForGameChange;

    Instance(Public *a, QStringList args)
        : Base(a), cmdLine(args), persistentData(0), config(0),
          currentGame(0), terminateFunc(0)
    {
        singletonApp = a;
        mainThread = QThread::currentThread();

        logBuffer.setEntryFilter(&logFilter);

        Clock::setAppClock(&clock);
        Animation::setClock(&clock);

        // Built-in systems.
        systems.append(&fs);
        systems.append(&scriptSys);

        // Native App module.
        appModule.addArray("audienceForGameChange"); // game change observers
        scriptSys.addNativeModule("App", appModule);

        self.audienceForGameChange += scriptAudienceForGameChange;
    }

    ~Instance()
    {
        clock.audienceForTimeChange -= self;

        // Update the log filter in the persistent configuration.
        Record *filter = new Record;
        logFilter.write(*filter);
        config->names().add("log.filter", filter);

        delete config;
        Clock::setAppClock(0);
    }

    void initFileSystem(bool allowPlugins)
    {
        Folder &binFolder = fs.makeFolder("/bin");

        // Initialize the built-in folders. This hooks up the default native
        // directories into the appropriate places in the file system.
        // All of these are in read-only mode.
#ifdef MACOSX
        NativePath appDir = appPath.fileNamePath();
        binFolder.attach(new DirectoryFeed(appDir));
        fs.makeFolder("/data").attach(new DirectoryFeed(self.nativeBasePath()));
        fs.makeFolder("/modules").attach(new DirectoryFeed(self.nativeBasePath() / "modules"));

#elif WIN32
        NativePath appDir = appPath.fileNamePath();
        fs.makeFolder("/data").attach(new DirectoryFeed(appDir / "..\\data"));
        fs.makeFolder("/modules").attach(new DirectoryFeed(appDir / "..\\modules"));

#else // UNIX
        fs.makeFolder("/data").attach(new DirectoryFeed(self.nativeBasePath() / "data"));
        fs.makeFolder("/modules").attach(new DirectoryFeed(self.nativeBasePath() / "modules"));
#endif

        if(allowPlugins)
        {
            binFolder.attach(new DirectoryFeed(self.nativePluginBinaryPath()));
        }

        // User's home folder.
        fs.makeFolder("/home").attach(new DirectoryFeed(self.nativeHomePath(),
            DirectoryFeed::AllowWrite | DirectoryFeed::CreateIfMissing));

        // Populate the file system.
        fs.refresh();
    }

    void setLogLevelAccordingToOptions()
    {
        // Override the log message level.
        if(cmdLine.has("-loglevel") || cmdLine.has("-verbose") || cmdLine.has("-v") ||
           cmdLine.has("-vv") || cmdLine.has("-vvv"))
        {
            LogEntry::Level level = LogEntry::Message;
            try
            {
                int pos;
                if((pos = cmdLine.check("-loglevel", 1)) > 0)
                {
                    level = LogEntry::textToLevel(cmdLine.at(pos + 1));
                }
            }
            catch(Error const &er)
            {
                qWarning("%s", er.asText().toLatin1().constData());
            }

            // Aliases have not been defined at this point, so check all variants.
            level = LogEntry::Level(level
                                    - cmdLine.has("-verbose")
                                    - cmdLine.has("-v")
                                    - cmdLine.has("-vv") * 2
                                    - cmdLine.has("-vvv") * 3);

            if(level < LogEntry::XVerbose)
            {
                // Even more verbosity requested, so enable dev messages, too.
                logFilter.setAllowDev(LogEntry::AllDomains, true);
                level = LogEntry::XVerbose;
            }

            logFilter.setMinLevel(LogEntry::AllDomains, level);            
        }

        // Enable developer messages across the board?
        if(cmdLine.has("-devlog"))
        {
            logFilter.setAllowDev(LogEntry::AllDomains, true);
        }
        if(cmdLine.has("-nodevlog"))
        {
            logFilter.setAllowDev(LogEntry::AllDomains, false);
        }
    }
};

App::App(NativePath const &appFilePath, QStringList args)
    : d(new Instance(this, args))
{
    d->unixInfo.reset(new UnixInfo);

    // Global time source for animations.
    Animation::setClock(&d->clock);

    // This instance of LogBuffer is used globally.
    LogBuffer::setAppBuffer(d->logBuffer);

    // Do not flush the log buffer until we've found out where messages should
    // be flushed (Config.log.file).
    d->logBuffer.enableFlushing(false);

    // The log filter will be read from Config, but until that time we can use
    // the options from the command line.
    d->setLogLevelAccordingToOptions();

    d->appPath = appFilePath;

    LOG_NOTE("Application path: ") << d->appPath;

#ifdef MACOSX
    // When the application is started through Finder, we get a special command
    // line argument. The working directory needs to be changed.
    if(d->cmdLine.count() >= 2 && d->cmdLine.at(1).beginsWith("-psn"))
    {
        DirectoryFeed::changeWorkingDir(d->cmdLine.at(0).fileNamePath() + "/..");
    }
#endif
}

App::~App()
{
    LOG_AS("~App");

    d.reset();

    singletonApp = 0;
}

void App::setTerminateFunc(void (*func)(char const *))
{
    d->terminateFunc = func;
}

void App::handleUncaughtException(String message)
{
    LOG_CRITICAL(message);

    if(d->terminateFunc) d->terminateFunc(message.toUtf8().constData());
}

bool App::processEvent(Event const &ev)
{
    foreach(System *sys, d->systems)
    {
        if(sys->behavior() & System::ReceivesInputEvents)
        {
            if(sys->processEvent(ev))
                return true;
        }
    }
    return false;
}

void App::timeChanged(Clock const &clock)
{
    foreach(System *sys, d->systems)
    {
        if(sys->behavior() & System::ObservesTime)
        {
            sys->timeChanged(clock);
        }
    }
}

game::Game &App::game()
{
    DENG2_ASSERT(app().d->currentGame != 0);
    return *app().d->currentGame;
}

void App::setGame(game::Game &game)
{
    d->currentGame = &game;
}

bool App::inMainThread()
{
    if(!App::appExists())
    {
        // No app even created yet, must be main thread.
        return true;
    }
    return DENG2_APP->d->mainThread == QThread::currentThread();
}

NativePath App::nativePluginBinaryPath()
{
    if(!d->cachedPluginBinaryPath.isEmpty()) return d->cachedPluginBinaryPath;

    NativePath path;
#ifdef WIN32
    path = d->appPath.fileNamePath() / "plugins";
#else
# ifdef MACOSX
    path = d->appPath.fileNamePath() / "../DengPlugins";
# else
    path = DENG_LIBRARY_DIR;
# endif
    // Also check the system config files.
    d->unixInfo->path("libdir", path);
#endif
    return (d->cachedPluginBinaryPath = path);
}

NativePath App::nativeHomePath()
{
    if(!d->cachedHomePath.isEmpty()) return d->cachedHomePath;

    int i;
    if((i = d->cmdLine.check("-userdir", 1)))
    {
        d->cmdLine.makeAbsolutePath(i + 1);
        return (d->cachedHomePath = d->cmdLine.at(i + 1));
    }

#ifdef MACOSX
    NativePath nativeHome = QDir::homePath();
    nativeHome = nativeHome / "Library/Application Support/Doomsday Engine/runtime";
#elif WIN32
    NativePath nativeHome = appDataPath();
    nativeHome = nativeHome / "runtime";
#else // UNIX
    NativePath nativeHome = QDir::homePath();
    nativeHome = nativeHome / ".doomsday/runtime";
#endif
    return (d->cachedHomePath = nativeHome);
}

Archive &App::persistentData()
{
    DENG2_ASSERT(DENG2_APP->d->persistentData != 0);

    return *DENG2_APP->d->persistentData;
}

NativePath App::currentWorkPath()
{
    return NativePath::workPath();
}

bool App::setCurrentWorkPath(NativePath const &cwd)
{
    return NativePath::setWorkPath(cwd);
}

NativePath App::nativeBasePath()
{
    if(!d->cachedBasePath.isEmpty()) return d->cachedBasePath;

    int i;
    if((i = d->cmdLine.check("-basedir", 1)))
    {
        d->cmdLine.makeAbsolutePath(i + 1);
        return (d->cachedBasePath = d->cmdLine.at(i + 1));
    }

    NativePath path;
#ifdef WIN32
    path = d->appPath.fileNamePath() / "..";
#else
# ifdef MACOSX
    path = d->appPath.fileNamePath() / "../Resources";
# else
    path = DENG_BASE_DIR;
# endif
    // Also check the system config files.
    d->unixInfo->path("basedir", path);
#endif
    return (d->cachedBasePath = path);
}

void App::initSubsystems(SubsystemInitFlags flags)
{
    bool allowPlugins = !flags.testFlag(DisablePlugins);

    d->initFileSystem(allowPlugins);

    if(!homeFolder().has("persist.pack") || commandLine().has("-reset"))
    {
        // Recreate the persistent state data package.
        ZipArchive arch;
        arch.add("Info", String("# Package for Doomsday's persistent state.\n").toUtf8());
        Writer(homeFolder().replaceFile("persist.pack")) << arch;

        homeFolder().populate(Folder::PopulateOnlyThisFolder);
    }            

    d->persistentData = &homeFolder().locate<PackageFolder>("persist.pack").archive();

    // The configuration.
    d->config = new Config("/modules/Config.de");
    d->scriptSys.addNativeModule("Config", d->config->names());

    d->config->read();

    // Immediately after upgrading, OLD_VERSION is also present in the Version module.
    Version oldVer = d->config->upgradedFromVersion();
    if(oldVer != Version())
    {
        ArrayValue *old = new ArrayValue;
        *old << NumberValue(oldVer.major) << NumberValue(oldVer.minor)
             << NumberValue(oldVer.patch) << NumberValue(oldVer.build);
        d->scriptSys.nativeModule("Version").addArray("OLD_VERSION", old).setReadOnly();
    }

    // Set up the log buffer.
    LogBuffer &logBuf = LogBuffer::appBuffer();

    // Update the log buffer max entry count: number of items to hold in memory.
    logBuf.setMaxEntryCount(d->config->getui("log.bufferSize"));

    try
    {
        // The -out option can be used to override the configured output file.
        int pos = 0;
        if((pos = commandLine().check("-out", 1)) > 0)
        {
            logBuf.setOutputFile(String("/home") / commandLine().at(pos + 1));
        }
        else
        {
            // Set the log output file.
            logBuf.setOutputFile(d->config->gets("log.file"));
        }
    }
    catch(Error const &er)
    {
        LOG_WARNING("Failed to set log output file:\n" + er.asText());
    }

    try
    {
        // The level of enabled messages.
        d->logFilter.read(d->config->names().subrecord("log.filter"));
    }
    catch(Error const &er)
    {
        LOG_WARNING("Failed to apply log filter:\n" + er.asText());
    }

    // Command line options may override the saved config.
    d->setLogLevelAccordingToOptions();

    LOGDEV_NOTE("Developer log entries enabled");

    // We can start flushing now when the destination is known.
    logBuf.enableFlushing(true);

    if(allowPlugins)
    {
#if 0 // not yet handled by libdeng2
        // Load the basic plugins.
        loadPlugins();
#endif
    }

    // Update the wall clock time.
    d->clock.setTime(Time::currentHighPerformanceTime());

    // Now we can start observing progress of time.
    d->clock.audienceForTimeChange += this;

    LOG_VERBOSE("libdeng2::App %s subsystems initialized.") << Version().asText();
}

void App::addSystem(System &system)
{
    d->systems.removeAll(&system);
    d->systems.append(&system);
}

void App::removeSystem(System &system)
{
    d->systems.removeAll(&system);
}

bool App::appExists()
{
    return singletonApp != 0;
}

App &App::app()
{
    DENG2_ASSERT(appExists());
    return *singletonApp;
}

LogFilter &App::logFilter()
{
    return DENG2_APP->d->logFilter;
}

CommandLine &App::commandLine()
{
    return DENG2_APP->d->cmdLine;
}

NativePath App::executablePath()
{
    return DENG2_APP->d->appPath;
}

#ifdef MACOSX
NativePath App::nativeAppContentsPath()
{
    return DENG2_APP->d->appPath/"../..";
}
#endif

FileSystem &App::fileSystem()
{
    return DENG2_APP->d->fs;
}

ScriptSystem &de::App::scriptSystem()
{
    return DENG2_APP->d->scriptSys;
}

Folder &App::rootFolder()
{
    return fileSystem().root();
}

Folder &App::homeFolder()
{
    return rootFolder().locate<Folder>("home");
}

Config &App::config()
{
    DENG2_ASSERT(DENG2_APP->d->config != 0);
    return *DENG2_APP->d->config;
}

UnixInfo &App::unixInfo()
{
    return *DENG2_APP->d->unixInfo;
}

} // namespace de
