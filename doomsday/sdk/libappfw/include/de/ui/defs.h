/** @file ui/defs.h  Common de::ui namespace definitions.
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

#ifndef LIBAPPFW_UI_DEFS_H
#define LIBAPPFW_UI_DEFS_H

#include "../libappfw.h"
#include <QFlags>
#include <de/math.h>

namespace de {
namespace ui {

/**
 * Basic directions.
 */
enum Direction
{
    Left,
    Up,
    Right,
    Down,
    NoDirection
};

inline Direction opposite(Direction dir) {
    switch(dir) {
    case Left:        return Right;
    case Right:       return Left;
    case Up:          return Down;
    case Down:        return Up;
    case NoDirection: return NoDirection;
    }
}

inline bool isHorizontal(Direction dir) {
    return dir == ui::Left || dir == ui::Right;
}

inline bool isVertical(Direction dir) {
    return dir == ui::Up || dir == ui::Down;
}

/**
 * Flags for specifying alignment.
 */
enum AlignmentFlag
{
    AlignTop         = 0x1,
    AlignBottom      = 0x2,
    AlignLeft        = 0x4,
    AlignRight       = 0x8,

    AlignTopLeft     = AlignTop | AlignLeft,
    AlignTopRight    = AlignTop | AlignRight,
    AlignBottomLeft  = AlignBottom | AlignLeft,
    AlignBottomRight = AlignBottom | AlignRight,

    AlignCenter      = 0,

    DefaultAlignment = AlignCenter
};
Q_DECLARE_FLAGS(Alignment, AlignmentFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(Alignment)

template <typename SizeType, typename RectType>
typename RectType::Corner applyAlignment(Alignment align, SizeType const &size, RectType const &bounds)
{
    typename RectType::Corner p = bounds.topLeft;

    if(align.testFlag(AlignRight))
    {
        p.x += int(bounds.width()) - int(size.x);
    }
    else if(!align.testFlag(AlignLeft))
    {
        p.x += (int(bounds.width()) - int(size.x)) / 2;
    }

    if(align.testFlag(AlignBottom))
    {
        p.y += int(bounds.height()) - int(size.y);
    }
    else if(!align.testFlag(AlignTop))
    {
        p.y += de::floor((double(bounds.height()) - double(size.y)) / 2.0);
    }

    return p;
}

template <typename RectType, typename BoundsRectType>
void applyAlignment(Alignment align, RectType &alignedRect, BoundsRectType const &boundsRect)
{
    alignedRect.moveTopLeft(applyAlignment(align, alignedRect.size(), boundsRect));
}

/**
 * Flags for specifying content fitting/scaling.
 */
enum ContentFitFlag
{
    OriginalSize        = 0,
    FitToWidth          = 0x1,
    FitToHeight         = 0x2,
    OriginalAspectRatio = 0x4,

    FitToSize = FitToWidth | FitToHeight
};
Q_DECLARE_FLAGS(ContentFit, ContentFitFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(ContentFit)

/**
 * Policy for controlling size.
 */
enum SizePolicy
{
    Fixed,  ///< Size is fixed, content positioned inside.
    Filled, ///< Size is fixed, content expands to fill entire area.
    Expand  ///< Size depends on content, expands/contracts to fit.
};

} // namespace ui
} // namespace de

#endif // LIBAPPFW_UI_DEFS_H
