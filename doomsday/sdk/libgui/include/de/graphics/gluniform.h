/** @file gluniform.h  GL uniform.
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

#ifndef LIBGUI_GLUNIFORM_H
#define LIBGUI_GLUNIFORM_H

#include <de/libcore.h>
#include <de/Vector>
#include <de/Matrix>
#include <de/Observers>

#include "../gui/libgui.h"

#include <QLatin1String>

namespace de {

class GLProgram;
class GLTexture;

/**
 * Constant variable or a sampler in a shader.
 *
 * GLUniform's public interface allows the uniform value to be manipulated like
 * any other native variable (assignment, arithmetic, etc.). Think of GLUniform
 * instances as a native manifestation of shader uniform/attribute variables.
 *
 * The value of the uniform is stored locally in the GLUniform instance. When
 * the uniform has been bound to programs and its value changes, the programs
 * are notified and they mark the uniform as changed. When the program is then
 * later taken into use, the updated value of the changed uniforms is sent to
 * GL.
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC GLUniform
{
public:
    enum Type
    {
        Int,
        UInt,
        Float,
        Vec2,
        Vec3,
        Vec4,
        Mat3,
        Mat4,
        Sampler2D,
        Vec3Array,
        Vec4Array,
        Mat4Array
    };

    /**
     * Notified when the value of the uniform changes.
     */
    DENG2_DEFINE_AUDIENCE2(ValueChange, void uniformValueChanged(GLUniform &))

    /**
     * Notified when the uniform instance is deleted.
     */
    DENG2_DEFINE_AUDIENCE2(Deletion, void uniformDeleted(GLUniform &))

public:
    GLUniform(char const *nameInShader, Type uniformType, duint elements = 1);

    void setName(char const *nameInShader);

    /**
     * Returns the name of the uniform as it appears in shaders.
     */
    QLatin1String name() const;

    /**
     * Returns the value type of the shader.
     */
    Type type() const;

    GLUniform &operator = (dint value);
    GLUniform &operator = (duint value);
    GLUniform &operator = (dfloat value);
    GLUniform &operator = (ddouble value);
    GLUniform &operator = (Vector2f const &vec);
    GLUniform &operator = (Vector3f const &vec);
    GLUniform &operator = (Vector4f const &vec);
    GLUniform &operator = (Matrix3f const &vec);
    GLUniform &operator = (Matrix4f const &mat);
    GLUniform &operator = (GLTexture const &texture);
    GLUniform &operator = (GLTexture const *texture);

    GLUniform &set(duint elementIndex, Vector3f const &vec);
    GLUniform &set(duint elementIndex, Vector4f const &vec);
    GLUniform &set(duint elementIndex, Matrix4f const &mat);

    operator dint() const              { return toInt(); }
    operator duint() const             { return toUInt(); }
    operator dfloat() const            { return toFloat(); }
    operator ddouble() const           { return ddouble(toFloat()); }
    operator Vector2f() const          { return toVector2f(); }
    operator Vector3f() const          { return toVector3f(); }
    operator Vector4f() const          { return toVector4f(); }
    operator Matrix3f const &() const  { return toMatrix3f(); }
    operator Matrix4f const &() const  { return toMatrix4f(); }
    operator GLTexture const *() const { return texture(); }

    dint toInt() const;
    duint toUInt() const;
    dfloat toFloat() const;
    Vector2f const &toVector2f() const;
    Vector3f const &toVector3f() const;
    Vector4f const &toVector4f() const;
    Matrix3f const &toMatrix3f() const;
    Matrix4f const &toMatrix4f() const;

    GLTexture const *texture() const;

    /**
     * Updates the value of the uniform in a particular GL program.
     *
     * @param program  GL program instance.
     */
    void applyInProgram(GLProgram &program) const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_GLUNIFORM_H
