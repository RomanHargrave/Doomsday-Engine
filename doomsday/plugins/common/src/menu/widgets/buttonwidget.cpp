/** @file buttonwidget.cpp  Button widget.
 *
 * @authors Copyright © 2005-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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
#include "menu/widgets/buttonwidget.h"

#include "hu_menu.h" // menu sounds
#include "menu/page.h" // mnRendState

using namespace de;

namespace common {
namespace menu {

DENG2_PIMPL_NOREF(ButtonWidget)
{
    String text;              ///< Label text.
    patchid_t patch = -1;     ///< Used when drawing this instead of text, if set.
    bool noAltText  = false;
};

ButtonWidget::ButtonWidget(String const &text, patchid_t patch)
    : Widget()
    , d(new Instance)
{
    setFont(MENU_FONT2);
    setColor(MENU_COLOR1);

    setText(text);
    setPatch(patch);
}

ButtonWidget::~ButtonWidget()
{}

void ButtonWidget::draw(Point2Raw const *origin)
{
    DENG2_ASSERT(origin != 0);
    fontid_t const fontId = mnRendState->textFonts[font()];
    float textColor[4], t = (isFocused()? 1 : 0);

    // Flash if focused.
    if(isFocused() && cfg.menuTextFlashSpeed > 0)
    {
        float const speed = cfg.menuTextFlashSpeed / 2.f;
        t = (1 + sin(page().timer() / (float)TICSPERSEC * speed * DD_PI)) / 2;
    }

    lerpColor(textColor, mnRendState->textColors[color()], cfg.menuTextFlashColor, t, false/*rgb mode*/);
    textColor[CA] = mnRendState->textColors[color()][CA];

    FR_SetFont(fontId);
    FR_SetColorAndAlphav(textColor);
    DGL_Color4f(1, 1, 1, textColor[CA]);

    if(d->patch >= 0)
    {
        String replacement;
        if(!d->noAltText)
        {
            replacement = Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.menuPatchReplaceMode), d->patch, d->text);
        }

        DGL_Enable(DGL_TEXTURE_2D);
        WI_DrawPatch(d->patch, replacement, Vector2i(origin->x, origin->y),
                     ALIGN_TOPLEFT, 0, Hu_MenuMergeEffectWithDrawTextFlags(0));
        DGL_Disable(DGL_TEXTURE_2D);

        return;
    }

    DGL_Enable(DGL_TEXTURE_2D);
    FR_DrawText3(d->text.toUtf8().constData(), origin, ALIGN_TOPLEFT, Hu_MenuMergeEffectWithDrawTextFlags(0));
    DGL_Disable(DGL_TEXTURE_2D);
}

int ButtonWidget::handleCommand(menucommand_e cmd)
{
    if(cmd == MCMD_SELECT)
    {
        if(!isActive())
        {
            setFlags(Active);
            if(hasAction(MNA_ACTIVE))
            {
                execAction(MNA_ACTIVE);
            }
        }

        // We are not going to receive an "up event" so action that now.
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        setFlags(Active, UnsetFlags);
        if(hasAction(MNA_ACTIVEOUT))
        {
            execAction(MNA_ACTIVEOUT);
        }

        return true;
    }

    return false; // Not eaten.
}

void ButtonWidget::updateGeometry(Page *page)
{
    DENG2_ASSERT(page != 0);
    String useText = d->text;
    Size2Raw size;

    // @todo What if patch replacement is disabled?
    if(d->patch >= 0)
    {
        if(!d->noAltText)
        {
            // Use the replacement string?
            useText = Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.menuPatchReplaceMode), d->patch, d->text);
        }

        if(useText.isEmpty())
        {
            // Use the original patch.
            patchinfo_t info;
            R_GetPatchInfo(d->patch, &info);
            Rect_SetWidthHeight(geometry(), info.geometry.size.width,
                                            info.geometry.size.height);
            return;
        }
    }

    FR_SetFont(page->predefinedFont(mn_page_fontid_t(font())));
    FR_TextSize(&size, useText.toUtf8().constData());

    Rect_SetWidthHeight(geometry(), size.width, size.height);
}

String ButtonWidget::text() const
{
    return d->text;
}

ButtonWidget &ButtonWidget::setText(String const &newText)
{
    d->text = newText;
    return *this;
}

patchid_t ButtonWidget::patch() const
{
    return d->patch;
}

ButtonWidget &ButtonWidget::setPatch(patchid_t newPatch)
{
    d->patch = newPatch;
    return *this;
}

bool ButtonWidget::noAltText() const
{
    return d->noAltText;
}

ButtonWidget &ButtonWidget::setNoAltText(bool yes)
{
    d->noAltText = yes;
    return *this;
}

} // namespace menu
} // namespace common