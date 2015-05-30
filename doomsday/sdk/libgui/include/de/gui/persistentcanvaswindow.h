/** @file persistentcanvaswindow.h  Canvas window with persistent state.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBGUI_PERSISTENTCANVASWINDOW_H
#define LIBGUI_PERSISTENTCANVASWINDOW_H

#include <de/Error>
#include <de/CanvasWindow>
#include <de/Rectangle>

namespace de {

#undef main

/**
 * General-purpose top-level window with persistent state. Each instance must
 * be identified by a unique name (e.g., "main") that is used when saving the
 * window's state to Config.
 *
 * Supports fullscreen display modes (using DisplayMode).
 */
class LIBGUI_PUBLIC PersistentCanvasWindow : public CanvasWindow
{
    Q_OBJECT

public:
    /// Provided window ID was not valid. @ingroup errors
    DENG2_ERROR(InvalidIdError);

    /// Absolute minimum width of a window (in fullscreen also).
    static int const MIN_WIDTH;

    /// Absolute minimum height of a window (in fullscreen also).
    static int const MIN_HEIGHT;

    /**
     * Window attributes.
     */
    enum Attribute
    {
        End = 0, ///< Marks the end of an attribute list (not a valid attribute in itself)

        // Windowed attributes
        Left,
        Top,
        Width,
        Height,
        Centered,
        Maximized,

        // Fullscreen attributes
        Fullscreen,
        FullscreenWidth,
        FullscreenHeight,
        ColorDepthBits,

        // Other
        FullSceneAntialias,
        VerticalSync
    };

    /**
     * Notified after one or more window attributes have changed. If the
     * changes are queued, the notification is made only after all the changes
     * have been applied.
     */
    DENG2_DEFINE_AUDIENCE2(AttributeChange, void windowAttributesChanged(PersistentCanvasWindow &))

public:    
    /**
     * Constructs a new window using the persistent configuration associated
     * with @a id. Note that the configuration is saved persistently when the
     * window is deleted.
     *
     * Command line options (e.g., -xpos) can be used to modify the main window
     * configuration directly.
     *
     * @param id  Identifier of the window.
     */
    PersistentCanvasWindow(String const &id);

    String id() const;

    /**
     * Returns @c true iff the window is currently centered.
     */
    bool isCentered() const;

    /**
     * Returns the current placement of the window when it is in normal window
     * mode (neither fullscreen or maximized).
     */
    Rectanglei windowRect() const;

    Size fullscreenSize() const;

    inline int fullscreenWidth() const { return fullscreenSize().x; }

    inline int fullscreenHeight() const { return fullscreenSize().y; }

    int colorDepthBits() const;

    void show(bool yes = true);

    /**
     * Sets or changes one or more window attributes.
     *
     * @param attribs  Array of values:
     *      <pre>[ attribId, value, attribId, value, ..., 0 ]</pre>
     *      The array must be zero-terminated, as that indicates where the
     *      array ends (see Attribute).
     *
     * @return @c true iff all attribute deltas were validated and the window
     * was then successfully updated accordingly. If any attribute failed to
     * validate, the window will remain unchanged and @a false is returned.
     */
    bool changeAttributes(int const *attribs);

    /**
     * Saves the window's state into a persistent storage so that it can be later
     * on restored. Used at shutdown time to save window geometry.
     */
    void saveToConfig();

    /**
     * Restores the window's state from persistent storage. Used when creating
     * a window to determine its persistent configuration.
     */
    void restoreFromConfig();

    /**
     * Saves the current state in memory (not persistently). The saved state can
     * later be restored with a call to restoreState().
     */
    void saveState();

    /**
     * Restores the attribuets of the window from previously saved state.
     */
    void restoreState();

    static PersistentCanvasWindow &main();

    // Events.
    void moveEvent(QMoveEvent *);
    void resizeEvent(QResizeEvent *);

protected slots:
    void performQueuedTasks();

    /**
     * Forms the name of a Config variable for this window. Subclasses are
     * allowed to use this to store their own properties in the persistent
     * configuration.
     *
     * @param key  Variable name.
     *
     * @return Name of a variable in Config.
     */
    String configName(String const &key) const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_PERSISTENTCANVASWINDOW_H
