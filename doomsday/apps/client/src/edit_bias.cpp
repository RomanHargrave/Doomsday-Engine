/** @file edit_bias.cpp  Shadow Bias editor UI.
 *
 * @authors Copyright © 2006-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifdef __CLIENT__
#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"
#include "de_ui.h"

#include "world/map.h"
#include "world/p_players.h" // viewPlayer
#include "ConvexSubspace"
#include "Hand"
#include "HueCircle"
#include "render/viewports.h"
#include "render/rend_main.h" // gameDrawHUD/vOrigin, remove me
#include "edit_bias.h"

#include <de/Log>
#include <de/concurrency.h>

using namespace de;

D_CMD(BLEditor);

/*
 * Editor variables:
 */
int editHidden;
int editBlink;
int editShowAll;
int editShowIndices = true;

/*
 * Editor status:
 */
static bool editActive; // Edit mode active?
static bool editHueCircle;
static HueCircle *hueCircle;

void SBE_Register()
{
    // Variables.
    C_VAR_INT("edit-bias-blink",         &editBlink,          0, 0, 1);
    C_VAR_INT("edit-bias-hide",          &editHidden,         0, 0, 1);
    C_VAR_INT("edit-bias-show-sources",  &editShowAll,        0, 0, 1);
    C_VAR_INT("edit-bias-show-indices",  &editShowIndices,    0, 0, 1);

    // Commands.
    C_CMD_FLAGS("bledit",   "",     BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blquit",   "",     BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blclear",  "",     BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blsave",   NULL,   BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blnew",    "",     BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("bldel",    "",     BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("bllock",   NULL,   BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blunlock", NULL,   BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blgrab",   NULL,   BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blungrab", NULL,   BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("bldup",    "",     BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blc",      "fff",  BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("bli",      NULL,   BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("bllevels", NULL,   BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("blhue",    NULL,   BLEditor, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
}

bool SBE_Active()
{
    return editActive;
}

HueCircle *SBE_HueCircle()
{
    if(!editActive || !editHueCircle) return 0;
    return hueCircle;
}

void SBE_SetHueCircle(bool activate = true)
{
    if(!editActive) return;

    // Any change in state?
    if(activate == editHueCircle) return;

    // The circle can only be activated when something is grabbed.
    if(activate && App_WorldSystem().hand().isEmpty()) return;

    editHueCircle = activate;

    if(activate)
    {
        viewdata_t const &viewer = *R_ViewData(viewPlayer - ddPlayers);
        hueCircle->setOrientation(viewer.frontVec, viewer.sideVec, viewer.upVec);
    }
}

/*
 * Editor Functionality:
 */

static void SBE_Begin()
{
    if(editActive) return;

#ifdef __CLIENT__
    // Advise the game not to draw any HUD displays
    gameDrawHUD = false;
#endif

    editActive = true;
    editHueCircle = false;
    hueCircle = new HueCircle;

    LOG_AS("Bias");
    LOG_VERBOSE("Editing begins");
}

static void SBE_End()
{
    if(!editActive) return;

    App_WorldSystem().hand().ungrab();

    delete hueCircle; hueCircle = 0;
    editHueCircle = false;
    editActive = false;

#ifdef __CLIENT__
    // Advise the game it can safely draw any HUD displays again
    gameDrawHUD = true;
#endif

    LOG_AS("Bias");
    LOG_VERBOSE("Editing ends.");
}

static void SBE_Clear()
{
    DENG_ASSERT(editActive);
    App_WorldSystem().map().removeAllBiasSources();
}

static void SBE_Delete(int which)
{
    DENG_ASSERT(editActive);
    App_WorldSystem().map().removeBiasSource(which);
}

static BiasSource *SBE_New()
{
    DENG_ASSERT(editActive);
    try
    {
        Hand &hand = App_WorldSystem().hand();
        BiasSource &source = App_WorldSystem().map().addBiasSource(hand.origin());

        // Update the edit properties.
        hand.setEditIntensity(source.intensity());
        hand.setEditColor(source.color());

        hand.grab(source);

        // As this is a new source -- unlock immediately.
        source.unlock();

        return &source;
    }
    catch(Map::FullError const &)
    {} // Ignore this error.

    return 0;
}

static BiasSource *SBE_Dupe(BiasSource const &other)
{
    DENG_ASSERT(editActive);
    try
    {
        Hand &hand = App_WorldSystem().hand();
        BiasSource &source = App_WorldSystem().map().addBiasSource(other); // A copy is made.

        source.setOrigin(hand.origin());

        // Update the edit properties.
        hand.setEditIntensity(source.intensity());
        hand.setEditColor(source.color());

        hand.grab(source);

        // As this is a new source -- unlock immediately.
        source.unlock();

        return &source;
    }
    catch(Map::FullError const &)
    {} // Ignore this error.

    return 0;
}

static void SBE_Grab(int which)
{
    DENG_ASSERT(editActive);
    Hand &hand = App_WorldSystem().hand();
    if(BiasSource *source = App_WorldSystem().map().biasSourcePtr(which))
    {
        if(hand.isEmpty())
        {
            // Update the edit properties.
            hand.setEditIntensity(source->intensity());
            hand.setEditColor(source->color());
        }

        hand.grabMulti(*source);
    }
}

static void SBE_Ungrab(int which)
{
    DENG_ASSERT(editActive);
    Hand &hand = App_WorldSystem().hand();
    if(BiasSource *source = App_WorldSystem().map().biasSourcePtr(which))
    {
        hand.ungrab(*source);
    }
    else
    {
        hand.ungrab();
    }
}

static void SBE_SetLock(int which, bool enable = true)
{
    DENG_ASSERT(editActive);
    Hand &hand = App_WorldSystem().hand();
    if(BiasSource *source = App_WorldSystem().map().biasSourcePtr(which))
    {
        if(enable) source->lock();
        else       source->unlock();
        return;
    }

    for(Grabbable *grabbable : hand.grabbed())
    {
        if(enable) grabbable->lock();
        else       grabbable->unlock();
    }
}

static bool SBE_Save(char const *name = 0)
{
    DENG2_ASSERT(editActive);

    LOG_AS("Bias");

    Map &map = App_WorldSystem().map();

    ddstring_t fileName; Str_Init(&fileName);
    if(!name || !name[0])
    {
        Str_Set(&fileName, String(map.def()? map.def()->composeUri().path() : "unknownmap").toUtf8().constData());
    }
    else
    {
        Str_Set(&fileName, name);
        F_ExpandBasePath(&fileName, &fileName);
    }

    // Do we need to append an extension?
    if(String(Str_Text(&fileName)).fileNameExtension().isEmpty())
    {
        Str_Append(&fileName, ".ded");
    }

    F_ToNativeSlashes(&fileName, &fileName);
    FILE *file = fopen(Str_Text(&fileName), "wt");
    if(!file)
    {
        LOG_RES_WARNING("Failed to save light sources to \"%s\": could not open file")
                << NativePath(Str_Text(&fileName)).pretty();
        Str_Free(&fileName);
        return false;
    }

    LOG_RES_VERBOSE("Saving to \"%s\"...")
            << NativePath(Str_Text(&fileName)).pretty();

    String uid = (map.def()? map.def()->composeUniqueId(App_CurrentGame()) : "(unknown map)");
    fprintf(file, "# %i Bias Lights for %s", map.biasSourceCount(), uid.toUtf8().constData());

    // Since there can be quite a lot of these, make sure we'll skip
    // the ones that are definitely not suitable.
    fprintf(file, "\n\nSkipIf Not %s", App_CurrentGame().identityKey().toUtf8().constData());

    map.forAllBiasSources([&file, &uid] (BiasSource &bsrc)
    {
        float minLight, maxLight;
        bsrc.lightLevels(minLight, maxLight);

        fprintf(file, "\n\nLight {");
        fprintf(file, "\n  Map = \"%s\"", uid.toUtf8().constData());
        fprintf(file, "\n  Origin { %g %g %g }", bsrc.origin().x, bsrc.origin().y, bsrc.origin().z);
        fprintf(file, "\n  Color { %g %g %g }", bsrc.color().x, bsrc.color().y, bsrc.color().z);
        fprintf(file, "\n  Intensity = %g", bsrc.intensity());
        fprintf(file, "\n  Sector levels { %g %g }", minLight, maxLight);
        fprintf(file, "\n}");

        return LoopContinue;
    });

    fclose(file);
    Str_Free(&fileName);
    return true;
}

/*
 * Editor commands.
 */
D_CMD(BLEditor)
{
    DENG_UNUSED(src);

    char *cmd = argv[0] + 2;

    if(!qstricmp(cmd, "edit"))
    {
        SBE_Begin();
        return true;
    }

    if(!editActive)
    {
        LOG_WARNING("The bias lighting editor is not active");
        return false;
    }

    if(!qstricmp(cmd, "quit"))
    {
        SBE_End();
        return true;
    }

    if(!qstricmp(cmd, "save"))
    {
        return SBE_Save(argc >= 2 ? argv[1] : 0);
    }

    if(!qstricmp(cmd, "clear"))
    {
        SBE_Clear();
        return true;
    }

    if(!qstricmp(cmd, "hue"))
    {
        int activate = (argc >= 2 ? stricmp(argv[1], "off") : !editHueCircle);
        SBE_SetHueCircle(activate);
        return true;
    }

    Map &map = App_WorldSystem().map();
    coord_t handDistance;
    Hand &hand = App_WorldSystem().hand(&handDistance);

    if(!qstricmp(cmd, "new"))
    {
        return SBE_New() != 0;
    }

    if(!qstricmp(cmd, "c"))
    {
        // Update the edit properties.
        hand.setEditColor(Vector3f(argc > 1? strtod(argv[1], 0) : 1,
                                   argc > 2? strtod(argv[2], 0) : 1,
                                   argc > 3? strtod(argv[3], 0) : 1));
        return true;
    }

    if(!qstricmp(cmd, "i"))
    {
        hand.setEditIntensity(argc > 1? strtod(argv[1], 0) : 200);
        return true;
    }

    if(!qstricmp(cmd, "grab"))
    {
        SBE_Grab(map.indexOf(*map.biasSourceNear(hand.origin())));
        return true;
    }

    if(!qstricmp(cmd, "ungrab"))
    {
        SBE_Ungrab(argc > 1? atoi(argv[1]) : -1);
        return true;
    }

    if(!qstricmp(cmd, "lock"))
    {
        SBE_SetLock(argc > 1? atoi(argv[1]) : -1);
        return true;
    }

    if(!qstricmp(cmd, "unlock"))
    {
        SBE_SetLock(argc > 1? atoi(argv[1]) : -1, false);
        return true;
    }

    // Has a source index been given as an argument?
    int which = -1;
    if(!hand.isEmpty())
    {
        which = map.indexOf(hand.grabbed().first()->as<BiasSource>());
    }
    else
    {
        which = map.indexOf(*map.biasSourceNear(hand.origin()));
    }

    if(argc > 1)
    {
        which = atoi(argv[1]);
    }

    if(which < 0 || which >= map.biasSourceCount())
    {
        LOG_SCR_WARNING("Invalid bias light source index %i") << which;
        return false;
    }

    if(!qstricmp(cmd, "del"))
    {
        SBE_Delete(which);
        return true;
    }

    if(!qstricmp(cmd, "dup"))
    {
        return SBE_Dupe(map.biasSource(which)) != nullptr;
    }

    if(!qstricmp(cmd, "levels"))
    {
        float minLight = 0, maxLight = 0;
        if(argc >= 2)
        {
            minLight = strtod(argv[1], 0) / 255.0f;
            maxLight = argc >= 3? strtod(argv[2], 0) / 255.0f : minLight;
        }
        map.biasSource(which).setLightLevels(minLight, maxLight);
        return true;
    }

    return false;
}

/*
 * Editor visuals (would-be widgets):
 */

#include "api_fontrender.h"
#include "world/map.h"
#include "world/p_players.h"
#include "BspLeaf"
#include "SectorCluster"

#include "render/rend_font.h"

static void drawBoxBackground(Vector2i const &origin_, Vector2i const &size_, ui_color_t *color)
{
    Point2Raw origin(origin_.x, origin_.y);
    Size2Raw size(size_.x, size_.y);
    UI_GradientEx(&origin, &size, 6,
                  color ? color : UI_Color(UIC_BG_MEDIUM),
                  color ? color : UI_Color(UIC_BG_LIGHT),
                  .2f, .4f);
    UI_DrawRectEx(&origin, &size, 6, false, color ? color : UI_Color(UIC_BRD_HI),
                  NULL, .4f, -1);
}

static void drawText(String const &text, Vector2i const &origin_, ui_color_t *color, float alpha,
                     int align = ALIGN_LEFT, short flags = DTF_ONLY_SHADOW)
{
    Point2Raw origin(origin_.x, origin_.y);
    UI_TextOutEx2(text.toUtf8().constData(), &origin, color, alpha, align, flags);
}

/**
 * - index #, lock status
 * - origin
 * - distance from eye
 * - intensity, light level threshold
 * - color
 */
static void drawInfoBox(BiasSource *s, int rightX, String const title, float alpha)
{
    int const precision = 3;

    if(!s) return;

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    int th = FR_SingleLineHeight("Info");
    Vector2i size(16 + FR_TextWidth("Color:(0.000, 0.000, 0.000)"), 16 + th * 6);

    Vector2i origin(DENG_GAMEVIEW_WIDTH  - 10 - size.x - rightX,
                    DENG_GAMEVIEW_HEIGHT - 10 - size.y);

    ui_color_t color;
    color.red   = s->color().x;
    color.green = s->color().y;
    color.blue  = s->color().z;

    DENG_ASSERT_IN_MAIN_THREAD();

    glEnable(GL_TEXTURE_2D);

    drawBoxBackground(origin, size, &color);
    origin.x += 8;
    origin.y += 8 + th/2;

    drawText(title, origin, UI_Color(UIC_TITLE), alpha);
    origin.y += th;

    int sourceIndex = App_WorldSystem().map().indexOf(*s);
    coord_t distance = (s->origin() - vOrigin.xzy()).length();
    float minLight, maxLight;
    s->lightLevels(minLight, maxLight);

    String text1 = String("#%1").arg(sourceIndex, 3, 10, QLatin1Char('0')) + (s->isLocked()? " (locked)" : "");
    drawText(text1, origin, UI_Color(UIC_TEXT), alpha);
    origin.y += th;

    String text2 = String("Origin:") + s->origin().asText();
    drawText(text2, origin, UI_Color(UIC_TEXT), alpha);
    origin.y += th;

    String text3 = String("Distance:%1").arg(distance, 5, 'f', precision, QLatin1Char('0'));
    drawText(text3, origin, UI_Color(UIC_TEXT), alpha);
    origin.y += th;

    String text4 = String("Intens:%1").arg(s->intensity(), 5, 'f', precision, QLatin1Char('0'));
    if(!de::fequal(minLight, 0) || !de::fequal(maxLight, 0))
        text4 += String(" L:%2/%3").arg(int(255.0f * minLight), 3).arg(int(255.0f * maxLight), 3);
    drawText(text4, origin, UI_Color(UIC_TEXT), alpha);
    origin.y += th;

    String text5 = String("Color:(%1, %2, %3)").arg(s->color().x, 0, 'f', precision).arg(s->color().y, 0, 'f', precision).arg(s->color().z, 0, 'f', precision);
    drawText(text5, origin, UI_Color(UIC_TEXT), alpha);
    origin.y += th;

    glDisable(GL_TEXTURE_2D);
}

static void drawLightGauge(Vector2i const &origin, int height = 255)
{
    static float minLevel = 0, maxLevel = 0;
    static SectorCluster *lastCluster = 0;

    Hand &hand = App_WorldSystem().hand();
    Map &map = App_WorldSystem().map();

    BiasSource *source;
    if(!hand.isEmpty())
        source = &hand.grabbed().first()->as<BiasSource>();
    else
        source = map.biasSourceNear(hand.origin());

    if(ConvexSubspace *subspace = source->bspLeafAtOrigin().subspacePtr())
    {
        if(subspace->hasCluster() && lastCluster != subspace->clusterPtr())
        {
            lastCluster = &subspace->cluster();
            minLevel = maxLevel = lastCluster->lightSourceIntensity();
        }
    }

    float const lightLevel = lastCluster? lastCluster->lightSourceIntensity() : 0;
    if(lightLevel < minLevel)
        minLevel = lightLevel;
    if(lightLevel > maxLevel)
        maxLevel = lightLevel;

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);
    int off = FR_TextWidth("000");

    int minY = 0, maxY = 0;

    glBegin(GL_LINES);
    glColor4f(1, 1, 1, .5f);
    glVertex2f(origin.x + off, origin.y);
    glVertex2f(origin.x + off, origin.y + height);
    // Normal light level.
    int secY = origin.y + height * (1.0f - lightLevel);
    glVertex2f(origin.x + off - 4, secY);
    glVertex2f(origin.x + off, secY);
    if(maxLevel != minLevel)
    {
        // Max light level.
        maxY = origin.y + height * (1.0f - maxLevel);
        glVertex2f(origin.x + off + 4, maxY);
        glVertex2f(origin.x + off, maxY);

        // Min light level.
        minY = origin.y + height * (1.0f - minLevel);
        glVertex2f(origin.x + off + 4, minY);
        glVertex2f(origin.x + off, minY);
    }

    // Current min/max bias sector level.
    float minLight, maxLight;
    source->lightLevels(minLight, maxLight);
    if(minLight > 0 || maxLight > 0)
    {
        glColor3f(1, 0, 0);
        int p = origin.y + height * (1.0f - minLight);
        glVertex2f(origin.x + off + 2, p);
        glVertex2f(origin.x + off - 2, p);

        glColor3f(0, 1, 0);
        p = origin.y + height * (1.0f - maxLight);
        glVertex2f(origin.x + off + 2, p);
        glVertex2f(origin.x + off - 2, p);
    }
    glEnd();

    glEnable(GL_TEXTURE_2D);

    // The number values.
    drawText(String::number(int(255.0f * lightLevel)),
             Vector2i(origin.x, secY), UI_Color(UIC_TITLE), .7f, 0, DTF_ONLY_SHADOW);

    if(maxLevel != minLevel)
    {
        drawText(String::number(int(255.0f * maxLevel)),
                 Vector2i(origin.x + 2*off, maxY), UI_Color(UIC_TEXT), .7f, 0, DTF_ONLY_SHADOW);

        drawText(String::number(int(255.0f * minLevel)),
                 Vector2i(origin.x + 2*off, minY), UI_Color(UIC_TEXT), .7f, 0, DTF_ONLY_SHADOW);
    }

    glDisable(GL_TEXTURE_2D);
}

void SBE_DrawGui()
{
    float const opacity = .8f;

    if(!editActive || editHidden) return;

    if(!App_WorldSystem().hasMap()) return;

    Map &map = App_WorldSystem().map();
    Hand &hand = App_WorldSystem().hand();

    DENG_ASSERT_IN_MAIN_THREAD();

    // Go into screen projection mode.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT, 0, -1, 1);

    glEnable(GL_TEXTURE_2D);

    // Overall stats: numSources / MAX (left)
    String text = String("%1 / %2 (%3 free)")
                    .arg(map.biasSourceCount())
                    .arg(Map::MAX_BIAS_SOURCES)
                    .arg(Map::MAX_BIAS_SOURCES - map.biasSourceCount());

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    Vector2i size(FR_TextWidth(text.toUtf8().constData()) + 16,
                  FR_SingleLineHeight(text.toUtf8().constData()) + 16);
    int top = DENG_GAMEVIEW_HEIGHT - 10 - size.y;

    Vector2i origin(10, top);
    drawBoxBackground(origin, size, 0);
    origin.x += 8;
    origin.y += size.y / 2;

    drawText(text, origin, UI_Color(UIC_TITLE), opacity);
    origin.y = top - size.y / 2;

    // The map ID.
    String label = (map.def()? map.def()->composeUniqueId(App_CurrentGame()) : "(unknown map)");
    drawText(label, origin, UI_Color(UIC_TITLE), opacity);

    glDisable(GL_TEXTURE_2D);

    if(map.biasSourceCount())
    {
        // Stats for nearest & grabbed:
        drawInfoBox(map.biasSourceNear(hand.origin()), 0, "Nearest", opacity);

        if(!hand.isEmpty())
        {
            FR_SetFont(fontFixed);
            int x = FR_TextWidth("0") * 30;
            drawInfoBox(&hand.grabbed().first()->as<BiasSource>(), x, "Grabbed", opacity);
        }

        drawLightGauge(Vector2i(20, DENG_GAMEVIEW_HEIGHT/2 - 255/2));
    }

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

#endif // __CLIENT__
