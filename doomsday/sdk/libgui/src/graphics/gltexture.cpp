/** @file gltexture.cpp  GL texture atlas.
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

#include "de/GLTexture"
#include "de/GLInfo"
#include "de/graphics/opengl.h"

namespace de {

namespace internal
{
    enum TextureFlag {
        AutoMips        = 0x1,
        MipmapAvailable = 0x2,
        ParamsChanged   = 0x4
    };
    Q_DECLARE_FLAGS(TextureFlags, TextureFlag)
}

Q_DECLARE_OPERATORS_FOR_FLAGS(internal::TextureFlags)

using namespace internal;
using namespace gl;

DENG2_PIMPL(GLTexture)
{
    Size size;
    Image::Format format;
    GLuint name;
    GLenum texTarget;
    Filter minFilter;
    Filter magFilter;
    MipFilter mipFilter;
    Wraps wrap;
    dfloat maxAnisotropy;
    dfloat maxLevel;
    TextureFlags flags;

    Instance(Public *i)
        : Base(i)
        , format(Image::Unknown)
        , name(0)
        , texTarget(GL_TEXTURE_2D)
        , minFilter(Linear), magFilter(Linear), mipFilter(MipNone)
        , wrap(Wraps(Repeat, Repeat))
        , maxAnisotropy(1.0f)
        , maxLevel(1000.f)
        , flags(ParamsChanged)
    {}

    ~Instance()
    {
        release();
    }

    void alloc()
    {
        if(!name)
        {
            glGenTextures(1, &name);
        }
    }

    void release()
    {
        if(name)
        {
            glDeleteTextures(1, &name);
            name = 0;
        }
    }

    void clear()
    {
        release();
        size = Size();
        texTarget = GL_TEXTURE_2D;
        flags |= ParamsChanged;
        self.setState(NotReady);
    }

    bool isCube() const
    {
        return texTarget == GL_TEXTURE_CUBE_MAP;
    }

    static GLenum glWrap(gl::Wrapping w)
    {
        switch(w)
        {
        case Repeat:         return GL_REPEAT;
        case RepeatMirrored: return GL_MIRRORED_REPEAT;
        case ClampToEdge:    return GL_CLAMP_TO_EDGE;
        }
        return GL_REPEAT;
    }

    static GLenum glMinFilter(gl::Filter min, gl::MipFilter mip)
    {
        if(mip == MipNone)
        {
            if(min == Nearest) return GL_NEAREST;
            if(min == Linear)  return GL_LINEAR;
        }
        else if(mip == MipNearest)
        {
            if(min == Nearest) return GL_NEAREST_MIPMAP_NEAREST;
            if(min == Linear)  return GL_LINEAR_MIPMAP_NEAREST;
        }
        else // MipLinear
        {
            if(min == Nearest) return GL_NEAREST_MIPMAP_LINEAR;
            if(min == Linear)  return GL_LINEAR_MIPMAP_LINEAR;
        }
        return GL_NEAREST;
    }

    static GLenum glFace(gl::CubeFace face)
    {
        switch(face)
        {
        case PositiveX: return GL_TEXTURE_CUBE_MAP_POSITIVE_X;
        case PositiveY: return GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
        case PositiveZ: return GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
        case NegativeX: return GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
        case NegativeY: return GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
        case NegativeZ: return GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
        }
        return GL_TEXTURE_CUBE_MAP_POSITIVE_X;
    }

    void glBind() const
    {
        DENG2_ASSERT(name != 0);

        glBindTexture(texTarget, name); LIBGUI_ASSERT_GL_OK();
    }

    void glUnbind() const
    {
        glBindTexture(texTarget, 0);
    }

    /**
     * Update the OpenGL texture parameters. You must bind the texture before
     * calling.
     */
    void glUpdateParamsOfBoundTexture()
    {        
        glTexParameteri(texTarget, GL_TEXTURE_WRAP_S,     glWrap(wrap.x));
        glTexParameteri(texTarget, GL_TEXTURE_WRAP_T,     glWrap(wrap.y));
        glTexParameteri(texTarget, GL_TEXTURE_MAG_FILTER, magFilter == Nearest? GL_NEAREST : GL_LINEAR);
        glTexParameteri(texTarget, GL_TEXTURE_MIN_FILTER, glMinFilter(minFilter, mipFilter));
        glTexParameterf(texTarget, GL_TEXTURE_MAX_LEVEL,  maxLevel);

        if(GLInfo::extensions().EXT_texture_filter_anisotropic)
        {
            glTexParameterf(texTarget, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAnisotropy);
        }

        LIBGUI_ASSERT_GL_OK();

        flags &= ~ParamsChanged;
    }

    void glImage(int level, Size const &size, GLPixelFormat const &glFormat,
                 void const *data, CubeFace face = PositiveX)
    {
        /// @todo GLES2: Check for the BGRA extension.

        // Choose suitable informal format.
        GLenum const internalFormat =
                (glFormat.format == GL_BGRA?          GL_RGBA :
                 glFormat.format == GL_DEPTH_STENCIL? GL_DEPTH24_STENCIL8 :
                                                      glFormat.format);

        /*qDebug() << "glTexImage2D:" << name << (isCube()? glFace(face) : texTarget)
                << level << internalFormat << size.x << size.y << 0
                << glFormat.format << glFormat.type << data;*/

        if(data) glPixelStorei(GL_UNPACK_ALIGNMENT, glFormat.rowAlignment);
        glTexImage2D(isCube()? glFace(face) : texTarget,
                     level, internalFormat, size.x, size.y, 0,
                     glFormat.format, glFormat.type, data);

        LIBGUI_ASSERT_GL_OK();
    }

    void glSubImage(int level, Vector2i const &pos, Size const &size,
                    GLPixelFormat const &glFormat, void const *data, CubeFace face = PositiveX)
    {
        if(data) glPixelStorei(GL_UNPACK_ALIGNMENT, glFormat.rowAlignment);
        glTexSubImage2D(isCube()? glFace(face) : texTarget,
                        level, pos.x, pos.y, size.x, size.y,
                        glFormat.format, glFormat.type, data);

        LIBGUI_ASSERT_GL_OK();
    }
};

GLTexture::GLTexture() : d(new Instance(this))
{}

GLTexture::GLTexture(GLuint existingTexture, Size const &size) : d(new Instance(this))
{
    d->size = size;
    d->name = existingTexture;
    d->flags |= ParamsChanged;
}

void GLTexture::clear()
{
    d->clear();
}

void GLTexture::setMagFilter(Filter magFilter)
{
    d->magFilter = magFilter;
    d->flags |= ParamsChanged;
}

void GLTexture::setMinFilter(Filter minFilter, MipFilter mipFilter)
{
    d->minFilter = minFilter;
    d->mipFilter = mipFilter;
    d->flags |= ParamsChanged;
}

void GLTexture::setWrapS(Wrapping mode)
{
    d->wrap.x = mode;
    d->flags |= ParamsChanged;
}

void GLTexture::setWrapT(Wrapping mode)
{
    d->wrap.y = mode;
    d->flags |= ParamsChanged;
}

void GLTexture::setMaxAnisotropy(dfloat maxAnisotropy)
{
    d->maxAnisotropy = maxAnisotropy;
    d->flags |= ParamsChanged;
}

void GLTexture::setMaxLevel(dfloat maxLevel)
{
    d->maxLevel = maxLevel;
    d->flags |= ParamsChanged;
}

Filter GLTexture::minFilter() const
{
    return d->minFilter;
}

Filter GLTexture::magFilter() const
{
    return d->magFilter;
}

MipFilter GLTexture::mipFilter() const
{
    return d->mipFilter;
}

Wrapping GLTexture::wrapS() const
{
    return d->wrap.x;
}

Wrapping GLTexture::wrapT() const
{
    return d->wrap.y;
}

GLTexture::Wraps GLTexture::wrap() const
{
    return d->wrap;
}

dfloat GLTexture::maxAnisotropy() const
{
    return d->maxAnisotropy;
}

dfloat GLTexture::maxLevel() const
{
    return d->maxLevel;
}

bool GLTexture::isCubeMap() const
{
    return d->isCube();
}

void GLTexture::setAutoGenMips(bool genMips)
{
    if(genMips)
        d->flags |= AutoMips;
    else
        d->flags &= ~AutoMips;
}

bool GLTexture::autoGenMips() const
{
    return d->flags.testFlag(AutoMips);
}

void GLTexture::setUndefinedImage(GLTexture::Size const &size, Image::Format format, int level)
{
    d->texTarget = GL_TEXTURE_2D;
    d->size = size;
    d->format = format;

    d->alloc();
    d->glBind();
    d->glImage(level, size, Image::glFormat(format), NULL);
    d->glUnbind();

    setState(Ready);
}

void GLTexture::setUndefinedImage(CubeFace face, GLTexture::Size const &size,
                                  Image::Format format, int level)
{
    d->texTarget = GL_TEXTURE_CUBE_MAP;
    d->size = size;
    d->format = format;

    d->alloc();
    d->glBind();
    d->glImage(level, size, Image::glFormat(format), NULL, face);
    d->glUnbind();

    setState(Ready);
}

void GLTexture::setUndefinedContent(Size const &size, GLPixelFormat const &glFormat, int level)
{
    d->texTarget = GL_TEXTURE_2D;
    d->size = size;
    d->format = Image::Unknown;

    d->alloc();
    d->glBind();
    d->glImage(level, size, glFormat, NULL);
    d->glUnbind();

    setState(Ready);
}

void GLTexture::setUndefinedContent(CubeFace face, Size const &size, GLPixelFormat const &glFormat, int level)
{
    d->texTarget = GL_TEXTURE_CUBE_MAP;
    d->size = size;
    d->format = Image::Unknown;

    d->alloc();
    d->glBind();
    d->glImage(level, size, glFormat, NULL, face);
    d->glUnbind();

    setState(Ready);
}

void GLTexture::setDepthStencilContent(Size const &size)
{
    setUndefinedContent(size, GLPixelFormat(GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8));
}

void GLTexture::setImage(Image const &image, int level)
{
    d->texTarget = GL_TEXTURE_2D;
    d->size = image.size();
    d->format = image.format();

    d->alloc();
    d->glBind();
    d->glImage(level, image.size(), image.glFormat(), image.bits());
    d->glUnbind();

    if(!level && d->flags.testFlag(AutoMips))
    {
        generateMipmap();
    }

    setState(Ready);
}

void GLTexture::setImage(CubeFace face, Image const &image, int level)
{
    d->texTarget = GL_TEXTURE_CUBE_MAP;
    d->size = image.size();
    d->format = image.format();

    d->alloc();
    d->glBind();
    d->glImage(level, image.size(), image.glFormat(), image.bits(), face);
    d->glUnbind();

    if(!level && d->flags.testFlag(AutoMips))
    {
        generateMipmap();
    }

    setState(Ready);
}

void GLTexture::setSubImage(Image const &image, Vector2i const &pos, int level)
{
    d->texTarget = GL_TEXTURE_2D;

    d->alloc();
    d->glBind();
    d->glSubImage(level, pos, image.size(), image.glFormat(), image.bits());
    d->glUnbind();

    if(!level && d->flags.testFlag(AutoMips))
    {
        generateMipmap();
    }
}

void GLTexture::setSubImage(CubeFace face, Image const &image, Vector2i const &pos, int level)
{
    d->texTarget = GL_TEXTURE_CUBE_MAP;

    d->alloc();
    d->glBind();
    d->glSubImage(level, pos, image.size(), image.glFormat(), image.bits(), face);
    d->glUnbind();

    if(!level && d->flags.testFlag(AutoMips))
    {
        generateMipmap();
    }
}

void GLTexture::generateMipmap()
{
    if(d->name)
    {
        d->glBind();
        glGenerateMipmap(d->texTarget); LIBGUI_ASSERT_GL_OK();
        d->glUnbind();

        d->flags |= MipmapAvailable;
    }
}

GLTexture::Size GLTexture::size() const
{
    return d->size;
}

int GLTexture::mipLevels() const
{
    if(!isReady()) return 0;
    return d->flags.testFlag(MipmapAvailable)? levelsForSize(d->size) : 1;
}

GLTexture::Size GLTexture::levelSize(int level) const
{
    if(level < 0) return Size();
    return levelSize(d->size, level);
}

GLuint GLTexture::glName() const
{
    return d->name;
}

void GLTexture::glBindToUnit(int unit) const
{
    glActiveTexture(GL_TEXTURE0 + unit);

    aboutToUse();

    d->glBind();

    if(d->flags.testFlag(ParamsChanged))
    {
        d->glUpdateParamsOfBoundTexture();
    }
}

void GLTexture::glApplyParameters()
{
    if(d->flags.testFlag(ParamsChanged))
    {
        d->glBind();
        d->glUpdateParamsOfBoundTexture();
        d->glUnbind();
    }
}

Image::Format GLTexture::imageFormat() const
{
    return d->format;
}

GLTexture::Size GLTexture::maximumSize()
{
    int v = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &v); LIBGUI_ASSERT_GL_OK();
    return Size(v, v);
}

void GLTexture::aboutToUse() const
{
    // nothing to do
}

int GLTexture::levelsForSize(GLTexture::Size const &size)
{
    int mipLevels = 0;
    duint w = size.x;
    duint h = size.y;
    while(w > 1 || h > 1)
    {
        w = de::max(1u, w >> 1);
        h = de::max(1u, h >> 1);
        mipLevels++;
    }
    return mipLevels;
}

GLTexture::Size GLTexture::levelSize(GLTexture::Size const &size0, int level)
{
    Size s = size0;
    for(int i = 0; i < level; ++i)
    {
        s.x = de::max(1u, s.x >> 1);
        s.y = de::max(1u, s.y >> 1);
    }
    return s;
}

} // namespace de
