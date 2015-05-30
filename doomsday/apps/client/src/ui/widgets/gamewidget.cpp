/** @file gamewidget.cpp
 *
 * @authors Copyright © 2013-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h" // must be included first

#include "ui/widgets/gamewidget.h"
#include "clientapp.h"
#include "ui/ddevent.h"
#include "ui/ui_main.h"
#include "ui/sys_input.h"
#include "ui/busyvisual.h"
#include "ui/clientwindowsystem.h"
#include "ui/widgets/taskbarwidget.h"
#include "ui/widgets/busywidget.h"
#include "dd_def.h"
#include "dd_main.h"
#include "dd_loop.h"
#include "sys_system.h"
#include "edit_bias.h"
#include "world/map.h"
#include "network/net_main.h"
#include "render/r_main.h"
#include "render/rend_main.h"
#include "render/cameralensfx.h"
#include "audio/s_main.h"
#include "render/lightgrid.h"
#include "gl/gl_main.h"
#include "gl/sys_opengl.h"
#include "gl/gl_defer.h"

#include <de/GLState>

/**
 * Maximum number of milliseconds spent uploading textures at the beginning
 * of a frame. Note that non-uploaded textures will appear as pure white
 * until their content gets uploaded (you should precache them).
 */
#define FRAME_DEFERRED_UPLOAD_TIMEOUT 20

using namespace de;

DENG2_PIMPL(GameWidget)
{
    Instance(Public *i) : Base(i) {}

    void draw()
    {
        bool cannotDraw = (self.isDisabled() || !GL_IsFullyInited());

        if(renderWireframe || cannotDraw)
        {
            // When rendering is wireframe mode, we must clear the screen
            // before rendering a frame.
            glClear(GL_COLOR_BUFFER_BIT);
        }

        if(cannotDraw) return;

        if(App_GameLoaded())
        {
            // Notify the world that a new render frame has begun.
            App_WorldSystem().beginFrame(CPP_BOOL(R_NextViewer()));

            R_RenderViewPorts(Player3DViewLayer);
            R_RenderViewPorts(ViewBorderLayer);

            // End any open DGL sequence.
            DGL_End();

            // Notify the world that we've finished rendering the frame.
            App_WorldSystem().endFrame();
        }

        // End any open DGL sequence.
        DGL_End();
    }

    void updateSize()
    {
        LOG_AS("GameWidget");
        LOG_GL_XVERBOSE("View resized to ") << self.rule().recti().size().asText();

        // Update viewports.
        R_SetViewGrid(0, 0);
        if(!App_GameLoaded())
        {
            // Update for busy mode.
            R_UseViewPort(0);
        }
        UI_LoadFonts();
    }
};

GameWidget::GameWidget(String const &name)
    : GuiWidget(name), d(new Instance(this))
{
    requestGeometry(false);
}

void GameWidget::glApplyViewport(Rectanglei const &rect)
{
    GLState::current()
            .setNormalizedViewport(normalizedRect(rect))
            .apply();
}

void GameWidget::viewResized()
{
    GuiWidget::viewResized();

    /*
    if(BusyMode_Active() || isDisabled() || Sys_IsShuttingDown() ||
       !ClientApp::windowSystem().hasMain())
    {
        return;
    }

    d->updateSize();*/
}

void GameWidget::update()
{
    GuiWidget::update();

    if(isDisabled() || BusyMode_Active()) return;

    // We may be performing GL operations.
    ClientWindow::main().glActivate();

    // Run at least one (fractional) tic.
    Loop_RunTics();

    // We may have received a Quit message from the windowing system
    // during events/tics processing.
    if(Sys_IsShuttingDown())
        return;

    GL_ProcessDeferredTasks(FRAME_DEFERRED_UPLOAD_TIMEOUT);

    // Release the busy transition frame now when we can be sure that busy mode
    // is over / didn't start at all.
    if(!Con_TransitionInProgress())
    {
        ClientWindow::main().busy().releaseTransitionFrame();
    }
}

void GameWidget::drawContent()
{
    if(isDisabled() || !GL_IsFullyInited())
        return;

    GLState::push();

    Rectanglei pos;
    if(hasChangedPlace(pos))
    {
        // Automatically update if the widget is resized.
        d->updateSize();
    }

    d->draw();

    GLState::considerNativeStateUndefined();
    GLState::pop();
}

bool GameWidget::handleEvent(Event const &event)
{
    /**
     * @todo Event processing should occur here, not during Loop_RunTics().
     * However, care must be taken to reproduce the vanilla behavior of
     * controls with regard to response times.
     *
     * @todo Input drivers need to support Unicode text; for now we have to
     * submit as Latin1.
     */

    ClientWindow &window = root().window().as<ClientWindow>();

    if(event.type() == Event::MouseButton && !root().window().canvas().isMouseTrapped())
    {
        if(!window.hasSidebar())
        {
            // If the mouse is not trapped, we will just eat button clicks which
            // will prevent them from reaching the legacy input system.
            return true;
        }

        // If the sidebar is open, we must explicitly click on the GameWidget to
        // cause input to be trapped.
        switch(handleMouseClick(event))
        {
        case MouseClickFinished:
            // Click completed on the widget, trap the mouse.
            window.canvas().trapMouse();
            window.taskBar().close();
            root().setFocus(0); // Allow input to reach here.
            break;

        default:
            // Just ignore the event.
            return true;
        }
    }

    if(event.type() == Event::KeyPress ||
       event.type() == Event::KeyRepeat ||
       event.type() == Event::KeyRelease)
    {
        KeyEvent const &ev = event.as<KeyEvent>();
        Keyboard_Submit(ev.state() == KeyEvent::Pressed? IKE_DOWN :
                        ev.state() == KeyEvent::Repeat?  IKE_REPEAT :
                                                         IKE_UP,
                        ev.ddKey(), ev.nativeCode(), ev.text().toLatin1());
    }

    return false;
}
