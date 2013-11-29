/** @file gltarget.cpp  GL render target.
 *
 * Implementation does not use QGLFrameBufferObject because it does not allow
 * attaching manually created textures.
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

#include "de/GLTarget"
#include "de/GLTexture"
#include "de/GLState"
#include "de/CanvasWindow"
#include <de/Asset>

namespace de {

static Vector2ui const nullSize;

DENG2_PIMPL(GLTarget),
DENG2_OBSERVES(Asset, Deletion)
{
    enum AttachmentId {
        ColorBuffer,
        DepthBuffer,
        StencilBuffer,
        MAX_ATTACHMENTS
    };

    static AttachmentId attachmentToId(GLenum atc)
    {
        switch(atc)
        {
        case GL_COLOR_ATTACHMENT0:
            return ColorBuffer;

        case GL_DEPTH_ATTACHMENT:
            return DepthBuffer;

        case GL_STENCIL_ATTACHMENT:
            return StencilBuffer;

        case GL_DEPTH_STENCIL_ATTACHMENT:
            return DepthBuffer;

        default:
            DENG2_ASSERT(false);
            break;
        }
        return ColorBuffer; // should not be reached
    }

    GLuint fbo;
    GLuint renderBufs[MAX_ATTACHMENTS];
    GLTexture *bufTextures[MAX_ATTACHMENTS];
    Flags flags;
    Flags textureAttachment;    ///< Where to attach @a texture.
    GLTexture *texture;    
    Vector2ui size;
    Vector4f clearColor;
    Rectangleui activeRect; ///< Initially null.

    Instance(Public *i)
        : Base(i), fbo(0),
          flags(DefaultFlags), textureAttachment(NoAttachments),
          texture(0)
    {
        zap(renderBufs);
        zap(bufTextures);
    }

    Instance(Public *i, Flags const &texAttachment, GLTexture &colorTexture, Flags const &otherAtm)
        : Base(i), fbo(0),
          flags(texAttachment | otherAtm), textureAttachment(texAttachment),
          texture(&colorTexture), size(colorTexture.size())
    {
        zap(renderBufs);
        zap(bufTextures);
    }

    Instance(Public *i, Vector2ui const &targetSize, Flags const &fboFlags)
        : Base(i), fbo(0),
          flags(fboFlags), textureAttachment(NoAttachments),
          texture(0), size(targetSize)
    {
        zap(renderBufs);
        zap(bufTextures);
    }

    ~Instance()
    {
        release();
    }

    bool isDefault() const
    {
        return !texture && size == nullSize;
    }

    GLTexture *bufferTexture(Flags const &flags) const
    {
        if(flags == Color)
        {
            return bufTextures[ColorBuffer];
        }
        if(flags == DepthStencil || flags == Depth)
        {
            return bufTextures[DepthBuffer];
        }
        if(flags == Stencil)
        {
            return bufTextures[StencilBuffer];
        }
        return 0;
    }

    void attachRenderbuffer(AttachmentId id, GLenum type, GLenum attachment)
    {
        DENG2_ASSERT(size != Vector2ui(0, 0));

        glGenRenderbuffers       (1, &renderBufs[id]);
        glBindRenderbuffer       (GL_RENDERBUFFER, renderBufs[id]);
        glRenderbufferStorage    (GL_RENDERBUFFER, type, size.x, size.y);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,  attachment,
                                  GL_RENDERBUFFER, renderBufs[id]);
        LIBGUI_ASSERT_GL_OK();
    }

    void allocFBO()
    {
        if(isDefault() || fbo) return;

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        LOG_DEBUG("Creating FBO %i") << fbo;
    }

    void attachTexture(GLTexture &tex, GLenum attachment, int level = 0)
    {
        DENG2_ASSERT(tex.isReady());

        LOG_DEBUG("glTex %i (level %i) => FBO attachment %i (0x%x)")
                << tex.glName() << level << attachmentToId(attachment) << attachment;

        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, tex.glName(), level);
        LIBGUI_ASSERT_GL_OK();

        bufTextures[attachmentToId(attachment)] = &tex;
    }

    void alloc()
    {
        allocFBO();

        if(texture)
        {
            // The texture's attachment point must be unambiguously defined.
            DENG2_ASSERT(textureAttachment == Color   ||
                         textureAttachment == Depth   ||
                         textureAttachment == Stencil ||
                         textureAttachment == DepthStencil);

            attachTexture(*texture,
                          textureAttachment == Color?   GL_COLOR_ATTACHMENT0  :
                          textureAttachment == Depth?   GL_DEPTH_ATTACHMENT   :
                          textureAttachment == Stencil? GL_STENCIL_ATTACHMENT :
                                                        GL_DEPTH_STENCIL_ATTACHMENT);
        }

        if(size != nullSize) // A non-default target: size must be specified.
        {
            allocRenderBuffers();
        }

        validate();
    }

    void allocRenderBuffers()
    {
        DENG2_ASSERT(size != nullSize);

        // Fill in all the other requested attachments.
        if(flags.testFlag(Color) && !textureAttachment.testFlag(Color))
        {
            /// @todo Note that for GLES, GL_RGBA8 is not supported (without an extension).
            LOG_DEBUG("FBO color attachment %s") << size.asText();
            attachRenderbuffer(ColorBuffer, GL_RGBA8, GL_COLOR_ATTACHMENT0);
        }

        if(flags.testFlag(DepthStencil) && (!texture || textureAttachment == Color))
        {
            // We can use a combined depth/stencil buffer.
            LOG_DEBUG("FBO depth+stencil attachment %s") << size.asText();
            attachRenderbuffer(DepthBuffer, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL_ATTACHMENT);
        }
        else
        {
            // Separate depth and stencil, then.
            if(flags.testFlag(Depth) && !textureAttachment.testFlag(Depth))
            {
                LOG_DEBUG("FBO depth attachment %s") << size.asText();
                attachRenderbuffer(DepthBuffer, GL_DEPTH_COMPONENT16, GL_DEPTH_ATTACHMENT);
            }
            if(flags.testFlag(Stencil) && !textureAttachment.testFlag(Stencil))
            {
                LOG_DEBUG("FBO stencil attachment %s") << size.asText();
                attachRenderbuffer(StencilBuffer, GL_STENCIL_INDEX8, GL_STENCIL_ATTACHMENT);
            }
        }

        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }

    void releaseRenderBuffers()
    {
        glDeleteRenderbuffers(MAX_ATTACHMENTS, renderBufs);
        zap(renderBufs);
        zap(bufTextures);
    }

    void release()
    {
        self.setState(NotReady);
        if(fbo)
        {
            releaseRenderBuffers();
            glDeleteFramebuffers(1, &fbo);
            fbo = 0;
        }
        zap(bufTextures);
        texture = 0;
        size = Vector2ui(0, 0);
    }

    void resizeRenderBuffers(Size const &newSize)
    {
        size = newSize;

        releaseRenderBuffers();
        allocRenderBuffers();
    }

    void validate()
    {
        if(isDefault())
        {
            self.setState(Ready);
            return;
        }

        DENG2_ASSERT(fbo != 0);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        GLState::considerNativeStateUndefined(); // state was manually changed

        if(status != GL_FRAMEBUFFER_COMPLETE)
        {
            self.setState(NotReady);

            throw ConfigError("GLTarget::validate",
                status == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT? "Incomplete attachments" :
                status == GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS? "Mismatch with dimensions" :
                status == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT? "No images attached" :
                                                                        "Unsupported");
        }
        self.setState(Ready);
    }

    void assetDeleted(Asset &asset)
    {
        if(&asset == texture)
        {
            release();
        }
    }
};

GLTarget::GLTarget() : d(new Instance(this))
{
    setState(Ready);
}

GLTarget::GLTarget(GLTexture &colorTarget, Flags const &otherAttachments)
    : d(new Instance(this, Color, colorTarget, otherAttachments))
{
    LOG_AS("GLTarget");
    d->alloc();
}

GLTarget::GLTarget(Flags const &attachment, GLTexture &texture, Flags const &otherAttachments)
    : d(new Instance(this, attachment, texture, otherAttachments))
{
    LOG_AS("GLTarget");
    d->alloc();
}

GLTarget::GLTarget(Vector2ui const &size, Flags const &flags)
    : d(new Instance(this, size, flags))
{
    LOG_AS("GLTarget");
    d->alloc();
}

void GLTarget::configure()
{
    LOG_AS("GLTarget");

    d->release();

    d->texture = 0;
    d->textureAttachment = NoAttachments;
    d->flags = NoAttachments;
    d->size = nullSize;

    setState(Ready);
}

void GLTarget::configure(GLTexture *colorTex, GLTexture *depthStencilTex)
{
    DENG2_ASSERT(colorTex || depthStencilTex);

    LOG_AS("GLTarget");

    d->release();

    d->texture = 0;
    d->textureAttachment = NoAttachments;
    d->flags = ColorDepthStencil;
    d->size = (colorTex? colorTex->size() : depthStencilTex->size());

    d->allocFBO();

    // The color attachment.
    if(colorTex)
    {
        DENG2_ASSERT(colorTex->isReady());
        DENG2_ASSERT(d->size == colorTex->size());
        d->attachTexture(*colorTex, GL_COLOR_ATTACHMENT0);
    }
    else
    {
        d->attachRenderbuffer(Instance::ColorBuffer, GL_RGBA8, GL_COLOR_ATTACHMENT0);
    }

    // The depth attachment.
    if(depthStencilTex)
    {
        DENG2_ASSERT(depthStencilTex->isReady());
        DENG2_ASSERT(d->size == depthStencilTex->size());
        d->attachTexture(*depthStencilTex, GL_DEPTH_STENCIL_ATTACHMENT);
    }
    else
    {
        d->attachRenderbuffer(Instance::DepthBuffer, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL_ATTACHMENT);
    }

    d->validate();
}

void GLTarget::configure(Flags const &attachment, GLTexture &texture, Flags const &otherAttachments)
{
    LOG_AS("GLTarget");

    d->release();

    // Set new configuration.
    d->texture = &texture;
    d->textureAttachment = attachment;
    d->flags = attachment | otherAttachments;
    d->size = texture.size();

    d->alloc();
}

void GLTarget::glBind() const
{
    DENG2_ASSERT(isReady());
    if(!isReady()) return;

    glBindFramebuffer(GL_FRAMEBUFFER, d->fbo);
}

void GLTarget::glRelease() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

QImage GLTarget::toImage() const
{
    if(!d->fbo)
    {
        return CanvasWindow::main().canvas().grabImage();
    }
    else if(d->flags & Color)
    {
        // Read the contents of the color attachment.
        Size imgSize = size();
        QImage img(QSize(imgSize.x, imgSize.y), QImage::Format_ARGB32);
        glBind();
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glReadPixels(0, 0, imgSize.x, imgSize.y, GL_BGRA, GL_UNSIGNED_BYTE,
                     (GLvoid *) img.constBits());
        // Restore the stack's target.
        GLState::top().target().glBind();
        return img;
    }
    return QImage();
}

void GLTarget::setClearColor(Vector4f const &color)
{
    d->clearColor = color;
}

void GLTarget::clear(Flags const &attachments)
{
    glBind();

    // Only clear what we have.
    Flags which = attachments & d->flags;

    glClearColor(d->clearColor.x, d->clearColor.y, d->clearColor.z, d->clearColor.w);
    glClear((which & Color?   GL_COLOR_BUFFER_BIT   : 0) |
            (which & Depth?   GL_DEPTH_BUFFER_BIT   : 0) |
            (which & Stencil? GL_STENCIL_BUFFER_BIT : 0));

    GLState::top().target().glBind();
}

void GLTarget::resize(Size const &size)
{
    // The default target resizes itself automatically with the canvas.
    if(d->size == size || d->isDefault()) return;

    glBind();
    if(d->texture)
    {
        d->texture->setUndefinedImage(size, d->texture->imageFormat());
    }
    d->resizeRenderBuffers(size);
    GLState::top().target().glBind();
}

GLTexture *GLTarget::attachedTexture(Flags const &attachment) const
{
    return d->bufferTexture(attachment);
}

GLuint GLTarget::glName() const
{
    return d->fbo;
}

GLTarget::Size GLTarget::size() const
{
    if(d->texture)
    {
        return d->texture->size();
    }
    else if(d->size != nullSize)
    {
        return d->size;
    }
    return CanvasWindow::main().canvas().size();
}

void GLTarget::setActiveRect(Rectangleui const &rect, bool applyGLState)
{
    d->activeRect = rect;
    if(applyGLState)
    {
        // Forcibly update viewport and scissor (and other GL state).
        GLState::considerNativeStateUndefined();
        GLState::top().apply();
    }
}

void GLTarget::unsetActiveRect(bool applyGLState)
{
    setActiveRect(Rectangleui(), applyGLState);
}

Rectangleui GLTarget::scaleToActiveRect(Rectangleui const &rectInTarget) const
{
    // If no sub rectangle is defined, do nothing.
    if(!hasActiveRect())
    {
        return rectInTarget;
    }

    Vector2f const scaling = Vector2f(d->activeRect.size()) / size();

    return Rectangleui(d->activeRect.left()  + scaling.x * rectInTarget.left(),
                       d->activeRect.top()   + scaling.y * rectInTarget.top(),
                       rectInTarget.width()  * scaling.x,
                       rectInTarget.height() * scaling.y);
}

Rectangleui const &GLTarget::activeRect() const
{
    return d->activeRect;
}

bool GLTarget::hasActiveRect() const
{
    return !d->activeRect.isNull();
}

} // namespace de
