/** @file compositetexture.h  Composite Texture.
 *
 * @author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DENG_RESOURCE_COMPOSITETEXTURE_H
#define DENG_RESOURCE_COMPOSITETEXTURE_H

#include <QList>
#include <QMultiMap>

#include <de/Reader>
#include <de/String>
#include <de/Vector>

#include "dd_types.h" // For lumpnum_t
#include "patchname.h"

namespace de {

/**
 * A logical texture composite of one or more @em component images.
 *
 * The component images are sorted according to the order in which they
 * should be composited, from bottom-most to top-most.
 *
 * @ingroup resource
 */
class CompositeTexture
{
public:
    /**
     * Flags denoting usage traits.
     */
    enum Flag {
        Custom = 0x1  ///< The texture does not originate from the current game.
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    /**
     * Archived format variants.
     */
    enum ArchiveFormat {
        DoomFormat,   ///< Format used by most id Tech 1 games.
        StrifeFormat  ///< Differs slightly from DoomFormat (omits unused values).
    };

    /**
     * Component image.
     */
    struct Component
    {
    protected:
        explicit Component(Vector2i const &origin = Vector2i());

    public:
        /// Origin of the top left corner of the component (in texture space units).
        Vector2i const &origin() const;

        bool operator == (Component const &other) const;

        inline bool operator != (Component const &other) const {
            return !(*this == other);
        }

        /// X-axis origin of the top left corner of the component (in texture space units).
        inline int xOrigin() const { return origin().x; }

        /// Y-axis origin of the top left corner of the component (in texture space units).
        inline int yOrigin() const { return origin().y; }

        /// Returns the number of the lump (file) containing the associated
        /// image; otherwise @c -1 (not found).
        lumpnum_t lumpNum() const;

        friend class CompositeTexture;

    private:
        Vector2i _origin;    ///< Top left corner in the texture coordinate space.
        lumpnum_t _lumpNum;  ///< Index of the lump containing the associated image.
    };
    typedef QList<Component> Components;

public:
    /**
     * Construct a default composite texture.
     */
    explicit CompositeTexture(String const &percentEncodedName = "",
                              de::Vector2i logicalDimensions   = de::Vector2i(),
                              Flags flags                      = 0);

    /**
     * Construct a composite texture by deserializing an archived id-tech 1
     * format definition from the Reader. Lump numbers will be looked up using
     * @a patchNames and any discrepancies or issues in the texture will be
     * logged about at this time.
     *
     * @param reader        Reader.
     * @param patchNames    List of component image names.
     * @param format        Format of the archived data.
     *
     * @return  The deserialized composite texture. Caller gets ownership.
     */
    static CompositeTexture *constructFrom(Reader &reader, QList<PatchName> patchNames,
                                           ArchiveFormat format = DoomFormat);

    /**
     * Compare two composite texture definitions for equality.
     *
     * @return @c true if the definitions are equal.
     */
    bool operator == (CompositeTexture const &other) const;

    inline bool operator != (CompositeTexture const &other) const {
        return !(*this == other);
    }

    /// Returns the percent-endcoded symbolic name of the texture.
    String percentEncodedName() const;

    /// Returns the percent-endcoded symbolic name of the texture.
    String const &percentEncodedNameRef() const;

    /// Returns the logical dimensions of the texture (in map space units).
    Vector2i const &logicalDimensions() const;

    /// Returns the logical width of the texture (in map space units).
    inline int logicalWidth() const { return logicalDimensions().x; }

    /// Returns the logical height of the texture (in map space units).
    inline int logicalHeight() const { return logicalDimensions().y; }

    /// Returns the pixel dimensions of the texture.
    Vector2i const &dimensions() const;

    /// Returns the pixel width of the texture.
    inline int width() const { return dimensions().x; }

    /// Returns the pixel height of the texture.
    inline int height() const { return dimensions().y; }

    /// Returns the associated "original index" for the texture.
    int origIndex() const;

    /// Change the "original index" value for the texture.
    void setOrigIndex(int newIndex);

    /// Returns the total number of component images.
    inline int componentCount() const { return components().count(); }

    /**
     * Provides access to the component images of the texture for efficent traversal.
     */
    Components const &components() const;

    /**
     * Returns @c true if the texture has flagged @a flagsToTest.
     */
    inline bool isFlagged(Flags flagsToTest) const { return (flags() & flagsToTest) != 0; }

    /**
     * Returns the flags for the composite texture.
     */
    Flags flags() const;

    /**
     * Change the composite texture's flags.
     *
     * @param flagsToChange  Flags to change the value of.
     * @param operation      Logical operation to perform on the flags.
     */
    void setFlags(Flags flagsToChange, de::FlagOp operation = de::SetFlags);

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(CompositeTexture::Flags)

typedef CompositeTexture::Component CompositeTextureComponent;

} // namespace de

#endif // DENG_RESOURCE_COMPOSITETEXTURE_H
