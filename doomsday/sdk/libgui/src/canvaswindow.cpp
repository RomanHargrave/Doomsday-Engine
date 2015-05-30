/** @file canvaswindow.cpp Canvas window implementation.
 * @ingroup base
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include "de/CanvasWindow"
#include "de/GuiApp"

#include <QApplication>
#include <QGLFormat>
#include <QMoveEvent>
#include <QThread>
#include <QTimer>

#include <de/Config>
#include <de/Record>
#include <de/NumberValue>
#include <de/Log>
#include <de/RootWidget>
#include <de/GLState>
#include <de/c_wrapper.h>

namespace de {

static CanvasWindow *mainWindow = 0;

DENG2_PIMPL(CanvasWindow)
{
    Canvas* canvas; ///< Drawing surface for the contents of the window.
    Canvas* recreated;
    Canvas::FocusChangeAudience canvasFocusAudience; ///< Stored here during recreation.
    bool ready;
    bool mouseWasTrapped;
    unsigned int frameCount;
    float fps;

    Instance(Public *i)
        : Base(i),
          canvas(0),
          recreated(0),
          ready(false),
          mouseWasTrapped(false),
          frameCount(0),
          fps(0)
    {}

    ~Instance()
    {
        if(thisPublic == mainWindow)
        {
            mainWindow = 0;
        }
    }

    void updateFrameRateStatistics(void)
    {
        static Time lastFpsTime;

        Time const nowTime = Clock::appTime();

        // Increment the (local) frame counter.
        frameCount++;

        // Count the frames every other second.
        TimeDelta elapsed = nowTime - lastFpsTime;
        if(elapsed > 2.5)
        {
            fps = frameCount / elapsed;
            lastFpsTime = nowTime;
            frameCount = 0;
        }
    }

    void finishCanvasRecreation()
    {
        DENG2_ASSERT_IN_MAIN_THREAD();

        LOGDEV_GL_MSG("About to replace Canvas %p with %p")
                << de::dintptr(canvas) << de::dintptr(recreated);

        // Copy the audiences of the old canvas.
        recreated->copyAudiencesFrom(*canvas);

        // Switch the central widget. This will delete the old canvas automatically.
        self.setCentralWidget(recreated);
        canvas = recreated;
        recreated = 0;

        // Set up the basic GL state for the new canvas.
        canvas->makeCurrent();
        LIBGUI_ASSERT_GL_OK();

        DENG2_FOR_EACH_OBSERVER(Canvas::GLInitAudience, i, canvas->audienceForGLInit())
        {
            i->canvasGLInit(*canvas);
        }

        DENG2_GUI_APP->notifyGLContextChanged();

#ifdef DENG_X11
        canvas->update();
#else
        canvas->updateGL();
#endif
        LIBGUI_ASSERT_GL_OK();

        // Reacquire the focus.
        canvas->setFocus();
        if(mouseWasTrapped)
        {
            canvas->trapMouse();
        }

        // Restore the old focus change audience.
        canvas->audienceForFocusChange() = canvasFocusAudience;

        LOGDEV_GL_MSG("Canvas replaced with %p") << de::dintptr(canvas);
    }
};

CanvasWindow::CanvasWindow()
    : QMainWindow(0), d(new Instance(this))
{
    // Create the drawing canvas for this window.
    setCentralWidget(d->canvas = new Canvas(this)); // takes ownership

    d->canvas->audienceForGLReady() += this;
    d->canvas->audienceForGLDraw() += this;

    // All input goes to the canvas.
    d->canvas->setFocus();
}

bool CanvasWindow::isReady() const
{
    return d->ready;
}

float CanvasWindow::frameRate() const
{
    return d->fps;
}

void CanvasWindow::recreateCanvas()
{
    DENG2_ASSERT_IN_MAIN_THREAD();

    GLState::considerNativeStateUndefined();

    d->ready = false;

    // Steal the focus change audience temporarily so no spurious focus
    // notifications are sent.
    d->canvasFocusAudience = canvas().audienceForFocusChange();
    canvas().audienceForFocusChange().clear();

    // We'll re-trap the mouse after the new canvas is ready.
    d->mouseWasTrapped = canvas().isMouseTrapped();
    canvas().trapMouse(false);
    canvas().setParent(0);
    canvas().hide();

    // Create the replacement Canvas. Once it's created and visible, we'll
    // finish the switch-over.
    d->recreated = new Canvas(this, d->canvas);
    d->recreated->audienceForGLReady() += this;

    //d->recreated->setGeometry(d->canvas->geometry());
    d->recreated->show();
    d->recreated->update();

    LIBGUI_ASSERT_GL_OK();

    LOGDEV_GL_MSG("Canvas recreated, old one still exists");
    qDebug() << "old Canvas" << &canvas();
    qDebug() << "new Canvas" << d->recreated;
}

bool CanvasWindow::isRecreationInProgress() const
{
    return d->recreated != 0;
}

Canvas &CanvasWindow::canvas() const
{
    DENG2_ASSERT(d->canvas != 0);
    return *d->canvas;
}

bool CanvasWindow::ownsCanvas(Canvas *c) const
{
    if(!c) return false;
    return (d->canvas == c || d->recreated == c);
}

#ifdef WIN32
bool CanvasWindow::event(QEvent *ev)
{
    if(ev->type() == QEvent::ActivationChange)
    {
        //LOG_DEBUG("CanvasWindow: Forwarding QEvent::KeyRelease, Qt::Key_Alt");
        QKeyEvent keyEvent = QKeyEvent(QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
        return QApplication::sendEvent(&canvas(), &keyEvent);
    }
    return QMainWindow::event(ev);
}
#endif

void CanvasWindow::hideEvent(QHideEvent *ev)
{
    LOG_AS("CanvasWindow");

    QMainWindow::hideEvent(ev);

    LOG_GL_VERBOSE("Hide event (hidden:%b)") << isHidden();
}

void CanvasWindow::canvasGLReady(Canvas &canvas)
{
    d->ready = true;

    if(d->recreated == &canvas)
    {
#ifndef DENG_X11
        d->finishCanvasRecreation();
#else
        // Need to defer the finalization.
        qDebug() << "defer recreation";
        QTimer::singleShot(100, this, SLOT(finishCanvasRecreation()));
#endif
    }
}

void CanvasWindow::canvasGLDraw(Canvas &)
{
    d->updateFrameRateStatistics();
}

duint CanvasWindow::grabAsTexture(GrabMode mode) const
{
    return d->canvas->grabAsTexture(
                mode == GrabHalfSized? QSize(width()/2, height()/2) : QSize());
}

duint CanvasWindow::grabAsTexture(Rectanglei const &area, GrabMode mode) const
{
    QSize size;
    if(mode == GrabHalfSized)
    {
        size = QSize(area.width()/2, area.height()/2);
    }
    return d->canvas->grabAsTexture(
                QRect(area.left(), area.top(), area.width(), area.height()), size);
}

bool CanvasWindow::grabToFile(NativePath const &path) const
{
    return d->canvas->grabImage().save(path.toString());
}

void CanvasWindow::swapBuffers(gl::SwapBufferMode swapMode) const
{
    // Force a swapbuffers right now.
    d->canvas->swapBuffers(swapMode);
}

void CanvasWindow::glActivate()
{
    canvas().makeCurrent();
}

void CanvasWindow::glDone()
{
    canvas().doneCurrent();
}

void *CanvasWindow::nativeHandle() const
{
    return reinterpret_cast<void *>(winId());
}

void CanvasWindow::finishCanvasRecreation()
{
    d->finishCanvasRecreation();
}

bool CanvasWindow::mainExists()
{
    return mainWindow != 0;
}

CanvasWindow &CanvasWindow::main()
{
    DENG2_ASSERT(mainWindow != 0);
    return *mainWindow;
}

void CanvasWindow::setMain(CanvasWindow *window)
{
    mainWindow = window;
}

} // namespace de
