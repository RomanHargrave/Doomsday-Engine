/** @file inputsystem.h  Input subsystem.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef CLIENT_INPUTSYSTEM_H
#define CLIENT_INPUTSYSTEM_H

#include <functional>
#include <de/Error>
#include <de/Record>
#include <de/System>
#include "ddevent.h"
#include "SettingsRegister"

class BindContext;
class InputDevice;

#define DEFAULT_BINDING_CONTEXT_NAME    "game"
#define CONSOLE_BINDING_CONTEXT_NAME    "console"
#define UI_BINDING_CONTEXT_NAME         "deui"
#define GLOBAL_BINDING_CONTEXT_NAME     "global"

/**
 * Input devices, binding context stack and event tracking.
 *
 * @par Bindings
 *
 * Bindings are Record structures which describe an event => action trigger
 * relationship. The event being a specific observable state scenario (such as
 * a keypress on a keyboard) and the trigger, a more abstract action that can
 * be "bound" to it (such as executing a console command).
 *
 * However, it is important to note this relationship is modelled from the
 * @em action's perspective, rather than that of the event. This is to support
 * stronger decoupling of the origin from any possible action.
 *
 * Once configured (@see configure()), bindings may be freely moved between
 * contexts, assuming it makes sense to do so. The bindings themselves do not
 * reference the context in which they might reside.
 *
 * @todo Input drivers belong in this system.
 *
 * @ingroup ui
 */
class InputSystem : public de::System
{
public:
    InputSystem();

    SettingsRegister &settings();

    // System.
    void timeChanged(de::Clock const &);

public: // Input devices -----------------------------------------------------

    /// Required/referenced input device is missing. @ingroup errors
    DENG2_ERROR(MissingDeviceError);

    /*
     * Lookup an InputDevice by it's unique @a id.
     */
    InputDevice &device(int id) const;

    /**
     * Lookup an InputDevice by it's unique @a id.
     *
     * @return  Pointer to the associated InputDevice; otherwise @c nullptr.
     */
    InputDevice *devicePtr(int id) const;

    /**
     * Iterate through all the InputDevices.
     */
    de::LoopResult forAllDevices(std::function<de::LoopResult (InputDevice &)> func) const;

    /**
     * Returns the total number of InputDevices initialized.
     */
    int deviceCount() const;

    /**
     * (Re)initialize the input device models, returning all controls to their
     * default states.
     */
    void initAllDevices();

    /**
     * Returns @c true if the shift key of the keyboard is thought to be down.
     * @todo: Refactor away
     */
    bool shiftDown() const;

public: // Event processing --------------------------------------------------
    /**
     * Clear the input event queue.
     */
    void clearEvents();

    bool ignoreEvents(bool yes = true);

    /**
     * @param ev  A copy is made.
     */
    void postEvent(ddevent_t *ev);

    /**
     * Process all incoming input for the given timestamp.
     * This is called only in the main thread, and also from the busy loop.
     *
     * This gets called at least 35 times per second. Usually more frequently
     * than that.
     */
    void processEvents(timespan_t ticLength);

    void processSharpEvents(timespan_t ticLength);

    /**
     * If an action has been defined for the event, trigger it.
     *
     * This is meant to be used as a way for Widgets to take advantage of the
     * traditional bindings system for user-customizable actions.
     *
     * @param event    Event instance.
     * @param context  Name of the binding context. If empty, all contexts
     *                 all checked.
     *
     * @return @c true if an action was triggered, @c false otherwise.
     */
    bool tryEvent(de::Event const &event, de::String const &context = "");
    bool tryEvent(ddevent_t const &event, de::String const &context = "");

public:

    static bool convertEvent(ddevent_t const &from, event_t &to);
    static bool convertEvent(de::Event const &from, ddevent_t &to);

    /**
     * Updates virtual input device state.
     *
     * Normally this is called automatically at the appropriate time, however
     * if a widget eats an event before it is passed to the bindings system,
     * it might still wish to call this to ensure subsequent bindings are
     * correctly evaluated.
     *
     * @param event  Input event.
     *
     * @todo make private (all widgets should belong to / own a BindContext).
     */
    void trackEvent(de::Event const &event);
    void trackEvent(ddevent_t const &event);

public: // Binding (context) management --------------------------------------

    /// Required/referenced binding context is missing. @ingroup errors
    DENG2_ERROR(MissingContextError);

    void bindDefaults();

    void bindGameDefaults();

    /**
     * Try to make a new (console) command binding.
     *
     * @param eventDesc  Textual descriptor for the event, with the relevant
     *                   context for the would-be binding encoded.
     * @param command    Console command(s) which the binding will execute when
     *                   triggered, if a binding is created.
     *
     * @return  Resultant command binding Record if any.
     */
    de::Record *bindCommand(char const *eventDesc, char const *command);

    /**
     * Try to make a new (player) impulse binding.
     *
     * @param ctrlDesc     Textual descriptor for the device-control event.
     * @param impulseDesc  Player impulse which the binding will execute when
     *                     triggered, if a binding is created. Impulses are
     *                     associated with a context, which determines where
     *                     the binding will be linked, if a binding is created.
     *
     * @return  Resultant impulse binding Record if any.
     */
    de::Record *bindImpulse(char const *ctrlDesc, char const *impulseDesc);

    /**
     * Try to remove the one unique binding associated with @a id.
     *
     * @return  @c true if that binding was removed.
     */
    bool removeBinding(int id);

    /**
     * Remove all bindings in all contexts.
     */
    void removeAllBindings();

    // ---

    /**
     * Enable the contexts for the initial state.
     */
    void initialContextActivations();

    /**
     * Destroy all binding contexts and the bindings within the contexts.
     */
    void clearAllContexts();

    /**
     * Returns @c true if the symbolic @a name references a known context.
     */
    bool hasContext(de::String const &name) const;

    /**
     * Creates a new binding context. The new context has the highest priority
     * of all existing contexts, and is inactive.
     *
     * @param name  A unique, symbolic name for the bind context.
     *
     * @return  Resultant BindContext. Ownership is retained.
     */
    BindContext *newContext(de::String const &name);

    /**
     * Lookup a binding context by symbolic @a name.
     */
    BindContext &context(de::String const &name) const;
    BindContext *contextPtr(de::String const &name) const;

    /**
     * Lookup a binding context by stack @a position.
     */
    BindContext &contextAt(int position) const;

    /**
     * Returns the stack position of the specified binding @a context.
     */
    int contextPositionOf(BindContext *context) const;

    /**
     * Iterate through all the BindContexts from highest to lowest priority.
     */
    de::LoopResult forAllContexts(std::function<de::LoopResult (BindContext &)> func) const;

    /**
     * Returns the total number of binding contexts in the system.
     */
    int contextCount() const;

public:
    /**
     * Register the console commands and variables of this module.
     */
    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

#endif // CLIENT_INPUTSYSTEM_H
