/** @file submenuitem.h
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_UI_SUBMENUITEM_H
#define DENG_CLIENT_UI_SUBMENUITEM_H

#include "../uidefs.h"
#include "item.h"
#include "listcontext.h"

namespace ui {

/**
 * UI context item that contains items for a submenu.
 */
class SubmenuItem : public Item
{
public:
    SubmenuItem(de::String const &label, ui::Direction openingDirection)
        : Item(ShownAsButton, label), _dir(openingDirection) {}

    Context &items() { return _items; }
    Context const &items() const { return _items; }
    ui::Direction openingDirection() const { return _dir; }

private:
    ListContext _items;
    ui::Direction _dir;
};

} // namespace ui

#endif // DENG_CLIENT_UI_SUBMENUITEM_H