/** @file saveslots.h  Map of logical saved game session slots.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_SAVESLOTS_H
#define LIBCOMMON_SAVESLOTS_H

#include <de/Error>
#include <de/Path>
#include <de/String>
#include <de/game/SavedSession>

/**
 * Maps saved game session file names into a finite set of "save slots".
 *
 * @ingroup libcommon
 */
class SaveSlots
{
public:
    /// A missing/unknown slot was referenced. @ingroup errors
    DENG2_ERROR(MissingSlotError);

    /**
     * Logical save slot.
     */
    class Slot
    {
    public:
        /// Logical saved session status:
        enum SessionStatus {
             Loadable,
             Incompatible,
             Unused
        };

    public:
        Slot(de::String id, bool userWritable, de::String saveName, int menuWidgetId = 0);

        /**
         * Returns the logical status of the saved session associated with the logical save slot.
         */
        SessionStatus sessionStatus() const;

        inline bool isLoadable() const     { return sessionStatus() == Loadable; }
        inline bool isIncompatible() const { return sessionStatus() == Incompatible; }
        inline bool isUnused() const       { return sessionStatus() == Unused; }

        /**
         * Returns @c true iff the logical save slot is user-writable.
         */
        bool isUserWritable() const;

        /**
         * Returns the unique identifier/name for the logical save slot.
         */
        de::String const &id() const;

        /**
         * Returns the absolute path of the saved session, bound to the logical save slot.
         */
        de::String const &savePath() const;

        /**
         * Returns the name of the saved session, bound to the logical save slot.
         * @see savePath()
         */
        inline de::String const saveName() const { return savePath().fileNameWithoutExtension(); }

        /**
         * Change the name of the saved session, bound to the logical save slot.
         *
         * @param newName  New of the saved session to bind to.
         */
        void bindSaveName(de::String newName);

        /**
         * Force a manual update of the save slot status and update any linked menu item.
         * Calling this is usually unnecessary as the status is automatically updated when
         * the associated saved session changes.
         */
        void updateStatus();

    private:
        friend class SaveSlots;
        void setSavedSession(de::game::SavedSession *newSession);

        DENG2_PRIVATE(d)
    };

public:
    /**
     * Create a new (empty) set of save slots.
     */
    SaveSlots();

    /**
     * Add a new logical save slot.
     *
     * @param id            Unique identifier for this slot.
     * @param userWritable  @c true= allow the user to write to this slot.
     * @param saveName      Name of the saved session to bind to this slot.
     * @param menuWidgetId  Unique identifier of the game menu widget to associate this slot with.
     *                      Use @c 0 for none.
     */
    void add(de::String const &id, bool userWritable, de::String const &saveName,
             int menuWidgetId = 0);

    /**
     * Returns the total number of logical save slots.
     */
    int count() const;

    /// @copydoc count()
    inline int size() const { return count(); }

    /**
     * Returns @c true iff @a slotId is interpretable as a logical slot identifier.
     */
    bool has(de::String const &slotId) const;

    /// @see slot()
    inline Slot &operator [] (de::String const &slotId) {
        return slot(slotId);
    }

    /**
     * Returns the logical save slot associated with @a slotId.
     *
     * @see has()
     */
    Slot &slot(de::String const &slotId) const;

    /**
     * Returns the logical save slot associated with the saved session @a name specified.
     */
    Slot *slotBySaveName(de::String const &name) const;

    /**
     * Returns the logical save slot associated with the @em first saved session with the
     * user @a description specified.
     */
    Slot *slotBySavedUserDescription(de::String const &description) const;

    /**
     * Parse @a str and determine whether it references a logical save slot.
     *
     * @param str  String to be parsed. Parse is divided into three passes.
     *             Pass 1: Check for a known game-save description which matches this.
     *                 Search is in logical save slot creation order.
     *             Pass 2: Check for keyword identifiers.
     *                 <auto>  = The "auto save" slot.
     *                 <last>  = The last used slot.
     *                 <quick> = The currently nominated "quick save" slot.
     *             Pass 3: Check for a unique save slot identifier.
     *
     * @return  The referenced Slot otherwise @c 0.
     */
    Slot *slotByUserInput(de::String const &str) const;

    /**
     * Force a manual update of all save slot statuses and update any linked menu items.
     * Calling this is usually unnecessary as the status of a slot is automatically updated
     * when the associated saved session changes.
     */
    void updateAll();

    /**
     * Register the console commands and variables of this module.
     *
     * - game-save-last-slot   Last-used slot. Can be @c -1 (not set).
     * - game-save-quick-slot  Nominated quick-save slot. Can be @c -1 (not set).
     */
    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

typedef SaveSlots::Slot SaveSlot;

#endif  // LIBCOMMON_SAVESLOTS_H
