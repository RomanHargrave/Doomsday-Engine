/**
 * @file displaymode.cpp
 * Platform-independent display mode management. @ingroup gl
 *
 * @authors Copyright (c) 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "displaymode.h"
#include "displaymode_native.h"

#include <vector>
#include <set>
#include <algorithm>
#include <de/Log>

static bool inited = false;

static float differenceToOriginalHz(float hz);

struct Mode : public DisplayMode
{
    Mode()
    {
        memset(static_cast<DisplayMode*>(this), 0, sizeof(DisplayMode));
    }

    Mode(const DisplayMode& dm)
    {
        memcpy(static_cast<DisplayMode*>(this), &dm, sizeof(dm));
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
        if(height == b.height)
        {
            if(width == b.width)
            {
                if(depth == b.depth)
                {
                    // The refresh rate that more closely matches the original is preferable.
                    return differenceToOriginalHz(refreshRate) < differenceToOriginalHz(b.refreshRate);
                }
                return depth < b.depth;
            }
            return width < b.width;
        }
        return height < b.height;
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
        LOG_DEBUG("size: %i x %i x %i, rate: %.1f Hz, ratio: %i:%i")
                << width << height << depth << refreshRate << ratioX << ratioY;
    }
};

typedef std::set<Mode> Modes; // note: no duplicates
static Modes modes;
static Mode originalMode;
static bool captured;

static float differenceToOriginalHz(float hz)
{
    return qAbs(hz - originalMode.refreshRate);
}

int DisplayMode_Init(void)
{
    if(inited) return true;

    captured = false;
    DisplayMode_Native_Init();

    // This is used for sorting the mode set (Hz).
    originalMode = Mode::fromCurrent();

    for(int i = 0; i < DisplayMode_Native_Count(); ++i)
    {
        Mode mode(i);
        if(mode.depth < 16 || mode.width < 320 || mode.height < 240)
            continue; // This mode is not good.
        modes.insert(mode);
    }

    LOG_DEBUG("Current mode is:");
    originalMode.debugPrint();

    LOG_DEBUG("All available modes:");
    for(Modes::iterator i = modes.begin(); i != modes.end(); ++i)
    {
        i->debugPrint();
    }

    inited = true;
    return true;
}

void DisplayMode_Shutdown(void)
{
    if(!inited) return;

    LOG_INFO("Restoring original display mode due to shutdown.");

    // Back to the original mode.
    DisplayMode_Change(&originalMode, false /*release captured*/);

    modes.clear();

    DisplayMode_Native_Shutdown();
    captured = false;
    inited = false;
}

const DisplayMode* DisplayMode_OriginalMode(void)
{
    return &originalMode;
}

const DisplayMode* DisplayMode_Current(void)
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

const DisplayMode* DisplayMode_ByIndex(int index)
{
    Q_ASSERT(index >= 0);
    Q_ASSERT(index < (int) modes.size());

    int pos = 0;
    for(Modes::iterator i = modes.begin(); i != modes.end(); ++i, ++pos)
    {
        if(pos == index)
        {
            return &*i;
        }
    }

    Q_ASSERT(false);
    return 0; // unreachable
}

template <typename T>
T squared(const T& v) { return v * v; }

const DisplayMode* DisplayMode_FindClosest(int width, int height, int depth, float freq)
{
    int bestScore = -1;
    const DisplayMode* best = 0;

    for(Modes::iterator i = modes.begin(); i != modes.end(); ++i)
    {
        int score = squared(i->width - width) + squared(i->height - height) + squared(i->depth - depth);
        if(freq)
        {
            score += squared(i->refreshRate - freq);
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

boolean DisplayMode_IsEqual(const DisplayMode* a, const DisplayMode* b)
{
    return Mode(*a) == Mode(*b);
}

int DisplayMode_Change(const DisplayMode* mode, boolean shouldCapture)
{
    if(Mode::fromCurrent() == *mode && shouldCapture == captured)
    {
        LOG_DEBUG("DisplayMode: Requested mode is the same as current, ignoring.");

        // Already in this mode.
        return false;
    }
    captured = shouldCapture;
    return DisplayMode_Native_Change(mode, shouldCapture || (originalMode != *mode));
}
