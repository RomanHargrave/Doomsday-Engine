/** @file modelbank.h  Bank containing 3D models.
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

#ifndef LIBGUI_MODELBANK_H
#define LIBGUI_MODELBANK_H

#include <de/Bank>
#include <de/ModelDrawable>

namespace de {

/**
 * Bank of ModelDrawable instances.
 *
 * Loads model files using background tasks, as model files may contain large amounts of
 * geometry and preprocessing operations may be involved.
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC ModelBank : public Bank
{
public:
    /**
     * Interface for auxiliary data for a loaded model. @ingroup gl
     */
    class LIBGUI_PUBLIC IUserData
    {
    public:
        virtual ~IUserData() {}
        DENG2_AS_IS_METHODS()
    };

    typedef std::pair<ModelDrawable *, IUserData *> ModelWithData;

public:
    ModelBank();

    void add(DotPath const &id, String const &sourcePath);

    ModelDrawable &model(DotPath const &id);

    /**
     * Sets the user data of a loaded model.
     * @param id        Model identifier.
     * @param userData  User data object. Ownership taken.
     */
    void setUserData(DotPath const &id, IUserData *userData);

    IUserData const *userData(DotPath const &id) const;

    ModelWithData modelAndData(DotPath const &id);

protected:
    IData *loadFromSource(ISource &source);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_MODELBANK_H
