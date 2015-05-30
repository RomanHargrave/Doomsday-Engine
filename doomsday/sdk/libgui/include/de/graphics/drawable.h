/** @file drawable.h  Drawable object with buffers, programs and states.
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

#ifndef LIBGUI_DRAWABLE_H
#define LIBGUI_DRAWABLE_H

#include <QSet>

#include <de/libcore.h>
#include <de/Asset>
#include <de/String>

#include "../GLBuffer"
#include "../GLProgram"
#include "../GLState"
#include "../GLUniform"

namespace de {

/**
 * Drawable object with buffers, programs and states.
 *
 * This is the higher level, flexible, user-friendly combination of the lower
 * level GL classes. It is not mandatory to use this class for drawing; one can
 * always use the lower level classes directly.
 *
 * Drawable combines a set of GLBuffer instances with a set of GL programs and
 * GL states. There can be multiple (named) instances of buffers, programs, and
 * states in a Drawable. While each buffer must have a program, having a state
 * is optional. Each buffer can choose which of the Drawable's programs and
 * states is used with the buffer. It is also possible to assign external
 * programs and states for use with buffers. A default program (with id 0) is
 * always present in a Drawable.
 *
 * Example use cases:
 * - draw a single buffer with a program using the current GL state
 * - draw a single buffer with one of many alternative programs
 * - draw multiple buffers with the same program
 * - draw some buffers with a custom GL state and the rest with the
 *   current state
 * - draw a mixture of multiple buffers and programs, each with a
 *   custom state (e.g., a set of 3D sub-models representing an object
 *   in the game)
 *
 * The buffers are drawn in the order of ascending identifiers. If no state has
 * been defined for a buffer, the topmost state on the GL state stack is used
 * for drawing that particular buffer.
 *
 * GLUniform instances can be used externally to this class to manipulate the
 * uniforms in the programs. The user is expected to provide the static/dynamic
 * vertex data for the buffers.
 *
 * Drawable is an AssetGroup: it cannot be drawn until all the contained
 * buffers and programs are ready. It is allowed to insert further assets into
 * the group if they should be present before drawing is allowed (e.g.,
 * textures).
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC Drawable : public AssetGroup
{
    DENG2_NO_COPY  (Drawable)
    DENG2_NO_ASSIGN(Drawable)

public:
    /// User-provided (nonzero) identifier. Buffer identifiers define the
    /// drawing order of the buffers. (Note that this is not a de::Id.)
    typedef duint Id;

    /// User-provided name. Buffers, programs, and states can optionally be
    /// also identified using textual names.
    typedef String Name;

    typedef QList<Id> Ids;

public:
    Drawable();

    /**
     * Clears the drawable. All buffers, programs, and states are deleted.
     * The default buffer and program are cleared.
     */
    void clear();

    Ids allBuffers() const;
    Ids allPrograms() const;
    Ids allStates() const;

    bool hasBuffer(Id id) const;

    /**
     * Finds an existing buffer.
     * @param id  Identifier of the buffer.
     * @return GL buffer.
     */
    GLBuffer &buffer(Id id = 1) const;

    template <typename VBType>
    VBType &buffer(Id id = 1) const {
        DENG2_ASSERT(dynamic_cast<VBType *>(&buffer(id)) != 0);
        return static_cast<VBType &>(buffer(id));
    }

    GLBuffer &buffer(Name const &bufferName) const;

    Id bufferId(Name const &bufferName) const;

    /**
     * Finds an exising program.
     * @param id  Identifier of the program.
     * @return GL program.
     */
    GLProgram &program(Id id = 0) const;

    GLProgram &program(Name const &programName) const;

    Id programId(Name const &programName) const;

    GLProgram const &programForBuffer(Id bufferId) const;

    GLProgram const &programForBuffer(Name const &bufferName) const;

    /**
     * Finds an existing state.
     * @param id  Identifier of the state.
     * @return GL state.
     */
    GLState &state(Id id) const;

    GLState &state(Name const &stateName) const;

    Id stateId(Name const &stateName) const;

    GLState const *stateForBuffer(Id bufferId) const;

    GLState const *stateForBuffer(Name const &bufferName) const;

    /**
     * Adds a new buffer or replaces an existing one. The buffer will use the
     * default program.
     *
     * @param id      Identifier of the buffer.
     * @param buffer  GL buffer. Drawable gets ownership.
     */
    void addBuffer(Id id, GLBuffer *buffer);

    Id addBuffer(Name const &bufferName, GLBuffer *buffer);

    /**
     * Adds a new buffer, reserving an unused identifier for it. The chosen
     * identifier is larger than any of the buffer identifiers currently in
     * use. The buffer will use the default program.
     *
     * @param buffer  GL buffer. Drawable gets ownership.
     *
     * @return  Identifier chosen for the buffer.
     */
    Id addBuffer(GLBuffer *buffer);

    /**
     * Adds a new buffer, reserving an unused identifier for it. The chosen
     * identifier is larger than any of the buffer identifiers currently in
     * use. The buffer will use a new program.
     *
     * @param buffer       GL buffer. Drawable gets ownership.
     * @param programName  Name for the program.
     *
     * @return  Identifier chosen for the buffer.
     */
    Id addBufferWithNewProgram(GLBuffer *buffer, Name const &programName = "");

    void addBufferWithNewProgram(Id id, GLBuffer *buffer, Name const &programName = "");

    Id addBufferWithNewProgram(Name const &bufferName, GLBuffer *buffer, Name const &programName = "");

    /**
     * Creates a program or replaces an existing one with a blank program.
     * @param id  Identifier of the program. Cannot be zero.
     * @return GL program.
     */
    GLProgram &addProgram(Id id);

    Id addProgram(Name const &programName);

    /**
     * Creates a state or replaces an existing one with a default state.
     * @param id     Identifier of the state.
     * @param state  State to add.
     * @return GL state.
     */
    GLState &addState(Id id, GLState const &state = GLState());

    Id addState(Name const &stateName, GLState const &state = GLState());

    void removeBuffer(Id id);
    void removeProgram(Id id);
    void removeState(Id id);

    void removeBuffer(Name const &bufferName);
    void removeProgram(Name const &programName);
    void removeState(Name const &stateName);

    /**
     * Sets the program to be used with a buffer.
     *
     * @param bufferId  Buffer whose program is being set.
     * @param program   GL program instance. If not owned by Drawable,
     *                  must not be destroyed while Drawable is using it.
     */
    void setProgram(Id bufferId, GLProgram &program);

    void setProgram(Id bufferId, Name const &programName);
    void setProgram(Name const &bufferName, GLProgram &program);
    void setProgram(Name const &bufferName, Name const &programName);

    /**
     * Sets the program to be used with all buffers.
     *
     * @param program  GL program instance. If not owned by Drawable,
     *                 must not be destroyed while Drawable is using it.
     */
    void setProgram(GLProgram &program);

    void setProgram(Name const &programName);

    /**
     * Sets the state to be used with a buffer.
     *
     * @param bufferId  Buffer whose state is being set.
     * @param state     GL state instance. If not owned by Drawable,
     *                  must not be destroyed while Drawable is using it.
     */
    void setState(Id bufferId, GLState &state);

    void setState(Name const &bufferName, GLState &state);
    void setState(Id bufferId, Name const &stateName);
    void setState(Name const &bufferName, Name const &stateName);

    /**
     * Sets the state to be used with all buffers.
     *
     * @param state  GL state instance. If not owned by Drawable,
     *               must not be destroyed while Drawable is using it.
     */
    void setState(GLState &state);

    void setState(Name const &stateName);

    /**
     * Removes the state configured for a buffer. When the buffer is drawn, the
     * current GL state from the state stack is used instead of some custom
     * state.
     *
     * @param bufferId  Buffer whose state is to be unset.
     */
    void unsetState(Id bufferId);

    void unsetState(Name const &bufferName);

    /**
     * Removes the state configured for all buffers. When drawing, the current
     * GL state from the state stack is used instead of some custom state.
     */
    void unsetState();

    /**
     * Draws all the buffers using the selected program(s) and state(s).
     * Drawing is only allowed when all assets are ready.
     */
    virtual void draw() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_DRAWABLE_H
