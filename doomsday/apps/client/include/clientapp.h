/** @file clientapp.h  The client application.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENTAPP_H
#define CLIENTAPP_H

#include <de/BaseGuiApp>

#include "settingsregister.h"
#include "network/serverlink.h"
#include "ui/inputsystem.h"
#include "ui/clientwindowsystem.h"
#include "ui/infine/infinesystem.h"
#include "render/rendersystem.h"
#include "resource/resourcesystem.h"
#include "updater.h"
#include "Games"
#include "world/worldsystem.h"

/**
 * The client application.
 */
class ClientApp : public de::BaseGuiApp
{
    Q_OBJECT

public:
    ClientApp(int &argc, char **argv);

    /**
     * Sets up all the subsystems of the application. Must be called before the
     * event loop is started. At the end of this method, the bootstrap script is
     * executed.
     */
    void initialize();

    void preFrame();
    void postFrame();

    /**
     * Reports a new alert to the user.
     *
     * @param msg    Message to show. May contain style escapes.
     * @param level  Importance of the message.
     */
    static void alert(de::String const &msg, de::LogEntry::Level level = de::LogEntry::Message);

public:
    static ClientApp &app();
    static Updater &updater();
    static SettingsRegister &logSettings();
    static SettingsRegister &networkSettings();
    static SettingsRegister &audioSettings();    ///< @todo Belongs in AudioSystem.
    static ServerLink &serverLink();
    static InFineSystem &infineSystem();
    static InputSystem &inputSystem();
    static ClientWindowSystem &windowSystem();
    static RenderSystem &renderSystem();
    static ResourceSystem &resourceSystem();
    static de::Games &games();
    static de::WorldSystem &worldSystem();

    static bool hasRenderSystem();

public slots:
    void openHomepageInBrowser();
    void openInBrowser(QUrl url);

    /**
     * Enters the "native UI" mode that temporarily switches the main window to a
     * regular window and restores the desktop display mode. This allows the user to
     * access native UI widgets normally.
     *
     * Call this before showing native UI widgets. You must call endNativeUIMode()
     * afterwards.
     */
    void beginNativeUIMode();

    /**
     * Ends the "native UI" mode, restoring the previous main window properties.
     */
    void endNativeUIMode();

private:
    DENG2_PRIVATE(d)
};

#endif // CLIENTAPP_H
