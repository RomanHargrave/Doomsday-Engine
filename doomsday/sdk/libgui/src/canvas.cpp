/** @file canvas.cpp  OpenGL drawing surface (QWidget).
 *
 * @authors Copyright (c) 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Canvas"
#include "de/CanvasWindow"
#include "de/GLState"
#include "de/GLTexture"
#include "de/graphics/opengl.h"

#include <de/App>
#include <de/Log>
#include <de/Drawable>
#include <de/GLInfo>
#include <de/GLFramebuffer>

#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QShowEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QImage>
#include <QTimer>
#include <QTime>
#include <QDebug>

namespace de {

#ifdef DENG_X11
#  define LIBGUI_CANVAS_USE_DEFERRED_RESIZE
#endif

DENG2_PIMPL(Canvas)
{
    GLFramebuffer framebuf;

    CanvasWindow *parent;
    bool readyNotified;
    Size currentSize;
    Size pendingSize;
#ifdef LIBGUI_CANVAS_USE_DEFERRED_RESIZE
    QTimer resizeTimer;
#endif
    bool mouseGrabbed;
#ifdef WIN32
    bool altIsDown;
#endif
    QPoint prevMousePos;
    QTime prevWheelAt;
    QPoint wheelAngleAccum;
    int wheelDir[2];

    Instance(Public *i, CanvasWindow *parentWindow)
        : Base(i)
        , parent(parentWindow)
        , readyNotified(false)
        , mouseGrabbed(false)
    {        
        wheelDir[0] = wheelDir[1] = 0;

#ifdef WIN32
        altIsDown = false;
#endif
        //mouseDisabled = App::commandLine().has("-nomouse");

#ifdef LIBGUI_CANVAS_USE_DEFERRED_RESIZE
        resizeTimer.setSingleShot(true);
        QObject::connect(&resizeTimer, SIGNAL(timeout()), thisPublic, SLOT(updateSize()));
#endif
    }

    ~Instance()
    {
        glDeinit();
    }

    void grabMouse()
    {
        if(!self.isVisible()/* || mouseDisabled*/) return;

        if(!mouseGrabbed)
        {
            LOG_INPUT_VERBOSE("Grabbing mouse") << mouseGrabbed;

            mouseGrabbed = true;

            DENG2_FOR_PUBLIC_AUDIENCE2(MouseStateChange, i)
            {
                i->mouseStateChanged(Trapped);
            }
        }
    }

    void ungrabMouse()
    {
        if(!self.isVisible()/* || mouseDisabled*/) return;

        if(mouseGrabbed)
        {
            LOG_INPUT_VERBOSE("Ungrabbing mouse");

            // Tell the mouse driver that the mouse is untrapped.
            mouseGrabbed = false;

            DENG2_FOR_PUBLIC_AUDIENCE2(MouseStateChange, i)
            {
                i->mouseStateChanged(Untrapped);
            }
        }
    }

    static int nativeCode(QKeyEvent const *ev)
    {
#if defined(UNIX) && !defined(MACOSX)
        return ev->nativeScanCode();
#else
        return ev->nativeVirtualKey();
#endif
    }

    void handleKeyEvent(QKeyEvent *ev)
    {
        //LOG_AS("Canvas");

        ev->accept();
        //if(ev->isAutoRepeat()) return; // Ignore repeats, we do our own.

        /*
        qDebug() << "Canvas: key press" << ev->key() << QString("0x%1").arg(ev->key(), 0, 16)
                 << "text:" << ev->text()
                 << "native:" << ev->nativeVirtualKey()
                 << "scancode:" << ev->nativeScanCode();
        */

#ifdef WIN32
        // We must track the state of the alt key ourselves as the OS grabs the up event...
        if(ev->key() == Qt::Key_Alt)
        {
            if(ev->type() == QEvent::KeyPress)
            {
                if(altIsDown) return; // Ignore repeat down events(!)?
                altIsDown = true;
            }
            else if(ev->type() == QEvent::KeyRelease)
            {
                if(!altIsDown)
                {
                    LOG_DEBUG("Ignoring repeat alt up.");
                    return; // Ignore repeat up events.
                }
                altIsDown = false;
                //LOG_DEBUG("Alt is up.");
            }
        }
#endif

        DENG2_FOR_PUBLIC_AUDIENCE2(KeyEvent, i)
        {
            i->keyEvent(KeyEvent(ev->isAutoRepeat()?             KeyEvent::Repeat :
                                 ev->type() == QEvent::KeyPress? KeyEvent::Pressed :
                                                                 KeyEvent::Released,
                                 ev->key(),
                                 KeyEvent::ddKeyFromQt(ev->key(), ev->nativeVirtualKey(), ev->nativeScanCode()),
                                 nativeCode(ev),
                                 ev->text(),
                                 (ev->modifiers().testFlag(Qt::ShiftModifier)?   KeyEvent::Shift   : KeyEvent::NoModifiers) |
                                 (ev->modifiers().testFlag(Qt::ControlModifier)? KeyEvent::Control : KeyEvent::NoModifiers) |
                                 (ev->modifiers().testFlag(Qt::AltModifier)?     KeyEvent::Alt     : KeyEvent::NoModifiers) |
                                 (ev->modifiers().testFlag(Qt::MetaModifier)?    KeyEvent::Meta    : KeyEvent::NoModifiers)));
        }
    }

    void reconfigureFramebuffer()
    {
        framebuf.setColorFormat(Image::RGB_888);
        framebuf.resize(currentSize);
    }

    void glInit()
    {
        DENG2_ASSERT(parent != 0);
        framebuf.glInit();
    }

    void glDeinit()
    {
        framebuf.glDeinit();
    }

    void swapBuffers(gl::SwapBufferMode mode)
    {
        if(mode == gl::SwapStereoBuffers && !self.format().stereo())
        {
            // The canvas is not using a stereo format, must do a normal swap.
            mode = gl::SwapMonoBuffer;
        }

        /// @todo Double buffering is not really needed in manual FB mode.
        framebuf.swapBuffers(self, mode);
    }

    template <typename QtEventType>
    Vector2i translatePosition(QtEventType const *ev) const
    {
#ifdef DENG2_QT_5_1_OR_NEWER
        return Vector2i(ev->pos().x(), ev->pos().y()) * self.devicePixelRatio();
#else
        return Vector2i(ev->pos().x(), ev->pos().y());
#endif
    }

    DENG2_PIMPL_AUDIENCE(GLReady)
    DENG2_PIMPL_AUDIENCE(GLInit)
    DENG2_PIMPL_AUDIENCE(GLResize)
    DENG2_PIMPL_AUDIENCE(GLDraw)
    DENG2_PIMPL_AUDIENCE(FocusChange)
};

DENG2_AUDIENCE_METHOD(Canvas, GLReady)
DENG2_AUDIENCE_METHOD(Canvas, GLInit)
DENG2_AUDIENCE_METHOD(Canvas, GLResize)
DENG2_AUDIENCE_METHOD(Canvas, GLDraw)
DENG2_AUDIENCE_METHOD(Canvas, FocusChange)

Canvas::Canvas(CanvasWindow* parent, QGLWidget* shared)
    : QGLWidget(parent, shared), d(new Instance(this, parent))
{
    LOG_AS("Canvas");
    LOGDEV_GL_VERBOSE("Swap interval: ") << format().swapInterval();
    LOGDEV_GL_VERBOSE("Multisampling: %b") << (GLFramebuffer::defaultMultisampling() > 1);

    // We will be doing buffer swaps manually (for timing purposes).
    setAutoBufferSwap(false);

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void Canvas::setParent(CanvasWindow *parent)
{
    d->parent = parent;
}

QImage Canvas::grabImage(QSize const &outputSize)
{
    return grabImage(rect(), outputSize);
}

QImage Canvas::grabImage(QRect const &area, QSize const &outputSize)
{
    // We will be grabbing the visible, latest complete frame.
    glReadBuffer(GL_FRONT);
    QImage grabbed = grabFrameBuffer(); // no alpha
    if(area.size() != grabbed.size())
    {
        // Just take a portion of the full image.
        grabbed = grabbed.copy(area);
    }
    glReadBuffer(GL_BACK);
    if(outputSize.isValid())
    {
        grabbed = grabbed.scaled(outputSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    return grabbed;
}

GLuint Canvas::grabAsTexture(QSize const &outputSize)
{
    return grabAsTexture(rect(), outputSize);
}

GLuint Canvas::grabAsTexture(QRect const &area, QSize const &outputSize)
{
    return bindTexture(grabImage(area, outputSize), GL_TEXTURE_2D, GL_RGB,
                       QGLContext::LinearFilteringBindOption);
}

Canvas::Size Canvas::size() const
{
    return d->currentSize;
}

void Canvas::trapMouse(bool trap)
{
    if(trap)
    {
        d->grabMouse();
    }
    else
    {
        d->ungrabMouse();
    }
}

bool Canvas::isMouseTrapped() const
{
    return d->mouseGrabbed;
}

bool Canvas::isGLReady() const
{
    return d->readyNotified;
}

void Canvas::copyAudiencesFrom(Canvas const &other)
{
    d->audienceForGLReady         = other.d->audienceForGLReady;
    d->audienceForGLInit          = other.d->audienceForGLInit;
    d->audienceForGLResize        = other.d->audienceForGLResize;
    d->audienceForGLDraw          = other.d->audienceForGLDraw;
    d->audienceForFocusChange     = other.d->audienceForFocusChange;

    audienceForKeyEvent()         = other.audienceForKeyEvent();

    audienceForMouseStateChange() = other.audienceForMouseStateChange();
    audienceForMouseEvent()       = other.audienceForMouseEvent();
}

GLTarget &Canvas::renderTarget() const
{
    return d->framebuf.target();
}

GLFramebuffer &Canvas::framebuffer()
{
    return d->framebuf;
}

void Canvas::swapBuffers(gl::SwapBufferMode swapMode)
{
    d->swapBuffers(swapMode);
}

void Canvas::initializeGL()
{
    LOG_AS("Canvas");
    LOGDEV_GL_NOTE("Notifying GL init (during paint)");

#ifdef LIBGUI_USE_GLENTRYPOINTS
    getAllOpenGLEntryPoints();
#endif
    GLInfo::glInit();

    DENG2_FOR_AUDIENCE2(GLInit, i) i->canvasGLInit(*this);
}

void Canvas::resizeGL(int w, int h)
{
    d->pendingSize = Size(max(0, w), max(0, h));

    // Only react if this is actually a resize.
    if(d->currentSize != d->pendingSize)
    {
#ifdef LIBGUI_CANVAS_USE_DEFERRED_RESIZE
        LOGDEV_GL_MSG("Canvas %p triggered size to %ix%i from %s")
                << this << w << h << d->currentSize.asText();
        d->resizeTimer.start(100);
#else
        updateSize();
#endif
    }
}

void Canvas::updateSize()
{
#ifdef LIBGUI_CANVAS_USE_DEFERRED_RESIZE
    LOGDEV_GL_MSG("Canvas %p resizing now") << this;
#endif

    makeCurrent();
    d->currentSize = d->pendingSize; 
    d->reconfigureFramebuffer();

    DENG2_FOR_AUDIENCE2(GLResize, i) i->canvasGLResized(*this);
}

void Canvas::showEvent(QShowEvent* ev)
{
    LOG_AS("Canvas");

    QGLWidget::showEvent(ev);

    // The first time the window is shown, run the initialization callback. On
    // some platforms, OpenGL is not fully ready to be used before the window
    // actually appears on screen.
    if(isVisible() && !d->readyNotified)
    {
        LOGDEV_GL_XVERBOSE("Received first show event, scheduling GL ready notification");

#ifdef LIBGUI_USE_GLENTRYPOINTS
        makeCurrent();
        getAllOpenGLEntryPoints();
#endif
        GLInfo::glInit();
        QTimer::singleShot(1, this, SLOT(notifyReady()));
    }
}

void Canvas::notifyReady()
{
    if(d->readyNotified) return;

    d->readyNotified = true;

    d->glInit();
    d->reconfigureFramebuffer();

    // Print some information.
    QGLFormat const fmt = format();
    if(fmt.openGLVersionFlags().testFlag(QGLFormat::OpenGL_Version_3_3))
        LOG_GL_NOTE("OpenGL 3.3 supported");
    else if((fmt.openGLVersionFlags().testFlag(QGLFormat::OpenGL_Version_3_2)))
        LOG_GL_NOTE("OpenGL 3.2 supported");
    else if((fmt.openGLVersionFlags().testFlag(QGLFormat::OpenGL_Version_3_1)))
        LOG_GL_NOTE("OpenGL 3.1 supported");
    else if((fmt.openGLVersionFlags().testFlag(QGLFormat::OpenGL_Version_3_0)))
        LOG_GL_NOTE("OpenGL 3.0 supported");
    else if((fmt.openGLVersionFlags().testFlag(QGLFormat::OpenGL_Version_2_1)))
        LOG_GL_NOTE("OpenGL 2.1 supported");
    else if((fmt.openGLVersionFlags().testFlag(QGLFormat::OpenGL_Version_2_0)))
        LOG_GL_NOTE("OpenGL 2.0 supported");
    else
        LOG_GL_WARNING("OpenGL 2.0 is not supported!");

    LOGDEV_GL_XVERBOSE("Notifying GL ready");
    DENG2_FOR_AUDIENCE2(GLReady, i) i->canvasGLReady(*this);

    // This Canvas instance might have been destroyed now.
}

void Canvas::paintGL()
{
    if(!d->parent || d->parent->isRecreationInProgress()) return;
#ifdef LIBGUI_USE_GLENTRYPOINTS
    if(!glBindFramebuffer) return;
#endif

    DENG2_ASSERT(QGLContext::currentContext() != 0);

    LIBGUI_ASSERT_GL_OK();

    // Make sure any changes to the state stack become effective.
    GLState::current().apply();

    DENG2_FOR_AUDIENCE2(GLDraw, i) i->canvasGLDraw(*this);

    LIBGUI_ASSERT_GL_OK();
}

void Canvas::focusInEvent(QFocusEvent*)
{
    LOG_AS("Canvas");
    LOG_INPUT_VERBOSE("Gained focus");

    DENG2_FOR_AUDIENCE2(FocusChange, i) i->canvasFocusChanged(*this, true);
}

void Canvas::focusOutEvent(QFocusEvent*)
{
    LOG_AS("Canvas");
    LOG_INPUT_VERBOSE("Lost focus");

    // Automatically ungrab the mouse if focus is lost.
    d->ungrabMouse();

    DENG2_FOR_AUDIENCE2(FocusChange, i) i->canvasFocusChanged(*this, false);
}

void Canvas::keyPressEvent(QKeyEvent *ev)
{
    d->handleKeyEvent(ev);
}

void Canvas::keyReleaseEvent(QKeyEvent *ev)
{
    d->handleKeyEvent(ev);
}

static MouseEvent::Button translateButton(Qt::MouseButton btn)
{
    if(btn == Qt::LeftButton)   return MouseEvent::Left;
#ifdef DENG2_QT_4_7_OR_NEWER
    if(btn == Qt::MiddleButton) return MouseEvent::Middle;
#else
    if(btn == Qt::MidButton)    return MouseEvent::Middle;
#endif
    if(btn == Qt::RightButton)  return MouseEvent::Right;
    if(btn == Qt::XButton1)     return MouseEvent::XButton1;
    if(btn == Qt::XButton2)     return MouseEvent::XButton2;

    return MouseEvent::Unknown;
}

void Canvas::mousePressEvent(QMouseEvent *ev)
{
    ev->accept();

    DENG2_FOR_AUDIENCE2(MouseEvent, i)
    {
        i->mouseEvent(MouseEvent(translateButton(ev->button()), MouseEvent::Pressed,
                                 d->translatePosition(ev)));
    }
}

void Canvas::mouseReleaseEvent(QMouseEvent* ev)
{
    ev->accept();

    DENG2_FOR_AUDIENCE2(MouseEvent, i)
    {
        i->mouseEvent(MouseEvent(translateButton(ev->button()), MouseEvent::Released,
                                 d->translatePosition(ev)));
    }
}

void Canvas::mouseMoveEvent(QMouseEvent *ev)
{
    ev->accept();

    // Absolute events are only emitted when the mouse is untrapped.
    if(!d->mouseGrabbed)
    {
        DENG2_FOR_AUDIENCE2(MouseEvent, i)
        {
            i->mouseEvent(MouseEvent(MouseEvent::Absolute,
                                     d->translatePosition(ev)));
        }
    }
}

void Canvas::wheelEvent(QWheelEvent *ev)
{
    ev->accept();

#ifdef DENG2_QT_5_0_OR_NEWER
    float const devicePixels = d->parent->devicePixelRatio();

    QPoint numPixels = ev->pixelDelta();
    QPoint numDegrees = ev->angleDelta() / 8;
    d->wheelAngleAccum += numDegrees;

    if(!numPixels.isNull())
    {
        DENG2_FOR_AUDIENCE2(MouseEvent, i)
        {
            if(numPixels.x())
            {
                i->mouseEvent(MouseEvent(MouseEvent::FineAngle, Vector2i(devicePixels * numPixels.x(), 0),
                                         d->translatePosition(ev)));
            }
            if(numPixels.y())
            {
                i->mouseEvent(MouseEvent(MouseEvent::FineAngle, Vector2i(0, devicePixels * numPixels.y()),
                                         d->translatePosition(ev)));
            }
        }
    }

    QPoint const steps = d->wheelAngleAccum / 15;
    if(!steps.isNull())
    {
        DENG2_FOR_AUDIENCE2(MouseEvent, i)
        {
            if(steps.x())
            {
                i->mouseEvent(MouseEvent(MouseEvent::Step, Vector2i(steps.x(), 0),
                                         !d->mouseGrabbed? d->translatePosition(ev) : Vector2i()));
            }
            if(steps.y())
            {
                i->mouseEvent(MouseEvent(MouseEvent::Step, Vector2i(0, steps.y()),
                                         !d->mouseGrabbed? d->translatePosition(ev) : Vector2i()));
            }
        }
        d->wheelAngleAccum -= steps * 15;
    }

#else
    static const int MOUSE_WHEEL_CONTINUOUS_THRESHOLD_MS = 100;

    bool continuousMovement = (d->prevWheelAt.elapsed() < MOUSE_WHEEL_CONTINUOUS_THRESHOLD_MS);
    int axis = (ev->orientation() == Qt::Horizontal? 0 : 1);
    int dir = (ev->delta() < 0? -1 : 1);

    DENG2_FOR_AUDIENCE2(MouseEvent, i)
    {
        i->mouseEvent(MouseEvent(MouseEvent::FineAngle,
                                 axis == 0? Vector2i(ev->delta(), 0) :
                                            Vector2i(0, ev->delta()),
                                 d->translatePosition(ev)));
    }

    if(!continuousMovement || d->wheelDir[axis] != dir)
    {
        d->wheelDir[axis] = dir;

        DENG2_FOR_AUDIENCE2(MouseEvent, i)
        {
            i->mouseEvent(MouseEvent(MouseEvent::Step,
                                     axis == 0? Vector2i(dir, 0) :
                                     axis == 1? Vector2i(0, dir) : Vector2i(),
                                     !d->mouseGrabbed? d->translatePosition(ev) : Vector2i()));
        }
    }
#endif

    d->prevWheelAt.start();
}

} // namespace de
