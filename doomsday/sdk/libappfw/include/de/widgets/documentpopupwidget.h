/** @file documentpopupwidget.h
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_DOCUMENTPOPUPWIDGET_H
#define LIBAPPFW_DOCUMENTPOPUPWIDGET_H

#include "../DocumentWidget"
#include "../PopupWidget"

namespace de {

/**
 * Utility widget that has a document inside a popup.
 */
class LIBAPPFW_PUBLIC DocumentPopupWidget : public PopupWidget
{
    Q_OBJECT

public:
    DocumentPopupWidget(String const &name = "");

    DocumentWidget &document();
    DocumentWidget const &document() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_DOCUMENTPOPUPWIDGET_H
