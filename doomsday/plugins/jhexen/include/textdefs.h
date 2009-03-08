/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * textdefs.h:
 */

#ifndef __TEXTDEFS_H__
#define __TEXTDEFS_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#define GET_TXT(x)          ((*gi.text)[x].text)

#define NUM_QUITMESSAGES    0

#define PRESSKEY            GET_TXT(TXT_PRESSKEY)
#define PRESSYN             GET_TXT(TXT_PRESSYN)
#define TXT_PAUSED          GET_TXT(TXT_TXT_PAUSED)
#define QUITMSG             GET_TXT(TXT_QUITMSG)
#define LOADNET             GET_TXT(TXT_LOADNET)
#define QLOADNET            GET_TXT(TXT_QLOADNET)
#define QSAVESPOT           GET_TXT(TXT_QSAVESPOT)
#define SAVEDEAD            GET_TXT(TXT_SAVEDEAD)
#define SAVEOUTMAP          GET_TXT(TXT_SAVEOUTMAP)
#define QSPROMPT            GET_TXT(TXT_QSPROMPT)
#define QLPROMPT            GET_TXT(TXT_QLPROMPT)
#define NEWGAME             GET_TXT(TXT_NEWGAME)
#define MSGOFF              GET_TXT(TXT_MSGOFF)
#define MSGON               GET_TXT(TXT_MSGON)
#define NETEND              GET_TXT(TXT_NETEND)
#define ENDGAME             GET_TXT(TXT_ENDGAME)
#define ENDNOGAME           GET_TXT(TXT_ENDNOGAME)

#define SUICIDEOUTMAP        GET_TXT(TXT_SUICIDEOUTMAP)
#define SUICIDEASK          GET_TXT(TXT_SUICIDEASK)

#define DOSY                GET_TXT(TXT_DOSY)
#define TXT_GAMMA_LEVEL_OFF GET_TXT(TXT_TXT_GAMMA_LEVEL_OFF)
#define TXT_GAMMA_LEVEL_1   GET_TXT(TXT_TXT_GAMMA_LEVEL_1)
#define TXT_GAMMA_LEVEL_2   GET_TXT(TXT_TXT_GAMMA_LEVEL_2)
#define TXT_GAMMA_LEVEL_3   GET_TXT(TXT_TXT_GAMMA_LEVEL_3)
#define TXT_GAMMA_LEVEL_4   GET_TXT(TXT_TXT_GAMMA_LEVEL_4)
#define EMPTYSTRING         GET_TXT(TXT_EMPTYSTRING)

// Mana
#define TXT_MANA_1          GET_TXT(TXT_TXT_MANA_1)
#define TXT_MANA_2          GET_TXT(TXT_TXT_MANA_2)
#define TXT_MANA_BOTH       GET_TXT(TXT_TXT_MANA_BOTH)

// Keys
#define TXT_KEY_STEEL       GET_TXT(TXT_TXT_KEY_STEEL)
#define TXT_KEY_CAVE        GET_TXT(TXT_TXT_KEY_CAVE)
#define TXT_KEY_AXE         GET_TXT(TXT_TXT_KEY_AXE)
#define TXT_KEY_FIRE        GET_TXT(TXT_TXT_KEY_FIRE)
#define TXT_KEY_EMERALD     GET_TXT(TXT_TXT_KEY_EMERALD)
#define TXT_KEY_DUNGEON     GET_TXT(TXT_TXT_KEY_DUNGEON)
#define TXT_KEY_SILVER      GET_TXT(TXT_TXT_KEY_SILVER)
#define TXT_KEY_RUSTED      GET_TXT(TXT_TXT_KEY_RUSTED)
#define TXT_KEY_HORN        GET_TXT(TXT_TXT_KEY_HORN)
#define TXT_KEY_SWAMP       GET_TXT(TXT_TXT_KEY_SWAMP)
#define TXT_KEY_CASTLE      GET_TXT(TXT_TXT_KEY_CASTLE)

// Inventory items
#define TXT_INV_INVULNERABILITY GET_TXT(TXT_TXT_INV_INVULNERABILITY)
#define TXT_INV_HEALTH      GET_TXT(TXT_TXT_INV_HEALTH)
#define TXT_INV_SUPERHEALTH GET_TXT(TXT_TXT_INV_SUPERHEALTH)
#define TXT_INV_SUMMON      GET_TXT(TXT_TXT_INV_SUMMON)
#define TXT_INV_TORCH       GET_TXT(TXT_TXT_INV_TORCH)
#define TXT_INV_EGG         GET_TXT(TXT_TXT_INV_EGG)
#define TXT_INV_FLY         GET_TXT(TXT_TXT_INV_FLY)
#define TXT_INV_TELEPORT    GET_TXT(TXT_TXT_INV_TELEPORT)
#define TXT_INV_POISONBAG   GET_TXT(TXT_TXT_INV_POISONBAG)
#define TXT_INV_TELEPORTOTHER GET_TXT(TXT_TXT_INV_TELEPORTOTHER)
#define TXT_INV_SPEED       GET_TXT(TXT_TXT_INV_SPEED)
#define TXT_INV_BOOSTMANA   GET_TXT(TXT_TXT_INV_BOOSTMANA)
#define TXT_INV_BOOSTARMOR  GET_TXT(TXT_TXT_INV_BOOSTARMOR)
#define TXT_INV_BLASTRADIUS GET_TXT(TXT_TXT_INV_BLASTRADIUS)
#define TXT_INV_HEALINGRADIUS GET_TXT(TXT_TXT_INV_HEALINGRADIUS)

// Puzzle items
#define TXT_INV_PUZZSKULL   GET_TXT(TXT_TXT_INV_PUZZSKULL)
#define TXT_INV_PUZZGEMBIG  GET_TXT(TXT_TXT_INV_PUZZGEMBIG)
#define TXT_INV_PUZZGEMRED  GET_TXT(TXT_TXT_INV_PUZZGEMRED)
#define TXT_INV_PUZZGEMGREEN1 GET_TXT(TXT_TXT_INV_PUZZGEMGREEN1)
#define TXT_INV_PUZZGEMGREEN2 GET_TXT(TXT_TXT_INV_PUZZGEMGREEN2)
#define TXT_INV_PUZZGEMBLUE1 GET_TXT(TXT_TXT_INV_PUZZGEMBLUE1)
#define TXT_INV_PUZZGEMBLUE2 GET_TXT(TXT_TXT_INV_PUZZGEMBLUE2)
#define TXT_INV_PUZZBOOK1   GET_TXT(TXT_TXT_INV_PUZZBOOK1)
#define TXT_INV_PUZZBOOK2   GET_TXT(TXT_TXT_INV_PUZZBOOK2)
#define TXT_INV_PUZZSKULL2  GET_TXT(TXT_TXT_INV_PUZZSKULL2)
#define TXT_INV_PUZZFWEAPON GET_TXT(TXT_TXT_INV_PUZZFWEAPON)
#define TXT_INV_PUZZCWEAPON GET_TXT(TXT_TXT_INV_PUZZCWEAPON)
#define TXT_INV_PUZZMWEAPON GET_TXT(TXT_TXT_INV_PUZZMWEAPON)
#define TXT_INV_PUZZGEAR    GET_TXT(TXT_TXT_INV_PUZZGEAR)
#define TXT_USEPUZZLEFAILED GET_TXT(TXT_TXT_USEPUZZLEFAILED)

// Items
#define TXT_ITEMHEALTH      GET_TXT(TXT_TXT_ITEMHEALTH)
#define TXT_ITEMBAGOFHOLDING GET_TXT(TXT_TXT_ITEMBAGOFHOLDING)
#define TXT_ITEMSHIELD1     GET_TXT(TXT_TXT_ITEMSHIELD1)
#define TXT_ITEMSHIELD2     GET_TXT(TXT_TXT_ITEMSHIELD2)
#define TXT_ITEMSUPERMAP    GET_TXT(TXT_TXT_ITEMSUPERMAP)
#define TXT_ARMOR1          GET_TXT(TXT_TXT_ARMOR1)
#define TXT_ARMOR2          GET_TXT(TXT_TXT_ARMOR2)
#define TXT_ARMOR3          GET_TXT(TXT_TXT_ARMOR3)
#define TXT_ARMOR4          GET_TXT(TXT_TXT_ARMOR4)

// Weapons
#define TXT_WEAPON_F2       GET_TXT(TXT_TXT_WEAPON_F2)
#define TXT_WEAPON_F3       GET_TXT(TXT_TXT_WEAPON_F3)
#define TXT_WEAPON_F4       GET_TXT(TXT_TXT_WEAPON_F4)
#define TXT_WEAPON_C2       GET_TXT(TXT_TXT_WEAPON_C2)
#define TXT_WEAPON_C3       GET_TXT(TXT_TXT_WEAPON_C3)
#define TXT_WEAPON_C4       GET_TXT(TXT_TXT_WEAPON_C4)
#define TXT_WEAPON_M2       GET_TXT(TXT_TXT_WEAPON_M2)
#define TXT_WEAPON_M3       GET_TXT(TXT_TXT_WEAPON_M3)
#define TXT_WEAPON_M4       GET_TXT(TXT_TXT_WEAPON_M4)
#define TXT_QUIETUS_PIECE   GET_TXT(TXT_TXT_QUIETUS_PIECE)
#define TXT_WRAITHVERGE_PIECE GET_TXT(TXT_TXT_WRAITHVERGE_PIECE)
#define TXT_BLOODSCOURGE_PIECE GET_TXT(TXT_TXT_BLOODSCOURGE_PIECE)

#define TXT_CHEATGODON      GET_TXT(TXT_TXT_CHEATGODON)
#define TXT_CHEATGODOFF     GET_TXT(TXT_TXT_CHEATGODOFF)
#define TXT_CHEATNOCLIPON   GET_TXT(TXT_TXT_CHEATNOCLIPON)
#define TXT_CHEATNOCLIPOFF  GET_TXT(TXT_TXT_CHEATNOCLIPOFF)
#define TXT_CHEATWEAPONS    GET_TXT(TXT_TXT_CHEATWEAPONS)
#define TXT_CHEATHEALTH     GET_TXT(TXT_TXT_CHEATHEALTH)
#define TXT_CHEATKEYS       GET_TXT(TXT_TXT_CHEATKEYS)
#define TXT_CHEATSOUNDON    GET_TXT(TXT_TXT_CHEATSOUNDON)
#define TXT_CHEATSOUNDOFF   GET_TXT(TXT_TXT_CHEATSOUNDOFF)
#define TXT_CHEATTICKERON   GET_TXT(TXT_TXT_CHEATTICKERON)
#define TXT_CHEATTICKEROFF  GET_TXT(TXT_TXT_CHEATTICKEROFF)
#define TXT_CHEATINVITEMS3 GET_TXT(TXT_TXT_CHEATINVITEMS3)
#define TXT_CHEATITEMSFAIL GET_TXT(TXT_TXT_CHEATITEMSFAIL)
#define TXT_CHEATWARP       GET_TXT(TXT_TXT_CHEATWARP)
#define TXT_CHEATSCREENSHOT GET_TXT(TXT_TXT_CHEATSCREENSHOT)
#define TXT_CHEATIDDQD      GET_TXT(TXT_TXT_CHEATIDDQD)
#define TXT_CHEATIDKFA      GET_TXT(TXT_TXT_CHEATIDKFA)
#define TXT_CHEATBADINPUT   GET_TXT(TXT_TXT_CHEATBADINPUT)
#define TXT_CHEATNOMAP      GET_TXT(TXT_TXT_CHEATNOMAP)

#define TXT_GAMESAVED       GET_TXT(TXT_TXT_GAMESAVED)

#define HUSTR_CHATMACRO1    GET_TXT(TXT_HUSTR_CHATMACRO1)
#define HUSTR_CHATMACRO2    GET_TXT(TXT_HUSTR_CHATMACRO2)
#define HUSTR_CHATMACRO3    GET_TXT(TXT_HUSTR_CHATMACRO3)
#define HUSTR_CHATMACRO4    GET_TXT(TXT_HUSTR_CHATMACRO4)
#define HUSTR_CHATMACRO5    GET_TXT(TXT_HUSTR_CHATMACRO5)
#define HUSTR_CHATMACRO6    GET_TXT(TXT_HUSTR_CHATMACRO6)
#define HUSTR_CHATMACRO7    GET_TXT(TXT_HUSTR_CHATMACRO7)
#define HUSTR_CHATMACRO8    GET_TXT(TXT_HUSTR_CHATMACRO8)
#define HUSTR_CHATMACRO9    GET_TXT(TXT_HUSTR_CHATMACRO9)
#define HUSTR_CHATMACRO0    GET_TXT(TXT_HUSTR_CHATMACRO0)

#define AMSTR_FOLLOWON      GET_TXT(TXT_AMSTR_FOLLOWON)
#define AMSTR_FOLLOWOFF     GET_TXT(TXT_AMSTR_FOLLOWOFF)
#define AMSTR_ROTATEON      GET_TXT(TXT_AMSTR_ROTATEON)
#define AMSTR_ROTATEOFF     GET_TXT(TXT_AMSTR_ROTATEOFF)

#define AMSTR_GRIDON        GET_TXT(TXT_AMSTR_GRIDON)
#define AMSTR_GRIDOFF       GET_TXT(TXT_AMSTR_GRIDOFF)

#define AMSTR_MARKEDSPOT    GET_TXT(TXT_AMSTR_MARKEDSPOT)
#define AMSTR_MARKSCLEARED  GET_TXT(TXT_AMSTR_MARKSCLEARED)

#endif
