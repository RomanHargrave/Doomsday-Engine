/** @file bindcontext.h  Input system binding context.
 *
 * @authors Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_INPUTSYSTEM_BINDCONTEXT_H
#define CLIENT_INPUTSYSTEM_BINDCONTEXT_H

#include <functional>
#include <de/Action>
#include <de/Observers>
#include "commandbinding.h"
#include "impulsebinding.h"

/// @todo: Move to public API
typedef int (*FallbackResponderFunc)(event_t *);
typedef int (*DDFallbackResponderFunc)(ddevent_t const *);
// todo ends

struct PlayerImpulse;

/**
 * Contextualized grouping of input (and windowing system) event bindings.
 *
 * @ingroup ui
 */
class BindContext
{
public:
    /// Notified when the active state of the context changes.
    DENG2_DEFINE_AUDIENCE2(ActiveChange, void bindContextActiveChanged(BindContext &context))

    /// Notified when the list of devices to acquire changes.
    DENG2_DEFINE_AUDIENCE2(AcquireDeviceChange, void bindContextAcquireDeviceChanged(BindContext &context))

    /// Notified whenever a new binding is made in this context.
    DENG2_DEFINE_AUDIENCE2(BindingAddition, void bindContextBindingAdded(BindContext &context, void *binding, bool isCommand))

public:
    /**
     * @param name  Symbolic name for the context.
     */
    explicit BindContext(de::String const &name);
    ~BindContext();

    /**
     * Returns @c true if the context is @em active, meaning, bindings in the context
     * are in effect and their associated action(s) will be executed if triggered.
     *
     * @see activate(), deactivate()
     */
    bool isActive() const;

    /**
     * Returns @c true if the context is @em protected, meaning, it should not be
     * manually (de)activated by the end user, directly.
     *
     * @see protect(), unprotect()
     */
    bool isProtected() const;

    /**
     * Change the @em protected state of the context.
     *
     * @param yes  @c true= protected.
     *
     * @see isProtected()
     */
    void protect(bool yes = true);
    inline void unprotect(bool yes = true) { protect(!yes); }

    /**
     * Returns the symbolic name of the context.
     */
    de::String name() const;
    void setName(de::String const &newName);

    /**
     * (De)activate the context, causing re-evaluation of the binding context stack.
     * The effective bindings for events may change as a result of calling this.
     *
     * @param yes  @c true= activate if inactive, and vice versa.
     */
    void activate(bool yes = true);
    inline void deactivate(bool yes = true) { activate(!yes); }

    void acquire(int deviceId, bool yes = true);
    void acquireAll(bool yes = true);

    bool willAcquire(int deviceId) const;
    bool willAcquireAll() const;

public: // Binding management: ------------------------------------------------------

    void clearAllBindings();

    /**
     * @param id  Unique identifier of the binding to delete.
     * @return  @c true if the binding was found and deleted.
     */
    bool deleteBinding(int id);

    /**
     * Delete all other bindings matching either @a commandBind or @a impulseBind.
     */
    void deleteMatching(CommandBinding const *commandBind, ImpulseBinding const *impulseBind);

    /**
     * Looks through the context for a binding that matches either @a match1 or @a match2.
     */
    bool findMatchingBinding(CommandBinding const *match1, ImpulseBinding const *match2,
                             CommandBinding **cmdResult, ImpulseBinding **impResult) const;

    // Commands ---------------------------------------------------------------------

    CommandBinding *bindCommand(char const *eventDesc, char const *command);

    /**
     * @param deviceId  (@c < 0 || >= NUM_INPUT_DEVICES) for wildcard search.
     */
    CommandBinding *findCommandBinding(char const *command, int deviceId = -1) const;

    /**
     * Iterate through all the CommandBindings of the context.
     */
    de::LoopResult forAllCommandBindings(std::function<de::LoopResult (CommandBinding &)> func) const;

    /**
     * Returns the total number of command bindings in the context.
     */
    int commandBindingCount() const;

    // Impulses ---------------------------------------------------------------------

    /**
     * @param ctrlDesc     Device-control descriptor.
     * @param impulse      Player impulse to bind to.
     * @param localPlayer  Local player number.
     *
     * @todo: Parse the the impulse descriptor here? -ds
     */
    ImpulseBinding *bindImpulse(char const *ctrlDesc, PlayerImpulse const &impulse,
                                int localPlayer);

    ImpulseBinding *findImpulseBinding(int deviceId, ibcontroltype_t bindType, int controlId) const;

    /**
     * Iterate through all the ImpulseBindings of the context.
     *
     * @param localPlayer  (@c < 0 || >= DDMAXPLAYERS) for all local players.
     */
    de::LoopResult forAllImpulseBindings(int localPlayer, std::function<de::LoopResult (ImpulseBinding &)> func) const;

    inline de::LoopResult forAllImpulseBindings(std::function<de::LoopResult (ImpulseBinding &)> func) const {
        return forAllImpulseBindings(-1/*all local players*/, func);
    }

    /**
     * Returns the total number of impulse bindings in the context.
     *
     * @param localPlayer  (@c < 0 || >= DDMAXPLAYERS) for all local players.
     */
    int impulseBindingCount(int localPlayer = -1) const;

public: // Triggering: --------------------------------------------------------------

    /**
     * Finds the action bound to a given event.
     *
     * @param event                            Event to match against.
     * @param respectHigherAssociatedContexts  Bindings shadowed by higher active contexts.
     *
     * @return Action instance (caller gets ownership), or @c nullptr if not found.
     */
    de::Action *actionForEvent(ddevent_t const &event,
                               bool respectHigherAssociatedContexts = true) const;

    /**
     * @todo: Conceptually the fallback responders don't belong; instead of "responding"
     * (immediately performing a reaction), we should be returning an Action instance. -jk
     */
    int tryFallbackResponders(ddevent_t const &event, event_t &ev, bool validGameEvent) const;
    void setFallbackResponder(FallbackResponderFunc newResponderFunc);
    void setDDFallbackResponder(DDFallbackResponderFunc newResponderFunc);

private:
    DENG2_PRIVATE(d)
};

#endif // CLIENT_INPUTSYSTEM_BINDCONTEXT_H
