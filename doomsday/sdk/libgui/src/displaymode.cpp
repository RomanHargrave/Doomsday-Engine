/** @file displaymode.cpp  Platform-independent display mode management.
 * @ingroup gl
 *
 * @authors Copyright (c) 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/gui/displaymode.h"
#include "de/gui/displaymode_native.h"

#include <de/App>
#include <de/Record>
#include <de/FunctionValue>
#include <de/DictionaryValue>
#include <de/ArrayValue>
#include <de/TextValue>
#include <de/NumberValue>

#include <vector>
#include <set>
#include <algorithm>
#include <de/math.h>
#include <de/Log>

namespace de {

static bool inited = false;
static DisplayColorTransfer originalColorTransfer;
static de::Binder binder;

static float differenceToOriginalHz(float hz);

namespace internal {

struct Mode : public DisplayMode
{
    Mode()
    {
        de::zapPtr(static_cast<DisplayMode *>(this));
    }

    Mode(const DisplayMode& dm)
    {
        memcpy(static_cast<DisplayMode *>(this), &dm, sizeof(dm));
    }

    Mode(int i)
    {
        DisplayMode_Native_GetMode(i, this);
        updateRatio();
    }

    static Mode fromCurrent()
    {
        Mode m;
        DisplayMode_Native_GetCurrentMode(&m);
        m.updateRatio();
        return m;
    }

    bool operator == (const Mode& other) const
    {
        return width == other.width && height == other.height &&
               depth == other.depth && refreshRate == other.refreshRate;
    }

    bool operator != (const Mode& other) const
    {
        return !(*this == other);
    }

    bool operator < (const Mode& b) const
    {
        if(width == b.width)
        {
            if(height == b.height)
            {
                if(depth == b.depth)
                {
                    // The refresh rate that more closely matches the original is preferable.
                    return differenceToOriginalHz(refreshRate) < differenceToOriginalHz(b.refreshRate);
                }
                return depth < b.depth;
            }
            return height < b.height;
        }
        return width < b.width;
    }

    void updateRatio()
    {
        ratioX = width;
        ratioY = height;

        float fx;
        float fy;
        if(width > height)
        {
            fx = width/float(height);
            fy = 1.f;
        }
        else
        {
            fx = 1.f;
            fy = height/float(width);
        }

        // Multiply until we arrive at a close enough integer ratio.
        for(int mul = 2; mul < qMin(width, height); ++mul)
        {
            float rx = fx*mul;
            float ry = fy*mul;
            if(qAbs(rx - qRound(rx)) < .01f && qAbs(ry - qRound(ry)) < .01f)
            {
                // This seems good.
                ratioX = qRound(rx);
                ratioY = qRound(ry);
                break;
            }
        }

        if(ratioX == 8 && ratioY == 5)
        {
            // This is commonly referred to as 16:10.
            ratioX *= 2;
            ratioY *= 2;
        }
    }

    void debugPrint() const
    {
        LOG_GL_VERBOSE("size: %i x %i x %i, rate: %.1f Hz, ratio: %i:%i")
                << width << height << depth << refreshRate << ratioX << ratioY;
    }
};

} // namespace internal

using namespace internal;

typedef std::set<Mode> Modes; // note: no duplicates

static Modes modes;
static Mode originalMode;
static bool captured;

static float differenceToOriginalHz(float hz)
{
    return qAbs(hz - originalMode.refreshRate);
}

static de::Value *Function_DisplayMode_OriginalMode(de::Context &, de::Function::ArgumentValues const &)
{
    using de::NumberValue;
    using de::TextValue;

    DisplayMode const *mode = DisplayMode_OriginalMode();

    de::DictionaryValue *dict = new de::DictionaryValue;
    dict->add(new TextValue("width"),       new NumberValue(mode->width));
    dict->add(new TextValue("height"),      new NumberValue(mode->height));
    dict->add(new TextValue("depth"),       new NumberValue(mode->depth));
    dict->add(new TextValue("refreshRate"), new NumberValue(mode->refreshRate));

    de::ArrayValue *ratio = new de::ArrayValue;
    *ratio << NumberValue(mode->ratioX) << NumberValue(mode->ratioY);
    dict->add(new TextValue("ratio"), ratio);

    return dict;
}

} // namespace de

using namespace de;

int DisplayMode_Init(void)
{
    if(inited) return true;

    captured = false;
    DisplayMode_Native_Init();
#if defined(MACOSX) || defined(UNIX)
    DisplayMode_SaveOriginalColorTransfer();
#endif

    // This is used for sorting the mode set (Hz).
    originalMode = Mode::fromCurrent();

    for(int i = 0; i < DisplayMode_Native_Count(); ++i)
    {
        Mode mode(i);
        if(mode.depth < 16 || mode.width < 320 || mode.height < 240)
            continue; // This mode is not good.
        modes.insert(mode);
    }

    LOG_GL_VERBOSE("Current mode is:");
    originalMode.debugPrint();

    LOG_GL_VERBOSE("All available modes:");
    for(Modes::iterator i = modes.begin(); i != modes.end(); ++i)
    {
        i->debugPrint();
    }

    // Script bindings.
    binder.initNew() << DENG2_FUNC_NOARG(DisplayMode_OriginalMode, "originalMode");
    de::App::scriptSystem().addNativeModule("DisplayMode", binder.module());
    binder.module().addNumber("DPI_FACTOR", 1.0);

    inited = true;
    return true;
}

void DisplayMode_Shutdown(void)
{
    if(!inited) return;

    binder.deinit();

    LOG_GL_NOTE("Restoring original display mode due to shutdown");

    // Back to the original mode.
    DisplayMode_Change(&originalMode, false /*release captured*/);

    modes.clear();

    DisplayMode_Native_Shutdown();
    captured = false;

    DisplayMode_Native_SetColorTransfer(&originalColorTransfer);

    inited = false;
}

void DisplayMode_SaveOriginalColorTransfer(void)
{
    DisplayMode_Native_GetColorTransfer(&originalColorTransfer);
}

DisplayMode const *DisplayMode_OriginalMode(void)
{
    return &originalMode;
}

DisplayMode const *DisplayMode_Current(void)
{
    static Mode currentMode;
    // Update it with current mode.
    currentMode = Mode::fromCurrent();
    return &currentMode;
}

int DisplayMode_Count(void)
{
    return (int) modes.size();
}

DisplayMode const *DisplayMode_ByIndex(int index)
{
    DENG2_ASSERT(index >= 0);
    DENG2_ASSERT(index < (int) modes.size());

    int pos = 0;
    for(Modes::iterator i = modes.begin(); i != modes.end(); ++i, ++pos)
    {
        if(pos == index)
        {
            return &*i;
        }
    }

    DENG2_ASSERT(false);
    return 0; // unreachable
}

DisplayMode const *DisplayMode_FindClosest(int width, int height, int depth, float freq)
{
    int bestScore = -1;
    DisplayMode const *best = 0;

    for(Modes::iterator i = modes.begin(); i != modes.end(); ++i)
    {
        int score = de::squared(i->width  - width) +
                    de::squared(i->height - height) +
                    de::squared(i->depth  - depth);
        if(freq >= 1)
        {
            score += de::squared(i->refreshRate - freq);
        }

        // Note: The first mode to hit the lowest score wins; if there are many modes
        // with the same score, the first one will be chosen. Particularly if the
        // frequency has not been specified, the sort order of the modes defines which
        // one is picked.
        if(!best || score < bestScore)
        {
            bestScore = score;
            best = &*i;
        }
    }
    return best;
}

int DisplayMode_IsEqual(DisplayMode const *a, DisplayMode const *b)
{
    if(!a || !b) return true; // Cannot compare against nothing.
    return Mode(*a) == Mode(*b);
}

int DisplayMode_Change(DisplayMode const *mode, int shouldCapture)
{
    if(Mode::fromCurrent() == *mode && !shouldCapture == !captured)
    {
        LOG_AS("DisplayMode");
        LOGDEV_GL_XVERBOSE("Requested mode is the same as current, ignoring request");

        // Already in this mode.
        return false;
    }
    captured = shouldCapture;
    return DisplayMode_Native_Change(mode, shouldCapture || (originalMode != *mode));
}

static inline de::duint16 intensity8To16(de::duint8 b)
{
    return (b << 8) | b; // 0xFF => 0xFFFF
}

void DisplayMode_GetColorTransfer(DisplayColorTransfer *colors)
{
    DisplayColorTransfer mapped;
    DisplayMode_Native_GetColorTransfer(&mapped);

    // Factor out the original color transfer function, which may be set up
    // specifically by the user.
    for(int i = 0; i < 256; ++i)
    {
#define LINEAR_UNMAP(i, c) ( (unsigned short) \
    de::clamp(0.f, float(mapped.table[i]) / float(originalColorTransfer.table[i]) * intensity8To16(c), 65535.f) )
        colors->table[i]       = LINEAR_UNMAP(i,       i);
        colors->table[i + 256] = LINEAR_UNMAP(i + 256, i);
        colors->table[i + 512] = LINEAR_UNMAP(i + 512, i);
    }
}

void DisplayMode_SetColorTransfer(DisplayColorTransfer const *colors)
{
    DisplayColorTransfer mapped;

    // Factor in the original color transfer function, which may be set up
    // specifically by the user.
    for(int i = 0; i < 256; ++i)
    {
#define LINEAR_MAP(i, c) ( (unsigned short) \
    de::clamp(0.f, float(colors->table[i]) / float(intensity8To16(c)) * originalColorTransfer.table[i], 65535.f) )
        mapped.table[i]       = LINEAR_MAP(i,       i);
        mapped.table[i + 256] = LINEAR_MAP(i + 256, i);
        mapped.table[i + 512] = LINEAR_MAP(i + 512, i);
    }

    DisplayMode_Native_SetColorTransfer(&mapped);
}
