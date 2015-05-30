/** @file atlastexture.h  Atlas stored on a GLTexture.
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

#include "de/AtlasTexture"
#include "de/RowAtlasAllocator"
#include "de/KdTreeAtlasAllocator"

namespace de {

AtlasTexture::AtlasTexture(Atlas::Flags const &flags, Atlas::Size const &totalSize)
    : Atlas(flags, totalSize)
{
    // Atlas textures are updated automatically when needed.
    setState(Ready);
}

AtlasTexture *AtlasTexture::newWithRowAllocator(Atlas::Flags const &flags, Atlas::Size const &totalSize)
{
    AtlasTexture *atlas = new AtlasTexture(flags, totalSize);
    atlas->setAllocator(new RowAtlasAllocator);
    return atlas;
}

AtlasTexture *AtlasTexture::newWithKdTreeAllocator(Atlas::Flags const &flags, Atlas::Size const &totalSize)
{
    AtlasTexture *atlas = new AtlasTexture(flags, totalSize);
    atlas->setAllocator(new KdTreeAtlasAllocator);
    return atlas;
}

void AtlasTexture::clear()
{
    Atlas::clear();
    GLTexture::clear();

    setState(Ready);
}

void AtlasTexture::aboutToUse() const
{
    Atlas::commit();
}

void AtlasTexture::commitFull(Image const &fullImage) const
{
    DENG2_ASSERT(fullImage.size() == totalSize());

    // While the Atlas is const, the texture isn't...
    const_cast<AtlasTexture *>(this)->setImage(fullImage);
}

void AtlasTexture::commit(Image const &image, Vector2i const &topLeft) const
{
    GLTexture *tex = const_cast<AtlasTexture *>(this);

    if(size() == GLTexture::Size(0, 0))
    {
        // Hasn't been full-committed yet.
        tex->setUndefinedImage(totalSize(), Image::RGBA_8888);
    }

    tex->setSubImage(image, topLeft);
}

} // namespace de
