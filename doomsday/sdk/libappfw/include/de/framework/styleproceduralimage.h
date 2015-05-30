/** @file styleproceduralimage.h  Procedural image that uses a common UI texture.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_STYLEPROCEDURALIMAGE_H
#define LIBAPPFW_STYLEPROCEDURALIMAGE_H

#include "../ProceduralImage"
#include "../GuiWidget"
#include <de/DotPath>

namespace de {

class StyleProceduralImage : public ProceduralImage
{
public:
    StyleProceduralImage(DotPath const &styleImageId, GuiWidget &owner, float angle = 0)
        : _owner(owner), _imageId(styleImageId), _id(Id::None), _angle(angle)
    {
        if(_owner.hasRoot())
        {
            // We can set this up right away.
            alloc();
        }
    }

    GuiRootWidget &root()
    {
        return _owner.root();
    }

    void setAngle(float angle)
    {
        _angle = angle;
    }

    void alloc()
    {
        _id = root().styleTexture(_imageId);
        setSize(root().atlas().imageRect(_id).size());
    }

    void glInit()
    {
        alloc();
    }

    void glDeinit()
    {
        _id = Id::None;
    }

    void glMakeGeometry(DefaultVertexBuf::Builder &verts, Rectanglef const &rect)
    {
        if(!_id.isNone())
        {
            Matrix4f turn = Matrix4f::rotateAround(rect.middle(), _angle);
            verts.makeQuad(rect, color(), root().atlas().imageRectf(_id), &turn);
        }
    }

private:
    GuiWidget &_owner;
    DotPath _imageId;
    Id _id;
    float _angle;
};

} // namespace de

#endif // LIBAPPFW_STYLEPROCEDURALIMAGE_H
