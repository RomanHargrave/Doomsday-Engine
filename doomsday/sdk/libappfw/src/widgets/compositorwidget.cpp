/** @file compositorwidget.cpp  Off-screen compositor.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/CompositorWidget"

#include <de/Drawable>

namespace de {

DENG_GUI_PIMPL(CompositorWidget)
{
    Drawable drawable;
    struct Buffer {
        GLTexture texture;
        QScopedPointer<GLTarget> offscreen;

        void clear() {
            texture.clear();
            offscreen.reset();
        }
    };
    int nextBufIndex;
    QList<Buffer *> buffers; ///< Stack of buffers to allow nested compositing.
    GLUniform uMvpMatrix;
    GLUniform uTex;    

    Instance(Public *i)
        : Base(i),
          nextBufIndex(0),
          uMvpMatrix("uMvpMatrix", GLUniform::Mat4),
          uTex      ("uTex",       GLUniform::Sampler2D)
    {
        uMvpMatrix = Matrix4f::ortho(0, 1, 0, 1);
    }

    /**
     * Starts using the next unused buffer. The buffer is resized if needed.
     * The size of the new buffer is the same as the size of the current GL
     * target.
     */
    Buffer *beginBufferUse()
    {
        if(nextBufIndex <= buffers.size())
        {
            buffers.append(new Buffer);
        }

        Buffer *buf = buffers[nextBufIndex];
        Vector2ui const size = GLState::current().target().rectInUse().size();
        //qDebug() << "compositor" << nextBufIndex << "should be" << size.asText();
        //qDebug() << buf->texture.size().asText() << size.asText();
        if(buf->texture.size() != size)
        {
            //qDebug() << "buffer texture defined" << size.asText();
            buf->texture.setUndefinedImage(size, Image::RGBA_8888);
            buf->offscreen.reset(new GLTarget(buf->texture));
        }
        nextBufIndex++;
        return buf;
    }

    void endBufferUse()
    {
        nextBufIndex--;
    }

    void glInit()
    {
        DefaultVertexBuf *buf = new DefaultVertexBuf;
        buf->setVertices(gl::TriangleStrip,
                         DefaultVertexBuf::Builder()
                            .makeQuad(Rectanglef(0, 0, 1, 1),
                                      Vector4f  (1, 1, 1, 1),
                                      Rectanglef(0, 0, 1, -1)),
                         gl::Static);

        drawable.addBuffer(buf);
        root().shaders().build(drawable.program(), "generic.textured.color") //"debug.textured.alpha"
                << uMvpMatrix
                << uTex;
    }

    void glDeinit()
    {
        qDeleteAll(buffers);
        buffers.clear();
        drawable.clear();
    }

    bool shouldBeDrawn() const
    {
        return self.isInitialized() && !self.isHidden() && self.visibleOpacity() > 0 &&
               GLState::current().target().rectInUse().size() != Vector2ui();
    }
};

CompositorWidget::CompositorWidget(String const &name)
    : GuiWidget(name), d(new Instance(this))
{}

GLTexture &CompositorWidget::composite() const
{
    DENG2_ASSERT(!d->buffers.isEmpty());
    return d->buffers.first()->texture;
}

void CompositorWidget::setCompositeProjection(Matrix4f const &projMatrix)
{
    d->uMvpMatrix = projMatrix;
}

void CompositorWidget::useDefaultCompositeProjection()
{
    d->uMvpMatrix = Matrix4f::ortho(0, 1, 0, 1);
}

void CompositorWidget::viewResized()
{
    GuiWidget::viewResized();
}

void CompositorWidget::preDrawChildren()
{
    if(!d->shouldBeDrawn()) return;

    //qDebug() << "entering compositor" << d->nextBufIndex;

    Instance::Buffer *buf = d->beginBufferUse();
    DENG2_ASSERT(!buf->offscreen.isNull());

    GLState::push()
            .setTarget(*buf->offscreen)
            .setViewport(Rectangleui::fromSize(buf->texture.size()));

    buf->offscreen->clear(GLTarget::Color);
}

void CompositorWidget::postDrawChildren()
{
    if(!d->shouldBeDrawn()) return;

    // Restore original rendering target.
    GLState::pop();

    //qDebug() << "exiting compositor, drawing composite";

    drawComposite();

    d->endBufferUse();
}

void CompositorWidget::glInit()
{
    d->glInit();
}

void CompositorWidget::glDeinit()
{
    d->glDeinit();
}

void CompositorWidget::drawComposite()
{
    if(!d->shouldBeDrawn()) return;

    glDisable(GL_ALPHA_TEST); /// @todo remove this
    glEnable(GL_TEXTURE_2D);

    DENG2_ASSERT(d->nextBufIndex > 0);

    Instance::Buffer *buf = d->buffers[d->nextBufIndex - 1];

    GLState::push()
            .setBlend(true)
            .setBlendFunc(gl::One, gl::OneMinusSrcAlpha)
            .setDepthTest(false);

    d->uTex = buf->texture;
    //d->uColor = Vector4f(1, 1, 1, visibleOpacity());
    d->drawable.draw();

    GLState::pop();

    //qDebug() << "composite" << d->nextBufIndex - 1 << "has been drawn";
}

} // namespace de
