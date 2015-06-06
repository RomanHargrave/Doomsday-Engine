/** @file automapstyle.cpp  Style configuration for AutomapWidget.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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

#include "common.h"
#include "hud/automapstyle.h"

#include "hu_stuff.h"
#include "hud/widgets/automapwidget.h"  /// @ref automapWidgetFlags

using namespace de;

DENG2_PIMPL_NOREF(AutomapStyle)
{
    automapcfg_lineinfo_t lineInfo[AUTOMAPCFG_MAX_LINEINFO];
    uint lineInfoCount = 0;

    svgid_t playerSvg = 0;
    svgid_t thingSvg = 0;

    automapcfg_lineinfo_t mapObjectInfo[NUM_MAP_OBJECTLISTS];

    Instance() { de::zap(mapObjectInfo); }

    void reset()
    {
        de::zap(lineInfo);
        lineInfoCount = 0;

        playerSvg = 0;
        thingSvg  = 0;

        de::zap(mapObjectInfo);
    }

    automapcfg_lineinfo_t *findLineInfo(int reqAutomapFlags, int reqSpecial, int reqSided,
        int reqNotFlagged)
    {
        for(uint i = 0; i < lineInfoCount; ++i)
        {
            automapcfg_lineinfo_t *info = &lineInfo[i];

            if(info->reqSpecial      != reqSpecial) continue;
            if(info->reqAutomapFlags != reqAutomapFlags) continue;
            if(info->reqSided        != reqSided) continue;
            if(info->reqNotFlagged   != reqNotFlagged) continue;

            return info;  // This is the one.
        }
        return nullptr;  // Not found.
    }

};

AutomapStyle::AutomapStyle() : d(new Instance)
{}

AutomapStyle::~AutomapStyle()
{}

void AutomapStyle::newLineInfo(int reqAutomapFlags, int reqSpecial, int reqSided, int reqNotFlagged,
                 float r, float g, float b, float a, blendmode_t blendmode, glowtype_t glowType,
                 float glowStrength, float glowSize, dd_bool scaleGlowWithView)
{
    DENG2_ASSERT(reqSpecial >= 0)
    DENG2_ASSERT(reqSided >= 0 && reqSided <= 2);

    // Later re-registrations override earlier ones.
    automapcfg_lineinfo_t *info = d->findLineInfo(reqAutomapFlags, reqSpecial, reqSided, reqNotFlagged);
    if(!info)
    {
        // Any room for a new special line?
        if(d->lineInfoCount >= AUTOMAPCFG_MAX_LINEINFO)
            throw Error("AutomapStyle::d->newLineInfo", "No available slot.");

        info = &d->lineInfo[d->lineInfoCount++];
    }

    info->reqAutomapFlags   = reqAutomapFlags;
    info->reqSpecial        = reqSpecial;
    info->reqSided          = reqSided;
    info->reqNotFlagged     = reqNotFlagged;

    info->rgba[0]       = de::clamp(0.f, r, 1.f);
    info->rgba[1]       = de::clamp(0.f, g, 1.f);
    info->rgba[2]       = de::clamp(0.f, b, 1.f);
    info->rgba[3]       = de::clamp(0.f, a, 1.f);
    info->glow          = glowType;
    info->glowStrength  = de::clamp(0.f, glowStrength, 1.f);
    info->glowSize      = glowSize;
    info->scaleWithView = scaleGlowWithView;
    info->blendMode     = blendmode;
}

automapcfg_lineinfo_t const &AutomapStyle::lineInfo(int lineType)
{
    DENG2_ASSERT(lineType >= 0 && lineType < NUM_MAP_OBJECTLISTS);
    return d->mapObjectInfo[lineType];
}

automapcfg_lineinfo_t const *AutomapStyle::tryFindLineInfo(automapcfg_objectname_t name) const
{
    if(name == AMO_NONE) return nullptr;  // Ignore

    if(name < 0 || name >= AMO_NUMOBJECTS)
        throw Error("AutomapStyle::tryFindLineInfo", "Unknown object #" + String::number((int) name));

    switch(name)
    {
    case AMO_UNSEENLINE:        return &d->mapObjectInfo[MOL_LINEDEF_UNSEEN  ];
    case AMO_SINGLESIDEDLINE:   return &d->mapObjectInfo[MOL_LINEDEF         ];
    case AMO_TWOSIDEDLINE:      return &d->mapObjectInfo[MOL_LINEDEF_TWOSIDED];
    case AMO_FLOORCHANGELINE:   return &d->mapObjectInfo[MOL_LINEDEF_FLOOR   ];
    case AMO_CEILINGCHANGELINE: return &d->mapObjectInfo[MOL_LINEDEF_CEILING ];

    default: break;
    }

    return nullptr;
}

automapcfg_lineinfo_t const *AutomapStyle::tryFindLineInfo_special(int special,
    int flags, Sector const *frontsector, Sector const *backsector, int automapFlags) const
{
    if(special > 0)
    {
        for(uint i = 0; i < d->lineInfoCount; ++i)
        {
            automapcfg_lineinfo_t const *info = &d->lineInfo[i];

            // Special restriction?
            if(info->reqSpecial != special) continue;

            // Sided restriction?
            if((info->reqSided == 1 && backsector && frontsector) ||
               (info->reqSided == 2 && (!backsector || !frontsector)))
                continue;

            // Line flags restriction?
            if(info->reqNotFlagged && (flags & info->reqNotFlagged)) continue;

            // Automap flags restriction?
            if(info->reqAutomapFlags != 0 && !(automapFlags & info->reqAutomapFlags)) continue;

            // This is the one!
            return info;
        }
    }
    return nullptr;  // Not found.
}

void AutomapStyle::applyDefaults()
{
    d->reset();

    d->mapObjectInfo[MOL_LINEDEF_UNSEEN].glow = GLOW_NONE;
    d->mapObjectInfo[MOL_LINEDEF_UNSEEN].glowStrength = 1;
    d->mapObjectInfo[MOL_LINEDEF_UNSEEN].glowSize = 10;
    d->mapObjectInfo[MOL_LINEDEF_UNSEEN].blendMode = BM_NORMAL;
    d->mapObjectInfo[MOL_LINEDEF_UNSEEN].scaleWithView = false;
    d->mapObjectInfo[MOL_LINEDEF_UNSEEN].rgba[0] = 1;
    d->mapObjectInfo[MOL_LINEDEF_UNSEEN].rgba[1] = 1;
    d->mapObjectInfo[MOL_LINEDEF_UNSEEN].rgba[2] = 1;
    d->mapObjectInfo[MOL_LINEDEF_UNSEEN].rgba[3] = 1;
    d->mapObjectInfo[MOL_LINEDEF].glow = GLOW_NONE;
    d->mapObjectInfo[MOL_LINEDEF].glowStrength = 1;
    d->mapObjectInfo[MOL_LINEDEF].glowSize = 10;
    d->mapObjectInfo[MOL_LINEDEF].blendMode = BM_NORMAL;
    d->mapObjectInfo[MOL_LINEDEF].scaleWithView = false;
    d->mapObjectInfo[MOL_LINEDEF].rgba[0] = 1;
    d->mapObjectInfo[MOL_LINEDEF].rgba[1] = 1;
    d->mapObjectInfo[MOL_LINEDEF].rgba[2] = 1;
    d->mapObjectInfo[MOL_LINEDEF].rgba[3] = 1;
    d->mapObjectInfo[MOL_LINEDEF_TWOSIDED].glow = GLOW_NONE;
    d->mapObjectInfo[MOL_LINEDEF_TWOSIDED].glowStrength = 1;
    d->mapObjectInfo[MOL_LINEDEF_TWOSIDED].glowSize = 10;
    d->mapObjectInfo[MOL_LINEDEF_TWOSIDED].blendMode = BM_NORMAL;
    d->mapObjectInfo[MOL_LINEDEF_TWOSIDED].scaleWithView = false;
    d->mapObjectInfo[MOL_LINEDEF_TWOSIDED].rgba[0] = 1;
    d->mapObjectInfo[MOL_LINEDEF_TWOSIDED].rgba[1] = 1;
    d->mapObjectInfo[MOL_LINEDEF_TWOSIDED].rgba[2] = 1;
    d->mapObjectInfo[MOL_LINEDEF_TWOSIDED].rgba[3] = 1;
    d->mapObjectInfo[MOL_LINEDEF_FLOOR].glow = GLOW_NONE;
    d->mapObjectInfo[MOL_LINEDEF_FLOOR].glowStrength = 1;
    d->mapObjectInfo[MOL_LINEDEF_FLOOR].glowSize = 10;
    d->mapObjectInfo[MOL_LINEDEF_FLOOR].blendMode = BM_NORMAL;
    d->mapObjectInfo[MOL_LINEDEF_FLOOR].scaleWithView = false;
    d->mapObjectInfo[MOL_LINEDEF_FLOOR].rgba[0] = 1;
    d->mapObjectInfo[MOL_LINEDEF_FLOOR].rgba[1] = 1;
    d->mapObjectInfo[MOL_LINEDEF_FLOOR].rgba[2] = 1;
    d->mapObjectInfo[MOL_LINEDEF_FLOOR].rgba[3] = 1;
    d->mapObjectInfo[MOL_LINEDEF_CEILING].glow = GLOW_NONE;
    d->mapObjectInfo[MOL_LINEDEF_CEILING].glowStrength = 1;
    d->mapObjectInfo[MOL_LINEDEF_CEILING].glowSize = 10;
    d->mapObjectInfo[MOL_LINEDEF_CEILING].blendMode = BM_NORMAL;
    d->mapObjectInfo[MOL_LINEDEF_CEILING].scaleWithView = false;
    d->mapObjectInfo[MOL_LINEDEF_CEILING].rgba[0] = 1;
    d->mapObjectInfo[MOL_LINEDEF_CEILING].rgba[1] = 1;
    d->mapObjectInfo[MOL_LINEDEF_CEILING].rgba[2] = 1;
    d->mapObjectInfo[MOL_LINEDEF_CEILING].rgba[3] = 1;

    setObjectSvg(AMO_THING, VG_TRIANGLE);
    setObjectSvg(AMO_THINGPLAYER, VG_ARROW);

    float rgb[3];
    AM_GetMapColor(rgb, cfg.common.automapL0, GRAYS+3, customPal);
    setObjectColorAndOpacity(AMO_UNSEENLINE, rgb[0], rgb[1], rgb[2], 1);

    AM_GetMapColor(rgb, cfg.common.automapL1, WALLCOLORS, customPal);
    setObjectColorAndOpacity(AMO_SINGLESIDEDLINE, rgb[0], rgb[1], rgb[2], 1);

    AM_GetMapColor(rgb, cfg.common.automapL0, TSWALLCOLORS, customPal);
    setObjectColorAndOpacity(AMO_TWOSIDEDLINE, rgb[0], rgb[1], rgb[2], 1);

    AM_GetMapColor(rgb, cfg.common.automapL2, FDWALLCOLORS, customPal);
    setObjectColorAndOpacity(AMO_FLOORCHANGELINE, rgb[0], rgb[1], rgb[2], 1);

    AM_GetMapColor(rgb, cfg.common.automapL3, CDWALLCOLORS, customPal);
    setObjectColorAndOpacity(AMO_CEILINGCHANGELINE, rgb[0], rgb[1], rgb[2], 1);
}

void AutomapStyle::objectColor(automapcfg_objectname_t name, float *r, float *g, float *b, float *a) const
{
    if(name < 0 || name >= AMO_NUMOBJECTS)
        throw Error("AutomapStyle::objectColor", "Unknown object #" + String::number((int) name));

    // It must be an object with an info.
    automapcfg_lineinfo_t const *info = nullptr;
    switch(name)
    {
    case AMO_UNSEENLINE:        info = &d->mapObjectInfo[MOL_LINEDEF_UNSEEN  ]; break;
    case AMO_SINGLESIDEDLINE:   info = &d->mapObjectInfo[MOL_LINEDEF         ]; break;
    case AMO_TWOSIDEDLINE:      info = &d->mapObjectInfo[MOL_LINEDEF_TWOSIDED]; break;
    case AMO_FLOORCHANGELINE:   info = &d->mapObjectInfo[MOL_LINEDEF_FLOOR   ]; break;
    case AMO_CEILINGCHANGELINE: info = &d->mapObjectInfo[MOL_LINEDEF_CEILING ]; break;

    default: DENG2_ASSERT(!"Object has no color property");
    }

    if(r) *r = info->rgba[0];
    if(g) *g = info->rgba[1];
    if(b) *b = info->rgba[2];
    if(a) *a = info->rgba[3];
}

void AutomapStyle::setObjectColor(automapcfg_objectname_t name, float r, float g, float b)
{
    if(name == AMO_NONE) return;  // Ignore.

    if(name < 0 || name >= AMO_NUMOBJECTS)
        throw Error("AutomapStyle::setObjectColor", "Unknown object #" + String::number((int) name));

    // It must be an object with a name.
    automapcfg_lineinfo_t *info = nullptr;
    switch(name)
    {
    case AMO_UNSEENLINE:        info = &d->mapObjectInfo[MOL_LINEDEF_UNSEEN  ]; break;
    case AMO_SINGLESIDEDLINE:   info = &d->mapObjectInfo[MOL_LINEDEF         ]; break;
    case AMO_TWOSIDEDLINE:      info = &d->mapObjectInfo[MOL_LINEDEF_TWOSIDED]; break;
    case AMO_FLOORCHANGELINE:   info = &d->mapObjectInfo[MOL_LINEDEF_FLOOR   ]; break;
    case AMO_CEILINGCHANGELINE: info = &d->mapObjectInfo[MOL_LINEDEF_CEILING ]; break;

    default: DENG2_ASSERT(!"Object has no color property");
    }

    info->rgba[0] = de::clamp(0.f, r, 1.f);
    info->rgba[1] = de::clamp(0.f, g, 1.f);
    info->rgba[2] = de::clamp(0.f, b, 1.f);
}

void AutomapStyle::setObjectColorAndOpacity(automapcfg_objectname_t name, float r, float g, float b, float a)
{
    if(name < 0 || name >= AMO_NUMOBJECTS)
        throw Error("AutomapStyle::setObjectColorAndOpacity", "Unknown object #" + String::number((int) name));

    // It must be an object with an info.
    automapcfg_lineinfo_t *info = nullptr;
    switch(name)
    {
    case AMO_UNSEENLINE:        info = &d->mapObjectInfo[MOL_LINEDEF_UNSEEN  ]; break;
    case AMO_SINGLESIDEDLINE:   info = &d->mapObjectInfo[MOL_LINEDEF         ]; break;
    case AMO_TWOSIDEDLINE:      info = &d->mapObjectInfo[MOL_LINEDEF_TWOSIDED]; break;
    case AMO_FLOORCHANGELINE:   info = &d->mapObjectInfo[MOL_LINEDEF_FLOOR   ]; break;
    case AMO_CEILINGCHANGELINE: info = &d->mapObjectInfo[MOL_LINEDEF_CEILING ]; break;

    default: DENG2_ASSERT(!"Object has no color property");
    }

    info->rgba[0] = de::clamp(0.f, r, 1.f);
    info->rgba[1] = de::clamp(0.f, g, 1.f);
    info->rgba[2] = de::clamp(0.f, b, 1.f);
    info->rgba[3] = de::clamp(0.f, a, 1.f);
}

void AutomapStyle::setObjectGlow(automapcfg_objectname_t name, glowtype_t type, float size,
    float alpha, dd_bool canScale)
{
    if(name < 0 || name >= AMO_NUMOBJECTS)
        throw Error("AutomapStyle::setObjectGlow", "Unknown object #" + String::number((int) name));

    automapcfg_lineinfo_t *info = nullptr;
    switch(name)
    {
    case AMO_UNSEENLINE:        info = &d->mapObjectInfo[MOL_LINEDEF_UNSEEN  ]; break;
    case AMO_SINGLESIDEDLINE:   info = &d->mapObjectInfo[MOL_LINEDEF         ]; break;
    case AMO_TWOSIDEDLINE:      info = &d->mapObjectInfo[MOL_LINEDEF_TWOSIDED]; break;
    case AMO_FLOORCHANGELINE:   info = &d->mapObjectInfo[MOL_LINEDEF_FLOOR   ]; break;
    case AMO_CEILINGCHANGELINE: info = &d->mapObjectInfo[MOL_LINEDEF_CEILING ]; break;

    default: DENG2_ASSERT(!"Object has no glow property");
    }

    info->glow          = type;
    info->glowStrength  = de::clamp(0.f, alpha, 1.f);
    info->glowSize      = de::clamp(0.f, size, 100.f);
    info->scaleWithView = canScale;
}

svgid_t AutomapStyle::objectSvg(automapcfg_objectname_t name) const
{
    if(name < 0 || name >= AMO_NUMOBJECTS)
        throw Error("AutomapStyle::objectSvg", "Unknown object #" + String::number((int) name));

    switch(name)
    {
    case AMO_THING:       return d->thingSvg;
    case AMO_THINGPLAYER: return d->playerSvg;

    default: DENG2_ASSERT(!"Object has no SVG property");
    }

    return 0;  // None.
}

void AutomapStyle::setObjectSvg(automapcfg_objectname_t name, svgid_t svg)
{
    if(name < 0 || name >= AMO_NUMOBJECTS)
        throw Error("AutomapStyle::setObjectSvg", "Unknown object #" + String::number((int) name));

    switch(name)
    {
    case AMO_THING:         d->thingSvg  = svg; break;
    case AMO_THINGPLAYER:   d->playerSvg = svg; break;

    default: DENG2_ASSERT(!"Object has no SVG property");
    }
}

static AutomapStyle style;

AutomapStyle *ST_AutomapStyle()
{
    return &style;
}

void ST_InitAutomapStyle()
{
    LOG_XVERBOSE("Initializing automap...");
    style.applyDefaults();
}

void AM_GetMapColor(float *rgb, float const *uColor, int palidx, dd_bool customPal)
{
    if((!customPal && !cfg.common.automapCustomColors) ||
       (customPal && cfg.common.automapCustomColors != 2))
    {
        R_GetColorPaletteRGBf(0, palidx, rgb, false);
        return;
    }

    rgb[0] = uColor[0];
    rgb[1] = uColor[1];
    rgb[2] = uColor[2];
}
