/** @file vertexbuilder.h  Utility for composing triangle strips.
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

#ifndef LIBGUI_VERTEXBUILDER_H
#define LIBGUI_VERTEXBUILDER_H

#include <QVector>
#include <de/Vector>
#include <de/Matrix>
#include <de/Rectangle>

namespace de {

/**
 * Utility for composing simple geometric constructs (using triangle strips).
 */
template <typename VertexType>
struct VertexBuilder
{
    struct Vertices : public QVector<VertexType> {
        Vertices() {
            QVector<VertexType>::reserve(64);
        }
        void transform(Matrix4f const &matrix) {
            for(int i = 0; i < QVector<VertexType>::size(); ++i) {
                (*this)[i].pos = matrix * (*this)[i].pos;
            }
        }
        Vertices &operator += (Vertices const &other) {
            concatenate(other, *this);
            return *this;
        }
        Vertices operator + (Vertices const &other) const {
            Vertices v(*this);
            return v += other;
        }
        Vertices &makeQuad(Rectanglef const &rect, Vector4f const &color, Vector2f const &uv) {
            Vertices quad;
            VertexType v;
            v.rgba = color;
            v.texCoord = uv;
            v.pos = rect.topLeft;      quad << v;
            v.pos = rect.topRight();   quad << v;
            v.pos = rect.bottomLeft(); quad << v;
            v.pos = rect.bottomRight;  quad << v;
            return *this += quad;
        }
        Vertices &makeQuad(Rectanglef const &rect, Rectanglef const &uv) {
            Vertices quad;
            VertexType v;
            v.pos = rect.topLeft;      v.texCoord = uv.topLeft;      quad << v;
            v.pos = rect.topRight();   v.texCoord = uv.topRight();   quad << v;
            v.pos = rect.bottomLeft(); v.texCoord = uv.bottomLeft(); quad << v;
            v.pos = rect.bottomRight;  v.texCoord = uv.bottomRight;  quad << v;
            return *this += quad;
        }
        Vertices &makeQuad(Rectanglef const &rect, Vector4f const &color, Rectanglef const &uv,
                           Matrix4f const *matrix = 0) {
            Vertices quad;
            VertexType v;
            v.rgba = color;
            v.pos = rect.topLeft;      v.texCoord = uv.topLeft;      quad << v;
            v.pos = rect.topRight();   v.texCoord = uv.topRight();   quad << v;
            v.pos = rect.bottomLeft(); v.texCoord = uv.bottomLeft(); quad << v;
            v.pos = rect.bottomRight;  v.texCoord = uv.bottomRight;  quad << v;
            if(matrix) quad.transform(*matrix);
            return *this += quad;
        }
        /// Makes a 3D quad with indirect UV coords. The points p1...p4 are specified
        /// with a clockwise winding (use Vertex3Tex2BoundsRgba).
        Vertices &makeQuadIndirect(Vector3f const &p1, Vector3f const &p2,
                                   Vector3f const &p3, Vector3f const &p4,
                                   Vector4f const &color, Rectanglef const &uv,
                                   Vector4f const &uvBounds, Vector2f const &texSize) {
            Vertices quad;
            VertexType v;
            v.rgba = color;
            v.texBounds = uvBounds;
            v.texCoord[1] = texSize;
            v.pos = p1; v.texCoord[0] = uv.topLeft;      quad << v;
            v.pos = p2; v.texCoord[0] = uv.topRight();   quad << v;
            v.pos = p4; v.texCoord[0] = uv.bottomLeft(); quad << v;
            v.pos = p3; v.texCoord[0] = uv.bottomRight;  quad << v;
            return *this += quad;
        }
        Vertices &makeCubeIndirect(Vector3f const &minPoint,
                                   Vector3f const &maxPoint,
                                   Rectanglef const &uv,
                                   Vector4f const &uvBounds,
                                   Vector2f const &texSize,
                                   Vector4f const faceColors[6]) {
            // Back.
            makeQuadIndirect(minPoint,
                             Vector3f(maxPoint.x, minPoint.y, minPoint.z),
                             Vector3f(maxPoint.x, maxPoint.y, minPoint.z),
                             Vector3f(minPoint.x, maxPoint.y, minPoint.z),
                             faceColors[0], uv, uvBounds, texSize);

            // Front.
            makeQuadIndirect(Vector3f(minPoint.x, minPoint.y, maxPoint.z),
                             Vector3f(maxPoint.x, minPoint.y, maxPoint.z),
                             maxPoint,
                             Vector3f(minPoint.x, maxPoint.y, maxPoint.z),
                             faceColors[1], uv, uvBounds, texSize);

            // Left.
            makeQuadIndirect(Vector3f(minPoint.x, minPoint.y, maxPoint.z),
                             minPoint,
                             Vector3f(minPoint.x, maxPoint.y, minPoint.z),
                             Vector3f(minPoint.x, maxPoint.y, maxPoint.z),
                             faceColors[2], uv, uvBounds, texSize);

            // Right.
            makeQuadIndirect(Vector3f(maxPoint.x, minPoint.y, minPoint.z),
                             Vector3f(maxPoint.x, minPoint.y, maxPoint.z),
                             maxPoint,
                             Vector3f(maxPoint.x, maxPoint.y, minPoint.z),
                             faceColors[3], uv, uvBounds, texSize);

            // Floor.
            makeQuadIndirect(Vector3f(minPoint.x, maxPoint.y, minPoint.z),
                             Vector3f(maxPoint.x, maxPoint.y, minPoint.z),
                             maxPoint,
                             Vector3f(minPoint.x, maxPoint.y, maxPoint.z),
                             faceColors[4], uv, uvBounds, texSize);

            // Ceiling.
            makeQuadIndirect(Vector3f(minPoint.x, minPoint.y, maxPoint.z),
                             Vector3f(maxPoint.x, minPoint.y, maxPoint.z),
                             Vector3f(maxPoint.x, minPoint.y, minPoint.z),
                             minPoint,
                             faceColors[5], uv, uvBounds, texSize);

            return *this;
        }
        Vertices &makeRing(Vector2f const &center, float outerRadius, float innerRadius,
                           int divisions, Vector4f const &color, Rectanglef const &uv,
                           float innerTexRadius = -1) {
            if(innerTexRadius < 0) innerTexRadius = innerRadius / outerRadius;
            Vertices ring;
            VertexType v;
            v.rgba = color;
            for(int i = 0; i <= divisions; ++i) {
                float const ang = 2 * PI * (i == divisions? 0 : i) / divisions;
                Vector2f r(cos(ang), sin(ang));
                // Outer.
                v.pos = center + r * outerRadius;
                v.texCoord = uv.middle() + r * .5f * uv.size();
                ring << v;
                // Inner.
                v.pos = center + r * innerRadius;
                v.texCoord = uv.middle() + r * (.5f * innerTexRadius) * uv.size();
                ring << v;
            }
            return *this += ring;
        }
        Vertices &makeRing(Vector2f const &center, float outerRadius, float innerRadius,
                           int divisions, Vector4f const &color, Vector2f const &uv) {
            return makeRing(center, outerRadius, innerRadius, divisions, color, Rectanglef(uv, uv));
        }
        Vertices &makeFlexibleFrame(Rectanglef const &rect, float cornerThickness,
                                    Vector4f const &color, Rectanglef const &uv) {
            Vector2f const uvOff = uv.size() / 2;
            Vertices verts;
            VertexType v;

            v.rgba = color;

            // Top left corner.
            v.pos      = rect.topLeft;
            v.texCoord = uv.topLeft;
            verts << v;

            v.pos      = rect.topLeft + Vector2f(0, cornerThickness);
            v.texCoord = uv.topLeft + Vector2f(0, uvOff.y);
            verts << v;

            v.pos      = rect.topLeft + Vector2f(cornerThickness, 0);
            v.texCoord = uv.topLeft + Vector2f(uvOff.x, 0);
            verts << v;

            v.pos      = rect.topLeft + Vector2f(cornerThickness, cornerThickness);
            v.texCoord = uv.topLeft + uvOff;
            verts << v;

            // Top right corner.
            v.pos      = rect.topRight() + Vector2f(-cornerThickness, 0);
            v.texCoord = uv.topRight() + Vector2f(-uvOff.x, 0);
            verts << v;

            v.pos      = rect.topRight() + Vector2f(-cornerThickness, cornerThickness);
            v.texCoord = uv.topRight() + Vector2f(-uvOff.x, uvOff.y);
            verts << v;

            v.pos      = rect.topRight();
            v.texCoord = uv.topRight();
            verts << v;

            v.pos      = rect.topRight() + Vector2f(0, cornerThickness);
            v.texCoord = uv.topRight() + Vector2f(0, uvOff.y);
            verts << v;

            // Discontinuity.
            verts << v;
            verts << v;

            v.pos      = rect.topRight() + Vector2f(-cornerThickness, cornerThickness);
            v.texCoord = uv.topRight() + Vector2f(-uvOff.x, uvOff.y);
            verts << v;

            // Bottom right corner.
            v.pos      = rect.bottomRight + Vector2f(0, -cornerThickness);
            v.texCoord = uv.bottomRight + Vector2f(0, -uvOff.y);
            verts << v;

            v.pos      = rect.bottomRight + Vector2f(-cornerThickness, -cornerThickness);
            v.texCoord = uv.bottomRight + Vector2f(-uvOff.x, -uvOff.y);
            verts << v;

            v.pos      = rect.bottomRight;
            v.texCoord = uv.bottomRight;
            verts << v;

            v.pos      = rect.bottomRight + Vector2f(-cornerThickness, 0);
            v.texCoord = uv.bottomRight + Vector2f(-uvOff.x, 0);
            verts << v;

            // Discontinuity.
            verts << v;
            verts << v;

            v.pos      = rect.bottomRight + Vector2f(-cornerThickness, -cornerThickness);
            v.texCoord = uv.bottomRight + Vector2f(-uvOff.x, -uvOff.y);
            verts << v;

            // Bottom left corner.
            v.pos      = rect.bottomLeft() + Vector2f(cornerThickness, 0);
            v.texCoord = uv.bottomLeft() + Vector2f(uvOff.x, 0);
            verts << v;

            v.pos      = rect.bottomLeft() + Vector2f(cornerThickness, -cornerThickness);
            v.texCoord = uv.bottomLeft() + Vector2f(uvOff.x, -uvOff.y);
            verts << v;

            v.pos      = rect.bottomLeft();
            v.texCoord = uv.bottomLeft();
            verts << v;

            v.pos      = rect.bottomLeft() + Vector2f(0, -cornerThickness);
            v.texCoord = uv.bottomLeft() + Vector2f(0, -uvOff.y);
            verts << v;

            // Discontinuity.
            verts << v;
            verts << v;

            // Closing the loop.
            v.pos      = rect.bottomLeft() + Vector2f(cornerThickness, -cornerThickness);
            v.texCoord = uv.bottomLeft() + Vector2f(uvOff.x, -uvOff.y);
            verts << v;

            v.pos      = rect.topLeft + Vector2f(0, cornerThickness);
            v.texCoord = uv.topLeft + Vector2f(0, uvOff.y);
            verts << v;

            v.pos      = rect.topLeft + Vector2f(cornerThickness, cornerThickness);
            v.texCoord = uv.topLeft + Vector2f(uvOff.x, uvOff.y);
            verts << v;

            return *this += verts;
        }
    };

    static void concatenate(Vertices const &stripSequence, Vertices &destStrip)
    {
        if(!stripSequence.size()) return;
        if(!destStrip.isEmpty())
        {
            destStrip << destStrip.back();
            destStrip << stripSequence.front();
        }
        destStrip << stripSequence;
    }
};

} // namespace de

#endif // LIBGUI_VERTEXBUILDER_H
