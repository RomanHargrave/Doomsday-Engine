/** @file popupmenuwidget.h
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

#ifndef LIBAPPFW_POPUPMENUWIDGET_H
#define LIBAPPFW_POPUPMENUWIDGET_H

#include "../PopupWidget"
#include "../MenuWidget"

namespace de {

/**
 * Popup widget that contains a menu.
 */
class LIBAPPFW_PUBLIC PopupMenuWidget : public PopupWidget
{
public:
    PopupMenuWidget(String const &name = "");

    MenuWidget &menu() const;

    ui::Data &items() { return menu().items(); }

    // Events.
    void update();

protected:
    void glMakeGeometry(DefaultVertexBuf::Builder &verts);
    void preparePanelForOpening();
    void panelClosing();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_POPUPMENUWIDGET_H
