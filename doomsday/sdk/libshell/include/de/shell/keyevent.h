/** @file shell/keyevent.h Key event.
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

#ifndef LIBSHELL_KEYEVENT_H
#define LIBSHELL_KEYEVENT_H

#include "libshell.h"
#include <de/Event>
#include <de/String>
#include <QFlags>

namespace de {
namespace shell {

/**
 * Key press event generated when the user presses a key on the keyboard.
 */
class LIBSHELL_PUBLIC KeyEvent : public Event
{
public:
    enum Modifier
    {
        None = 0x0,
        Control = 0x1
    };
    Q_DECLARE_FLAGS(Modifiers, Modifier)

public:
    KeyEvent(String const &keyText) : Event(KeyPress), _text(keyText), _code(0) {}
    KeyEvent(int keyCode, Modifiers mods = None) : Event(KeyPress), _code(keyCode), _modifiers(mods) {}

    String text() const { return _text; }
    int key() const { return _code; }
    Modifiers modifiers() const { return _modifiers; }

    bool operator == (KeyEvent const &other) const {
        return _text == other._text &&
               _code == other._code &&
               _modifiers == other._modifiers;
    }

private:
    String _text;           ///< Text to be inserted by the event.
    int _code;              ///< Qt key code.
    Modifiers _modifiers;   ///< Modifiers in effect.
};

Q_DECLARE_OPERATORS_FOR_FLAGS(KeyEvent::Modifiers)

} // namespace shell
} // namespace de

#endif // LIBSHELL_KEYEVENT_H
