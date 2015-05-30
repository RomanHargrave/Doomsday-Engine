/** @file multiplayermenuwidget.h
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_MULTIPLAYERMENUWIDGET_H
#define DENG_CLIENT_MULTIPLAYERMENUWIDGET_H

#include <de/PopupMenuWidget>

/**
 * Popup menu that appears in the task bar when joined to a multiplayer game.
 *
 * @ingroup ui
 */
class MultiplayerMenuWidget : public de::PopupMenuWidget
{
    Q_OBJECT
    
public:
    MultiplayerMenuWidget();
    
public slots:
    void updateElapsedTime();

protected:
    void preparePanelForOpening();
    void panelClosing();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_MULTIPLAYERMENUWIDGET_H
