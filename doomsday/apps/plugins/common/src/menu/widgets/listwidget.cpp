/** @file listwidget.cpp  UI widget for a selectable list of items.
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

#include <QtAlgorithms>
#include "common.h"
#include "menu/widgets/listwidget.h"

#include "hu_menu.h" // menu sounds
#include "menu/page.h"

using namespace de;

namespace common {
namespace menu {

ListWidget::Item::Item(String const &text, int userValue)
{
    setText(text);
    setUserValue(userValue);
}

void ListWidget::Item::setText(String const &newText)
{
    _text = newText;
}

String ListWidget::Item::text() const
{
    return _text;
}

void ListWidget::Item::setUserValue(int newUserValue)
{
    _userValue = newUserValue;
}

int ListWidget::Item::userValue() const
{
    return _userValue;
}

DENG2_PIMPL_NOREF(ListWidget)
{
    Items items;
    int selection = 0;  ///< Selected item (-1 if none).
    int first     = 0;  ///< First visible item.
    int numvis    = 0;

    ~Instance() { qDeleteAll(items); }
};

ListWidget::ListWidget()
    : Widget()
    , d(new Instance)
{
    setFont(MENU_FONT1);
    setColor(MENU_COLOR1);
}

ListWidget::~ListWidget()
{}

ListWidget &ListWidget::addItem(Item *item)
{
    if(item) d->items << item;
    return *this;
}

ListWidget &ListWidget::addItems(Items const &itemsToAdd)
{
    for(Item *item : itemsToAdd) addItem(item);
    return *this;
}

ListWidget::Items const &ListWidget::items() const
{
    return d->items;
}

void ListWidget::updateGeometry()
{
    geometry().setSize(Vector2ui(0, 0));
    FR_PushAttrib();
    FR_SetFont(page().predefinedFont(mn_page_fontid_t(font())));

    RectRaw itemGeometry;
    for(int i = 0; i < itemCount(); ++i)
    {
        Item *item = d->items[i];

        FR_TextSize(&itemGeometry.size, item->text().toUtf8().constData());
        if(i != itemCount() - 1)
        {
            itemGeometry.size.height *= 1 + MNDATA_LIST_LEADING;
        }

        geometry() |= Rectanglei::fromSize(Vector2i(itemGeometry.origin.xy), Vector2ui(itemGeometry.size.width, itemGeometry.size.height));

        itemGeometry.origin.y += itemGeometry.size.height;
    }
    FR_PopAttrib();
}

void ListWidget::draw() const
{
    bool const flashSelection = (isActive() && selectionIsVisible());
    Vector4f const &textColor = mnRendState->textColors[color()];
    float t = flashSelection? 1 : 0;

    if(flashSelection && cfg.common.menuTextFlashSpeed > 0)
    {
        float const speed = cfg.common.menuTextFlashSpeed / 2.f;
        t = (1 + sin(page().timer() / (float)TICSPERSEC * speed * DD_PI)) / 2;
    }

    Vector4f const flashColor = de::lerp(textColor, Vector4f(Vector3f(cfg.common.menuTextFlashColor), textColor.w), t);
    Vector4f const dimColor   = Vector4f(Vector3f(textColor) * MNDATA_LIST_NONSELECTION_LIGHT, textColor.w);

    if(d->first < d->items.count() && d->numvis > 0)
    {
        DGL_Enable(DGL_TEXTURE_2D);
        FR_SetFont(mnRendState->textFonts[font()]);

        Vector2i origin = geometry().topLeft;
        int itemIdx = d->first;
        do
        {
            Item const *item      = d->items[itemIdx];
            Vector4f const &color = d->selection == itemIdx? (flashSelection? flashColor : textColor) : dimColor;

            FR_SetColorAndAlpha(color.x, color.y, color.z, color.w);
            FR_DrawTextXY3(item->text().toUtf8().constData(), origin.x, origin.y, ALIGN_TOPLEFT, Hu_MenuMergeEffectWithDrawTextFlags(0));
            origin.y += FR_TextHeight(item->text().toUtf8().constData()) * (1 + MNDATA_LIST_LEADING);
        } while(++itemIdx < d->items.count() && itemIdx < d->first + d->numvis);

        DGL_Disable(DGL_TEXTURE_2D);
    }
}

int ListWidget::handleCommand(menucommand_e cmd)
{
    switch(cmd)
    {
    case MCMD_NAV_DOWN:
    case MCMD_NAV_UP:
        if(isActive())
        {
            int oldSelection = d->selection;
            if(MCMD_NAV_DOWN == cmd)
            {
                if(d->selection < itemCount() - 1)
                    selectItem(d->selection + 1);
            }
            else
            {
                if(d->selection > 0)
                    selectItem(d->selection - 1);
            }

            if(d->selection != oldSelection)
            {
                S_LocalSound(cmd == MCMD_NAV_DOWN? SFX_MENU_NAV_DOWN : SFX_MENU_NAV_UP, NULL);
                execAction(Modified);
            }
            return true;
        }
        return false; // Not eaten.

    case MCMD_NAV_OUT:
        if(isActive())
        {
            S_LocalSound(SFX_MENU_CANCEL, NULL);
            setFlags(Active, UnsetFlags);
            execAction(Closed);
            return true;
        }
        return false; // Not eaten.

    case MCMD_SELECT:
        if(!isActive())
        {
            S_LocalSound(SFX_MENU_ACCEPT, NULL);
            setFlags(Active);
            execAction(Activated);
        }
        else
        {
            S_LocalSound(SFX_MENU_ACCEPT, NULL);
            setFlags(Active, UnsetFlags);
            execAction(Deactivated);
        }
        return true;

    default: return false; // Not eaten.
    }
}

int ListWidget::selection() const
{
    return d->selection;
}

int ListWidget::first() const
{
    return d->first;
}

bool ListWidget::selectionIsVisible() const
{
    return (d->selection >= d->first && d->selection < d->first + d->numvis);
}

void ListWidget::updateVisibleSelection()
{
    d->numvis = itemCount();
    if(d->selection >= 0)
    {
        if(d->selection < d->first)
            d->first = d->selection;
        if(d->selection > d->first + d->numvis - 1)
            d->first = d->selection - d->numvis + 1;
    }
}

int ListWidget::itemData(int index) const
{
    if(index >= 0 && index < itemCount())
    {
        return d->items[index]->userValue();
    }
    return 0;
}

int ListWidget::findItem(int userValue) const
{
    for(int i = 0; i < d->items.count(); ++i)
    {
        Item *item = d->items[i];
        if(item->userValue() == userValue)
        {
            return i;
        }
    }
    return -1;
}

bool ListWidget::selectItem(int itemIndex, int flags)
{
    if(itemIndex >= 0 && itemIndex < itemCount())
    {
        if(d->selection != itemIndex)
        {
            d->selection = itemIndex;
            if(!(flags & MNLIST_SIF_NO_ACTION))
            {
                execAction(Modified);
            }
            return true;
        }
    }
    return false;
}

bool ListWidget::selectItemByValue(int userValue, int flags)
{
    return selectItem(findItem(userValue), flags);
}

} // namespace menu
} // namespace common
