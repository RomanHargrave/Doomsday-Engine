/** @file postprocessing.cpp World frame post processing.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "render/fx/postprocessing.h"
#include "ui/clientwindow.h"
#include "clientapp.h"

#include <de/Animation>
#include <de/Drawable>
#include <de/GLFramebuffer>

#include <QList>

using namespace de;

namespace fx {

DENG2_PIMPL(PostProcessing)
{
    GLFramebuffer framebuf;
    Drawable frame;
    GLUniform uMvpMatrix;
    GLUniform uFrame;
    GLUniform uFadeInOut;
    Animation fade;

    struct QueueEntry {
        String shaderName;
        float fade;
        TimeDelta span;

        QueueEntry(String const &name, float f, TimeDelta const &s)
            : shaderName(name), fade(f), span(s) {}
    };
    typedef QList<QueueEntry> Queue;
    Queue queue;

    typedef GLBufferT<Vertex2Tex> VBuf;

    Instance(Public *i)
        : Base(i)
        , uMvpMatrix("uMvpMatrix", GLUniform::Mat4)
        , uFrame    ("uTex",       GLUniform::Sampler2D)
        , uFadeInOut("uFadeInOut", GLUniform::Float)
        , fade(0, Animation::Linear)
    {}

    GuiRootWidget &root() const
    {
        return ClientWindow::main().game().root();
    }

#if 0
    Vector2ui consoleSize() const
    {        
        /**
         * @todo The offscreen target should simply use the viewport area, not
         * the full canvas size. This way the shader could, for instance,
         * easily mirror texture coordinates. However, this would require
         * drawing the frame without applying a further GL viewport in the game
         * widgets. -jk
         */
        //return self.viewRect().size();
        return root().window().canvas().size();
    }
#endif

    bool setShader(String const &name)
    {
        try
        {
            self.shaders().build(frame.program(), "fx.post." + name);
            LOG_VERBOSE("Post-processing shader \"fx.post.%s\"") << name;
            return true;
        }
        catch(Error const &er)
        {
            LOG_WARNING("Failed to set shader to \"fx.post.%s\":\n%s")
                    << name << er.asText();
        }
        return false;
    }

    /// Determines if the post-processing shader will be applied.
    bool isActive() const
    {
        return !fade.done() || fade > 0 || !queue.isEmpty();
    }

    void glInit()
    {
        //LOG_DEBUG("Allocating texture and target, size %s") << consoleSize().asText();

        framebuf.glInit();
        //framebuf.setColorFormat(Image::RGBA_8888);
        //framebuf.resize(consoleSize());

        uMvpMatrix = Matrix4f::ortho(0, 1, 0, 1);
        uFrame = framebuf.colorTexture();

        // Drawable for drawing stuff back to the original target.
        VBuf *buf = new VBuf;
        buf->setVertices(gl::TriangleStrip,
                         VBuf::Builder().makeQuad(Rectanglef(0, 0, 1, 1),
                                                  Rectanglef(0, 1, 1, -1)),
                         gl::Static);
        frame.addBuffer(buf);
        frame.program() << uMvpMatrix << uFrame << uFadeInOut;
    }

    void glDeinit()
    {
        LOG_DEBUG("Releasing GL resources");
        framebuf.glDeinit();
    }

    Rectangleui outRect() const
    {
        GLTarget &target = GLState::current().target();
        Rectangleui rect = Rectangleui::fromSize(target.size());

        if(target.hasActiveRect())
        {
            rect = target.scaleToActiveRect(rect);
        }
        return rect;
    }

    void update()
    {
        Rectangleui rect = outRect();

        framebuf.resize(rect.size());
        framebuf.setSampleCount(GLFramebuffer::defaultMultisampling());
    }

    void checkQueue()
    {
        // An ongoing fade?
        if(!fade.done()) return; // Let's check back later.

        if(!queue.isEmpty())
        {
            QueueEntry entry = queue.takeFirst();
            if(!entry.shaderName.isEmpty())
            {
                if(!setShader(entry.shaderName))
                {
                    fade = 0;
                    return;
                }
            }
            fade.setValue(entry.fade, entry.span);
            LOG_DEBUG("%s %s") << entry.shaderName << fade.asText();
        }
    }

    void begin()
    {
        if(!isActive()) return;

        update();

        GLState::push()
                .setTarget(framebuf.target())
                .setViewport(Rectangleui::fromSize(framebuf.size()))
                .setColorMask(gl::WriteAll)
                .apply();
        framebuf.target().clear(GLTarget::ColorDepthStencil);
    }

    void end()
    {
        if(!isActive()) return;

        GLState::pop().apply();
    }

    void draw()
    {
        if(!isActive()) return;

        glEnable(GL_TEXTURE_2D);
        glDisable(GL_ALPHA_TEST);

        /*
        Canvas::Size const size = root().window().canvas().size();
        uMvpMatrix =
                Matrix4f::ortho(0, size.x, 0, size.y) *
                Matrix4f::scaleThenTranslate(self.viewRect().size(),
                                             self.viewRect().topLeft);*/

        uFadeInOut = fade;

        GLState::push()
                .setBlend(false)
                .setDepthTest(false)
                .setViewport(Rectangleui::fromSize(GLState::current().target().size()))
                .apply();

        frame.draw();

        GLState::pop().apply();

        glEnable(GL_ALPHA_TEST);
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
    }
};

PostProcessing::PostProcessing(int console)
    : ConsoleEffect(console), d(new Instance(this))
{}

bool PostProcessing::isActive() const
{
    return d->isActive();
}

void PostProcessing::fadeInShader(String const &fxPostShader, TimeDelta const &span)
{
    d->queue.append(Instance::QueueEntry(fxPostShader, 1, span));
}

void PostProcessing::fadeOut(TimeDelta const &span)
{
    d->queue.append(Instance::QueueEntry("", 0, span));
}

void PostProcessing::glInit()
{
    if(!d->isActive()) return;

    LOG_AS("fx::PostProcessing");

    ConsoleEffect::glInit();
    d->glInit();
}

void PostProcessing::glDeinit()
{
    LOG_AS("fx::PostProcessing");

    d->glDeinit();
    ConsoleEffect::glDeinit();
}

void PostProcessing::beginFrame()
{
    d->begin();
}

void PostProcessing::draw()
{
    d->end();
    d->draw();
}

void PostProcessing::endFrame()
{
    LOG_AS("fx::PostProcessing");

    if(!d->isActive() && isInited())
    {
        glDeinit();
    }

    d->checkQueue();
}

} // namespace fx