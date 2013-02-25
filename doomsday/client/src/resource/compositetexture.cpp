/** @file compositetexture.cpp Composite Texture Definition.
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include <QList>
#include <QRect>
#include <de/ByteRefArray>
#include <de/String>
#include <de/Reader>

#include "de_platform.h"
#include "de_filesys.h"

#include "resource/patch.h"
#include "resource/patchname.h"
#include "resource/compositetexture.h"

namespace de {

CompositeTexture::Component::Component(int xOrigin, int yOrigin)
    : origin_(xOrigin, yOrigin), lumpNum_(-1)
{}

Vector2i const &CompositeTexture::Component::origin() const {
    return origin_;
}

lumpnum_t CompositeTexture::Component::lumpNum() const {
    return lumpNum_;
}

CompositeTexture::CompositeTexture(String percentEncodedName,
    int width, int height, Flags _flags)
    : name(percentEncodedName),
      flags_(_flags),
      logicalDimensions_(width, height),
      dimensions_(0, 0),
      origIndex_(-1)
{}

String CompositeTexture::percentEncodedName() const {
    return name;
}

String const &CompositeTexture::percentEncodedNameRef() const {
    return name;
}

Vector2i const &CompositeTexture::logicalDimensions() const {
    return logicalDimensions_;
}

Vector2i const &CompositeTexture::dimensions() const {
    return dimensions_;
}

CompositeTexture::Components const &CompositeTexture::components() const {
    return components_;
}

CompositeTexture::Flags &CompositeTexture::flags() {
    return flags_;
}

int CompositeTexture::origIndex() const {
    return origIndex_;
}

void CompositeTexture::setOrigIndex(int newIndex) {
    origIndex_ = newIndex;
}

static String readAndPercentEncodeRawName(Reader &from)
{
    /// @attention The raw ASCII name is not necessarily terminated.
    char asciiName[9];
    for(int i = 0; i < 8; ++i) { from >> asciiName[i]; }
    asciiName[8] = 0;

    // WAD format allows characters not typically permitted in native paths.
    // To achieve uniformity we apply a percent encoding to the "raw" names.
    return QString(QByteArray(asciiName).toPercentEncoding());
}

CompositeTexture *CompositeTexture::constructFrom(Reader &reader,
    QList<PatchName> patchNames, ArchiveFormat format)
{
    CompositeTexture *pctex = new CompositeTexture;

    // First is the raw name.
    pctex->name = readAndPercentEncodeRawName(reader);

    // Next is some unused junk from a previous format version.
    dint16 unused16;
    reader >> unused16;

    // Next up are scale and logical dimensions.
    byte scale[2]; /** @todo ZDoom defines these otherwise unused bytes as a
                       scale factor (div 8). We could interpret this also. */
    dint16 dimensions[2];

    reader >> scale[0] >> scale[1] >> dimensions[0] >> dimensions[1];

    // We'll initially accept these values as logical dimensions. However
    // we may need to adjust once we've checked the patch dimensions.
    pctex->logicalDimensions_.x = dimensions[0];
    pctex->logicalDimensions_.y = dimensions[1];

    pctex->dimensions_ = pctex->logicalDimensions_;

    if(format == DoomFormat)
    {
        // Next is some more unused junk from a previous format version.
        dint32 unused32;
        reader >> unused32;
    }

    /*
     * Finally, read the component images.
     * In the process we'll determine the final logical dimensions of the
     * texture by compositing the geometry of the component images.
     */
    dint16 componentCount;
    reader >> componentCount;

    QRect geom(QPoint(0, 0), QSize(pctex->logicalDimensions_.x,
                                   pctex->logicalDimensions_.y));

    int foundComponentCount = 0;
    for(dint16 i = 0; i < componentCount; ++i)
    {
        Component patch;

        dint16 origin16[2];
        reader >> origin16[0] >> origin16[1];
        patch.origin_.x = origin16[0];
        patch.origin_.y = origin16[1];

        dint16 pnamesIndex;
        reader >> pnamesIndex;

        if(pnamesIndex < 0 || pnamesIndex >= patchNames.count())
        {
            LOG_WARNING("Invalid PNAMES index %i in composite texture \"%s\", ignoring.")
                << pnamesIndex << pctex->name;
        }
        else
        {
            patch.lumpNum_ = patchNames[pnamesIndex].lumpNum();

            if(patch.lumpNum() >= 0)
            {
                /// There is now one more found component.
                foundComponentCount += 1;

                de::File1 &file = App_FileSystem().nameIndex().lump(patch.lumpNum_);

                // If this a "custom" component - the whole texture is.
                if(file.container().hasCustom())
                {
                    pctex->flags_ |= Custom;
                }

                // If this is a Patch - unite the geometry of the component.
                ByteRefArray fileData = ByteRefArray(file.cache(), file.size());
                if(Patch::recognize(fileData))
                {
                    try
                    {
                        Patch::Metadata info = Patch::loadMetadata(fileData);
                        geom |= QRect(QPoint(patch.origin_.x, patch.origin_.y),
                                      QSize(info.dimensions.x, info.dimensions.y));
                    }
                    catch(IByteArray::OffsetError const &)
                    {
                        LOG_WARNING("Component image \"%s\" (#%i) does not appear to be a valid Patch. "
                                    "It may be missing from composite texture \"%s\".")
                            << patchNames[pnamesIndex].percentEncodedNameRef() << i
                            << pctex->name;
                    }
                }
                file.unlock();
            }
            else
            {
                LOG_WARNING("Missing component image \"%s\" (#%i) in composite texture \"%s\", ignoring.")
                    << patchNames[pnamesIndex].percentEncodedNameRef() << i
                    << pctex->name;
            }
        }

        // Skip the unused "step dir" and "color map" values.
        reader >> unused16 >> unused16;

        // Add this component.
        pctex->components_.push_back(patch);
    }

    // Clip and apply the final height.
    if(geom.top()  < 0) geom.setTop(0);
    if(geom.height() > pctex->logicalDimensions_.y)
        pctex->dimensions_.y = geom.height();

    if(!foundComponentCount)
    {
        LOG_WARNING("Zero valid component images in composite texture %s, ignoring.")
            << pctex->name;
    }

    return pctex;
}

} // namespace de
