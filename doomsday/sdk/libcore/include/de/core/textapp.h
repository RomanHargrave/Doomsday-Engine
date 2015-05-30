/** @file textapp.h  Application with text-based/console interface.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_TEXTAPP_H
#define LIBDENG2_TEXTAPP_H

#include <QCoreApplication>
#include <de/App>
#include <de/Loop>

/**
 * Macro for conveniently accessing the de::TextApp singleton instance.
 */
#define DENG2_TEXT_APP   (static_cast<de::TextApp *>(qApp))

namespace de {

/**
 * Application with a text-based/console UI.
 *
 * The event loop is protected against uncaught exceptions. Catches the
 * exception and shuts down the app cleanly.
 *
 * @ingroup core
 */
class DENG2_PUBLIC TextApp : public QCoreApplication, public App,
                             DENG2_OBSERVES(Loop, Iteration)
{
    Q_OBJECT

public:
    TextApp(int &argc, char **argv);

    void setMetadata(String const &orgName, String const &orgDomain,
                     String const &appName, String const &appVersion);

    bool notify(QObject *receiver, QEvent *event);

    int execLoop();
    void stopLoop(int code);

    Loop &loop();

protected:
    NativePath appDataPath() const;
    void loopIteration();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_TEXTAPP_H
