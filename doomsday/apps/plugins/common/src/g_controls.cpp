/**
 * @file g_controls.c
 * Game controls, default bindings. @ingroup libcommon
 *
 * @author Copyright &copy; 1999-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 1993-1996 by id Software, Inc.
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

#include <math.h> // required for sqrt, fabs

#include "common.h"

#include "g_controls.h"
#include "pause.h"
#include "d_netsv.h"
#include "hu_menu.h"
#include "hu_msg.h"
#include "g_common.h"

// TODO Inventory should be exposed to all plugins, or refactored out to a new shared codebase, rather
//      than conditionally added to the common code base
#if __JHERETIC__ || __JHEXEN__
#  include "p_inventory.h"
#endif

// TODO These can ultimately be replaced by migrating all of doomsday to C++ 
//      and exposing this logic as member functions of the player class
//      Additionally, it makes little sense that this is necessary logic for all
//      plugins but Hexen..
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
#  define GOTWPN(x)         (plr->weaponOwned[x])
#  define ISWPN(x)          (plr->readyWeapon == x)
#endif

// XXX Migrated from `JOY` macro.
//     return type of `float` assumed, due to divison being performed.
static inline float JOY(float x)
{
    return x / 100.0F;
}

// TODO The following macros should be converted to constants, 
//      however, they are defined in headers in individual plugins
//      (such as doom's p_local.h).
#define TOCENTER            (-8)
#define DELTAMUL            (6.324555320) // Used when calculating ticcmd_t.lookdirdelta

static int const SLOWTURNTICS = 6;

typedef struct pcontrolstate_s {
    // Looking around.
    float           targetLookOffset;
    float           lookOffset;
    dd_bool         mlookPressed;

    // For accelerative turning:
    float           turnheld;
    float           lookheld;

    int             dclicktime;
    int             dclickstate;
    int             dclicks;
    int             dclicktime2;
    int             dclickstate2;
    int             dclicks2;
} pcontrolstate_t;

// Joystick axes.
typedef enum joyaxis_e {
    JA_X, JA_Y, JA_Z, JA_RX, JA_RY, JA_RZ, JA_SLIDER0, JA_SLIDER1,
    NUM_JOYSTICK_AXES
} joyaxis_t;

D_CMD(DefaultGameBinds);

// Input devices; state controls.
//static int povangle = -1;  ///< @c -1= means centered (really 0 - 7).
//static float mousex;
//static float mousey;

// Player control state.
static pcontrolstate_t controlStates[MAXPLAYERS];

void G_ControlRegister(void)
{
    // Control (options/preferences)
    C_VAR_INT   ("ctl-aim-noauto",  &cfg.common.noAutoAim, 0, 0, 1);
    C_VAR_FLOAT ("ctl-turn-speed",  &cfg.common.turnSpeed, 0, 1, 5);
    C_VAR_INT   ("ctl-run",         &cfg.common.alwaysRun, 0, 0, 1);

// TODO Modularization: Each plugin should be able to register its own controls,
//                      this can be easily factored in to the idTech1 engine support
//                      known as `common`.
#if __JHERETIC__ || __JHEXEN__
    C_VAR_BYTE  ("ctl-inventory-mode", &cfg.inventorySelectMode, 0, 0, 1);
    C_VAR_BYTE  ("ctl-inventory-wrap", &cfg.inventoryWrap, 0, 0, 1);
    C_VAR_BYTE  ("ctl-inventory-use-immediate", &cfg.inventoryUseImmediate, 0, 0, 1);
    C_VAR_BYTE  ("ctl-inventory-use-next", &cfg.inventoryUseNext, 0, 0, 1);
#endif

    C_VAR_FLOAT ("ctl-look-speed",  &cfg.common.lookSpeed, 0, 1, 5);
    C_VAR_INT   ("ctl-look-spring", &cfg.common.lookSpring, 0, 0, 1);

    C_VAR_BYTE  ("ctl-look-pov",    &cfg.common.povLookAround, 0, 0, 1);
    C_VAR_INT   ("ctl-look-joy",    &cfg.common.useJLook, 0, 0, 1);
    C_VAR_INT   ("ctl-look-joy-delta", &cfg.common.jLookDeltaMode, 0, 0, 1);

    C_CMD("defaultgamebindings",    "",     DefaultGameBinds);

    G_DefineControls();
}

void G_DefineControls(void)
{
    P_NewPlayerControl(CTL_WALK, CTLT_NUMERIC, "walk", "game");
    P_NewPlayerControl(CTL_SIDESTEP, CTLT_NUMERIC, "sidestep", "game");
    P_NewPlayerControl(CTL_ZFLY, CTLT_NUMERIC, "zfly", "game");
    P_NewPlayerControl(CTL_TURN, CTLT_NUMERIC, "turn", "game");
    P_NewPlayerControl(CTL_LOOK, CTLT_NUMERIC, "look", "game");
    P_NewPlayerControl(CTL_LOOK_PITCH, CTLT_NUMERIC, "lookpitch", "game");
    P_NewPlayerControl(CTL_HEAD_YAW, CTLT_NUMERIC, "yawhead", "game");
    P_NewPlayerControl(CTL_BODY_YAW, CTLT_NUMERIC, "yawbody", "game");
    P_NewPlayerControl(CTL_SPEED, CTLT_NUMERIC, "speed", "game");
    P_NewPlayerControl(CTL_MODIFIER_1, CTLT_NUMERIC, "strafe", "game");
    P_NewPlayerControl(CTL_ATTACK, CTLT_NUMERIC_TRIGGERED, "attack", "game");
    P_NewPlayerControl(CTL_USE, CTLT_IMPULSE, "use", "game");
    P_NewPlayerControl(CTL_LOOK_CENTER, CTLT_IMPULSE, "lookcenter", "game");
    P_NewPlayerControl(CTL_FALL_DOWN, CTLT_IMPULSE, "falldown", "game");
    P_NewPlayerControl(CTL_JUMP, CTLT_IMPULSE, "jump", "game");
    P_NewPlayerControl(CTL_WEAPON1, CTLT_IMPULSE, "weapon1", "game");
    P_NewPlayerControl(CTL_WEAPON2, CTLT_IMPULSE, "weapon2", "game");
    P_NewPlayerControl(CTL_WEAPON3, CTLT_IMPULSE, "weapon3", "game");
    P_NewPlayerControl(CTL_WEAPON4, CTLT_IMPULSE, "weapon4", "game");
    P_NewPlayerControl(CTL_WEAPON5, CTLT_IMPULSE, "weapon5", "game");
    P_NewPlayerControl(CTL_WEAPON6, CTLT_IMPULSE, "weapon6", "game");
    P_NewPlayerControl(CTL_WEAPON7, CTLT_IMPULSE, "weapon7", "game");
    P_NewPlayerControl(CTL_WEAPON8, CTLT_IMPULSE, "weapon8", "game");
    P_NewPlayerControl(CTL_WEAPON9, CTLT_IMPULSE, "weapon9", "game");
    P_NewPlayerControl(CTL_WEAPON0, CTLT_IMPULSE, "weapon0", "game");
// TODO Modularization: Each plugin should register its own controls
// TODO Future: The weapon inventory should be made abstract and dynamically growing 
//              rather than having a known number of weapons that must have hardcoded
//              support
#if __JDOOM64__
    P_NewPlayerControl(CTL_WEAPON10, CTLT_IMPULSE, "weapon10", "game");
#endif
    P_NewPlayerControl(CTL_NEXT_WEAPON, CTLT_IMPULSE, "nextweapon", "game");
    P_NewPlayerControl(CTL_PREV_WEAPON, CTLT_IMPULSE, "prevweapon", "game");
#if __JHERETIC__ || __JHEXEN__
    P_NewPlayerControl(CTL_USE_ITEM, CTLT_IMPULSE, "useitem", "game");
    P_NewPlayerControl(CTL_NEXT_ITEM, CTLT_IMPULSE, "nextitem", "game");
    P_NewPlayerControl(CTL_PREV_ITEM, CTLT_IMPULSE, "previtem", "game");
    P_NewPlayerControl(CTL_PANIC, CTLT_IMPULSE, "panic", "game");
#endif
#if __JHERETIC__
    P_NewPlayerControl(CTL_TOME_OF_POWER, CTLT_IMPULSE, "tome", "game");
    P_NewPlayerControl(CTL_INVISIBILITY, CTLT_IMPULSE, "invisibility", "game");
    P_NewPlayerControl(CTL_FLY, CTLT_IMPULSE, "fly", "game");
    P_NewPlayerControl(CTL_TORCH, CTLT_IMPULSE, "torch", "game");
    P_NewPlayerControl(CTL_HEALTH, CTLT_IMPULSE, "health", "game");
    P_NewPlayerControl(CTL_SUPER_HEALTH, CTLT_IMPULSE, "superhealth", "game");
    P_NewPlayerControl(CTL_TELEPORT, CTLT_IMPULSE, "teleport", "game");
    P_NewPlayerControl(CTL_FIREBOMB, CTLT_IMPULSE, "firebomb", "game");
    P_NewPlayerControl(CTL_INVULNERABILITY, CTLT_IMPULSE, "invulnerability", "game");
    P_NewPlayerControl(CTL_EGG, CTLT_IMPULSE, "egg", "game");
#endif
#if __JHEXEN__
    P_NewPlayerControl(CTL_FLY, CTLT_IMPULSE, "fly", "game");
    P_NewPlayerControl(CTL_TORCH, CTLT_IMPULSE, "torch", "game");
    P_NewPlayerControl(CTL_HEALTH, CTLT_IMPULSE, "health", "game");
    P_NewPlayerControl(CTL_MYSTIC_URN, CTLT_IMPULSE, "mysticurn", "game");
    P_NewPlayerControl(CTL_KRATER, CTLT_IMPULSE, "krater", "game");
    P_NewPlayerControl(CTL_SPEED_BOOTS, CTLT_IMPULSE, "speedboots", "game");
    P_NewPlayerControl(CTL_BLAST_RADIUS, CTLT_IMPULSE, "blast", "game");
    P_NewPlayerControl(CTL_TELEPORT, CTLT_IMPULSE, "teleport", "game");
    P_NewPlayerControl(CTL_TELEPORT_OTHER, CTLT_IMPULSE, "teleportother", "game");
    P_NewPlayerControl(CTL_POISONBAG, CTLT_IMPULSE, "poisonbag", "game");
    P_NewPlayerControl(CTL_FIREBOMB, CTLT_IMPULSE, "firebomb", "game");
    P_NewPlayerControl(CTL_INVULNERABILITY, CTLT_IMPULSE, "invulnerability", "game");
    P_NewPlayerControl(CTL_DARK_SERVANT, CTLT_IMPULSE, "darkservant", "game");
    P_NewPlayerControl(CTL_EGG, CTLT_IMPULSE, "egg", "game");
#endif

    P_NewPlayerControl(CTL_LOG_REFRESH, CTLT_IMPULSE, "msgrefresh", "game");

    P_NewPlayerControl(CTL_MAP, CTLT_IMPULSE, "automap", "game");
    P_NewPlayerControl(CTL_MAP_PAN_X, CTLT_NUMERIC, "mappanx", "map-freepan");
    P_NewPlayerControl(CTL_MAP_PAN_Y, CTLT_NUMERIC, "mappany", "map-freepan");
    P_NewPlayerControl(CTL_MAP_ZOOM, CTLT_NUMERIC, "mapzoom", "map");
    P_NewPlayerControl(CTL_MAP_ZOOM_MAX, CTLT_IMPULSE, "zoommax", "map");
    P_NewPlayerControl(CTL_MAP_FOLLOW, CTLT_IMPULSE, "follow", "map");
    P_NewPlayerControl(CTL_MAP_ROTATE, CTLT_IMPULSE, "rotate", "map");
    P_NewPlayerControl(CTL_MAP_MARK_ADD, CTLT_IMPULSE, "addmark", "map");
    P_NewPlayerControl(CTL_MAP_MARK_CLEAR_ALL, CTLT_IMPULSE, "clearmarks", "map");

    P_NewPlayerControl(CTL_HUD_SHOW, CTLT_IMPULSE, "showhud", "game");
    P_NewPlayerControl(CTL_SCORE_SHOW, CTLT_IMPULSE, "showscore", "game");
}

// TODO Modularization: Each game should register its own custom defaults
D_CMD(DefaultGameBinds)
{
    /// @todo: When the actual bindings setup UI is done, these default bindings
    /// should be generated by the engine based on some higher level metadata,
    /// described in a text file.

    // Traditional key bindings plus WASD and mouse look, and reasonable joystick defaults.
    const char* binds[] = {
        // Basic movement:
        "bindcontrol attack key-ctrl",
        "bindcontrol speed key-shift",
        "bindevent key-capslock-down {toggle ctl-run}",
        "bindcontrol strafe key-alt",
        "bindcontrol walk key-up",
        "bindcontrol walk key-w",
        "bindcontrol walk key-down-inverse",
        "bindcontrol walk key-s-inverse",
        "bindcontrol sidestep key-period",
        "bindcontrol sidestep key-d",
        "bindcontrol sidestep key-right+modifier-1-down",
        "bindcontrol sidestep key-comma-inverse",
        "bindcontrol sidestep key-a-inverse",
        "bindcontrol sidestep key-left-inverse+modifier-1-down",
        "bindcontrol zfly key-pgup-staged",
        "bindcontrol zfly key-e-staged",
        "bindcontrol zfly key-ins-staged-inverse",
        "bindcontrol zfly key-q-staged-inverse",
        "bindevent key-home-down {impulse falldown}",
        "bindevent key-f-down {impulse falldown}",
        "bindcontrol turn key-left-staged-inverse+modifier-1-up",
        "bindcontrol turn key-right-staged+modifier-1-up",
        "bindcontrol look key-delete-staged-inverse",
        "bindcontrol look key-pgdown-staged",
        "bindevent key-end-down {impulse lookcenter}",
        "bindevent key-slash {impulse jump}",
        "bindevent key-backslash {impulse jump}",
        "bindevent key-space-down {impulse use}",

        "bindevent sym-control-doubleclick-positive-walk {impulse use %p}",

        // Weapon keys:
        "bindevent key-1 {impulse weapon1}",
        "bindevent key-2 {impulse weapon2}",
        "bindevent key-3 {impulse weapon3}",
        "bindevent key-4 {impulse weapon4}",
#ifndef __JHEXEN__
        "bindevent key-5 {impulse weapon5}",
        "bindevent key-6 {impulse weapon6}",
        "bindevent key-7 {impulse weapon7}",
        "bindevent key-8 {impulse weapon8}",
        "bindevent key-9 {impulse weapon9}",
#endif
#ifdef __JDOOM64__
        "bindevent key-0 {impulse weapon10}",
#endif

#ifdef __JHERETIC__
        "bindevent key-backspace {impulse tome}",
#endif

#ifdef __JHEXEN__
        "bindevent key-backspace {impulse panic}",
        "bindevent key-b {impulse panic}",
        "bindevent key-v {impulse health}",
        "bindevent key-9 {impulse blast}",
        "bindevent key-8 {impulse teleport}",
        "bindevent key-7 {impulse teleportother}",
        "bindevent key-5 {impulse invulnerability}",
        "bindevent key-6 {impulse egg}",
#endif

#if __JHERETIC__ || __JHEXEN__
        "bindevent key-sqbracketleft {impulse previtem}",
        "bindevent key-sqbracketleft-repeat {impulse previtem}",
        "bindevent key-z {impulse previtem}",
        "bindevent key-z-repeat {impulse previtem}",
        "bindevent key-sqbracketright {impulse nextitem}",
        "bindevent key-sqbracketright-repeat {impulse nextitem}",
        "bindevent key-c {impulse nextitem}",
        "bindevent key-c-repeat {impulse nextitem}",
        "bindevent key-return {impulse useitem}",
        "bindevent key-x {impulse useitem}",
#endif

        // Player controls: mouse
        "bindcontrol turn mouse-x+modifier-1-up",
        "bindcontrol sidestep mouse-x+modifier-1-down",
        "bindcontrol look mouse-y",
        "bindcontrol attack mouse-left",
        "bindevent mouse-right-down {impulse use}",
        "bindevent mouse-wheelup {impulse nextweapon}",
        "bindevent mouse-wheeldown {impulse prevweapon}",

        // Player controls: joystick
        "bindcontrol turn joy-x+modifier-1-up",
        "bindcontrol sidestep joy-x+modifier-1-down",
        "bindcontrol walk joy-y-inverse",

        // Chat events:
        "bindevent key-t+multiplayer beginchat",
        "bindevent key-g+multiplayer {beginchat 0}",
        "bindevent key-y+multiplayer {beginchat 1}",
        "bindevent key-r+multiplayer {beginchat 2}",
        "bindevent key-b+multiplayer {beginchat 3}",
        "bindevent chat:key-return chatcomplete",
        "bindevent chat:key-escape chatcancel",
        "bindevent chat:key-f1 {chatsendmacro 0}",
        "bindevent chat:key-f2 {chatsendmacro 1}",
        "bindevent chat:key-f3 {chatsendmacro 2}",
        "bindevent chat:key-f4 {chatsendmacro 3}",
        "bindevent chat:key-f5 {chatsendmacro 4}",
        "bindevent chat:key-f6 {chatsendmacro 5}",
        "bindevent chat:key-f7 {chatsendmacro 6}",
        "bindevent chat:key-f8 {chatsendmacro 7}",
        "bindevent chat:key-f9 {chatsendmacro 8}",
        "bindevent chat:key-f10 {chatsendmacro 9}",
        "bindevent chat:key-backspace chatdelete",

        // Map events:
        "bindevent key-tab {impulse automap}",
        "bindevent map:key-f {impulse follow}",
        "bindevent map:key-r {impulse rotate}",
        "bindcontrol mapzoom key-equals",
        "bindcontrol mapzoom key-minus-inverse",
        "bindevent map:key-0 {impulse zoommax}",
        "bindevent map:key-m {impulse addmark}",
        "bindevent map:key-c {impulse clearmarks}",
        "bindcontrol mappany key-up",
        "bindcontrol mappany key-w",
        "bindcontrol mappany key-down-inverse",
        "bindcontrol mappany key-s-inverse",
        "bindcontrol mappanx key-right",
        "bindcontrol mappanx key-d",
        "bindcontrol mappanx key-left-inverse",
        "bindcontrol mappanx key-a-inverse",

        // UI events:
        "bindevent shortcut:key-esc menu",
#if !__JDOOM64__
        "bindevent shortcut:key-f1 helpscreen",
#endif
        "bindevent shortcut:key-f2 savegame",
        "bindevent shortcut:key-f3 loadgame",
        "bindevent shortcut:key-f4 {menu soundoptions}",
        "bindevent shortcut:key-f6 quicksave",
        "bindevent shortcut:key-f7 endgame",
        "bindevent shortcut:key-f8 {toggle msg-show}",
        "bindevent shortcut:key-f9 quickload",
        "bindevent shortcut:key-f10 quit",
        //"bindevent shortcut:key-f11 togglegamma",
        "bindevent shortcut:key-print screenshot",
        "bindevent shortcut:key-f12 screenshot",

        "bindevent key-pause pause",
        "bindevent key-p pause",

        "bindevent key-h {impulse showhud}",
        "bindevent key-backslash-down {impulse showscore}",
        "bindevent key-backslash-repeat {impulse showscore}",
        "bindevent key-minus-down {sub view-size 1}",
        "bindevent key-minus-repeat {sub view-size 1}",
        "bindevent key-equals-down {add view-size 1}",
        "bindevent key-equals-repeat {add view-size 1}",

        // Player message log:
#if !defined(__JHEXEN__) && !defined(__JHERETIC__)
        "bindevent key-return {impulse msgrefresh}",
#endif

        // Menu events:
        "bindevent menu:key-backspace menuback",
        "bindevent menu:key-backspace-repeat menuback",
        "bindevent menu:mouse-right menuback",
        "bindevent menu:mouse-right-repeat menuback",
        "bindevent menu:key-up menuup",
        "bindevent menu:key-up-repeat menuup",
        "bindevent menu:key-w menuup",
        "bindevent menu:key-w-repeat menuup",
        "bindevent menu:key-down menudown",
        "bindevent menu:key-down-repeat menudown",
        "bindevent menu:key-s menudown",
        "bindevent menu:key-s-repeat menudown",
        "bindevent menu:key-left menuleft",
        "bindevent menu:key-left-repeat menuleft",
        "bindevent menu:key-a menuleft",
        "bindevent menu:key-a-repeat menuleft",
        "bindevent menu:key-right menuright",
        "bindevent menu:key-right-repeat menuright",
        "bindevent menu:key-d menuright",
        "bindevent menu:key-d-repeat menuright",
        "bindevent menu:key-return menuselect",
        "bindevent menu:mouse-left-down menuselect",
        "bindevent menu:key-delete menudelete",
        "bindevent menu:key-pgup menupageup",
        "bindevent menu:key-pgup-repeat menupageup",
        "bindevent menu:key-pgdown menupagedown",
        "bindevent menu:key-pgdown-repeat menupagedown",

        // On-screen messages:
        "bindevent message:key-y messageyes",
        "bindevent message:mouse-left messageyes",
        "bindevent message:key-n messageno",
        "bindevent message:mouse-right messageno",
        "bindevent message:key-escape messagecancel",

        NULL
    };
    int i;
    for(i = 0; binds[i]; ++i)
    {
        DD_Execute(false, binds[i]);
    }
    return true;
}

/**
 * Registers the additional bind classes the game requires
 *
 * (Doomsday manages the bind class stack which forms the dynamic event
 * responder chain).
 */
void G_RegisterBindClasses(void)
{
    /// @todo Move the game's binding class creation here.
}

/**
 * Retrieve the look offset for the given player.
 */
float G_GetLookOffset(int pnum)
{
    return controlStates[pnum].lookOffset;
}


/**
 * Updates the viewers' look offset.
 */
void P_PlayerThinkHeadTurning(int pnum, timespan_t ticLength)
{
    pcontrolstate_t *state = &controlStates[pnum];
    float pos;

    DENG_UNUSED(ticLength);

    // Returned pos is in range -1...+1.
    P_GetControlState(pnum, CTL_HEAD_YAW, &pos, NULL);

    state->lookOffset = pos * .5f;

}

void G_ControlReset(void)
{
    if(IS_CLIENT) DD_Execute(true, "resetctlaccum");
}

/**
 * Resets the look offsets.
 * Called e.g. when starting a new map.
 */
void G_ResetLookOffset(int pnum)
{
    pcontrolstate_t *cstate = &controlStates[pnum];

    cstate->lookOffset = 0;
    cstate->targetLookOffset = 0;
    cstate->lookheld = 0;
}
