/** @file gamesession.cpp  Logical game session and saved session marshalling.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "gamesession.h"

#include <de/App>
#include <de/ArrayValue>
#include <de/NumberValue>
#include <de/RecordValue>
#include <de/Time>
#include <de/ZipArchive>
#include <de/game/SavedSession>
#include <doomsday/defs/episode.h>
#include "api_gl.h"
#include "d_netsv.h"
#include "g_common.h"
#include "g_game.h"
#include "r_common.h"
#include "hu_menu.h"
#include "hu_inventory.h"
#include "mapstatewriter.h"
#include "p_inventory.h"
#include "p_map.h"
#include "p_mapsetup.h"
#include "p_savedef.h"
#include "p_saveg.h"
#include "p_saveio.h"
#include "p_sound.h"
#include "p_tick.h"
#if __JDOOM__
#  include "doomv9mapstatereader.h"
#endif
#if __JHERETIC__
#  include "hereticv13mapstatereader.h"
#endif

using namespace de;
using de::game::SavedSession;
using de::game::SessionMetadata;

namespace common {

namespace internal
{
    static String composeSaveInfo(SessionMetadata const &metadata)
    {
        String info;
        QTextStream os(&info);
        os.setCodec("UTF-8");

        // Write header and misc info.
        Time now;
        os <<   "# Doomsday Engine saved game session package.\n#"
           << "\n# Generator: GameSession (libcommon)"
           << "\n# Generation Date: " + now.asDateTime().toString(Qt::SystemLocaleShortDate);

        // Write metadata.
        os << "\n\n" + metadata.asTextWithInfoSyntax() + "\n";

        return info;
    }

    static Block serializeCurrentMapState(bool excludePlayers = false)
    {
        Block data;
        SV_OpenFileForWrite(data);
        writer_s *writer = SV_NewWriter();
        MapStateWriter().write(writer, excludePlayers);
        Writer_Delete(writer);
        SV_CloseFile();
        return data;
    }

    /**
     * Lookup the briefing Finale for the current episode, map (if any).
     */
    static Record const *finaleBriefing(de::Uri const &mapUri)
    {
        if(::briefDisabled) return nullptr;

        // In a networked game the server will schedule the brief.
        if(IS_CLIENT || Get(DD_PLAYBACK)) return nullptr;

        // If we're already in the INFINE state, don't start a finale.
        if(G_GameState() == GS_INFINE) return nullptr;

        // Is there such a finale definition?
        return Defs().finales.tryFind("before", mapUri.compose());
    }
}

using namespace internal;

static GameSession *singleton;

static String const internalSavePath = "/home/cache/internal.save";

DENG2_PIMPL(GameSession), public SavedSession::IMapStateReaderFactory
{
    String episodeId;
    GameRuleset rules;
    bool inProgress = false;  ///< @c true= session is in progress / internal.save exists.

    de::Uri mapUri;
    duint mapEntryPoint = 0;  ///< Player entry point, for reborn.

    bool rememberVisitedMaps = false;
    QSet<de::Uri> visitedMaps;

    acs::System acscriptSys;  ///< The One acs::System instance.

    Instance(Public *i) : Base(i)
    {
        DENG2_ASSERT(singleton == nullptr);
        singleton = thisPublic;
    }

    inline String userSavePath(String const &fileName) {
        return Session::savePath() / fileName + ".save";
    }

    void cleanupInternalSave()
    {
        // Ensure the internal save folder exists.
        App::fileSystem().makeFolder(internalSavePath.fileNamePath());

        // Ensure that any pre-existing internal save is destroyed.
        // This may happen if the game was not shutdown properly in the event of a crash.
        /// @todo It may be possible to recover this session if it was written successfully
        /// before the fatal error occurred.
        Session::removeSaved(internalSavePath);
    }

    void resetStateForNewSession()
    {
        // Perform necessary prep.
        cleanupInternalSave();

        G_StopDemo();

        // Close the menu if open.
        Hu_MenuCommand(MCMD_CLOSEFAST);

        // If there are any InFine scripts running, they must be stopped.
        FI_StackClear();

        // Ignore a game action possibly set by script stop hooks; this is a completely new session.
        G_SetGameAction(GA_NONE);

        if(!IS_CLIENT)
        {
            for(player_t &plr : players)
            {
                if(!plr.plr->inGame) continue;

                // Force players to be initialized upon first map load.
                plr.playerState = PST_REBORN;
#if __JHEXEN__
                plr.worldTimer  = 0;
#else
                plr.didSecret   = false;
#endif
            }
        }

        M_ResetRandom();
    }

    void setEpisode(String const &newEpisodeId)
    {
        DENG2_ASSERT(!inProgress);

        episodeId = newEpisodeId;

        // Update the game status cvar.
        Con_SetString2("map-episode", episodeId.toUtf8().constData(), SVF_WRITE_OVERRIDE);
    }

    /**
     * Returns SessionMetadata for the game configuration in progress.
     */
    SessionMetadata metadata()
    {
        DENG2_ASSERT(inProgress);

        SessionMetadata meta;

        meta.set("sessionId",       duint(Timer_RealMilliseconds() + (mapTime << 24)));
        meta.set("gameIdentityKey", Session::gameId());
        meta.set("episode",         episodeId);
        meta.set("userDescription", "(Unsaved)");
        meta.set("mapUri",          mapUri.compose());
        meta.set("mapTime",         ::mapTime);

        meta.add("gameRules",       self.rules().toRecord());  // Takes ownership.

        ArrayValue *playersArray = new ArrayValue;
        for(player_t &plr : players)
        {
            *playersArray << NumberValue(CPP_BOOL(plr.plr->inGame), NumberValue::Boolean);
        }
        meta.set("players", playersArray);  // Takes ownership.

        if(rememberVisitedMaps)
        {
            ArrayValue *visitedMapsArray = new ArrayValue;
            for(de::Uri const &visitedMap : visitedMaps)
            {
                *visitedMapsArray << TextValue(visitedMap.compose());
            }
            meta.set("visitedMaps", visitedMapsArray); // Takes ownership.
        }

        return meta;
    }

    /**
     * Update/create a new SavedSession at the specified @a path from the current
     * game state.
     */
    SavedSession &updateSavedSession(String const &path, SessionMetadata const &metadata)
    {
        DENG2_ASSERT(inProgress);

        LOG_AS("GameSession");
        LOG_RES_VERBOSE("Serializing to \"%s\"...") << path;

        // Does the .save already exist?
        auto *saved = App::rootFolder().tryLocate<SavedSession>(path);
        if(saved)
        {
            DENG2_ASSERT(saved->mode().testFlag(File::Write));
            saved->replaceFile("Info") << composeSaveInfo(metadata).toUtf8();
        }
        else
        {
            // Create an empty package containing only the metadata.
            File &save = App::rootFolder().replaceFile(path);
            ZipArchive arch;
            arch.add("Info", composeSaveInfo(metadata).toUtf8());
            de::Writer(save) << arch;
            save.flush();

            // We can now reinterpret and populate the contents of the archive.
            saved = &save.reinterpret()->as<SavedSession>();
            saved->populate();
        }

        // Save the current game state to the .save package.
#if __JHEXEN__
        de::Writer(saved->replaceFile("ACScriptState")).withHeader()
                << acscriptSys.serializeWorldState();
#endif

        Folder &mapsFolder = App::fileSystem().makeFolder(saved->path() / "maps");
        DENG2_ASSERT(mapsFolder.mode().testFlag(File::Write));

        mapsFolder.replaceFile(mapUri.path() + "State")
                << serializeCurrentMapState();

        saved->flush();  // No need to populate; FS2 Files already in sync with source data.
        saved->cacheMetadata(metadata);  // Avoid immediately reopening the .save package.

        return *saved;
    }

#if __JDOOM__ || __JDOOM64__
    /**
     * @todo fixme: (Kludge) Assumes the original mobj info tic timing values have
     * not been modified!
     */
    void applyRuleFastMonsters(bool fast)
    {
        static bool oldFast = false;

        // Only modify when the rule changes state.
        if(fast == oldFast) return;
        oldFast = fast;

        for(dint i = S_SARG_RUN1; i <= S_SARG_RUN8; ++i)
        {
            STATES[i].tics = fast? 1 : 2;
        }
        for(dint i = S_SARG_ATK1; i <= S_SARG_ATK3; ++i)
        {
            STATES[i].tics = fast? 4 : 8;
        }
        for(dint i = S_SARG_PAIN; i <= S_SARG_PAIN2; ++i)
        {
            STATES[i].tics = fast? 1 : 2;
        }
    }
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    /**
     * @todo fixme: (Kludge) Assumes the original mobj info speed values have
     * not been modified!
     */
    void applyRuleFastMissiles(bool fast)
    {
        struct MissileData { mobjtype_t type; dfloat speed[2]; } const missileData[] =
        {
#if __JDOOM__ || __JDOOM64__
            { MT_BRUISERSHOT,       { 15, 20 } },
            { MT_HEADSHOT,          { 10, 20 } },
            { MT_TROOPSHOT,         { 10, 20 } },
# if __JDOOM64__
            { MT_BRUISERSHOTRED,    { 15, 20 } },
            { MT_NTROSHOT,          { 20, 40 } },
# endif
#elif __JHERETIC__
            { MT_IMPBALL,           { 10, 20 } },
            { MT_MUMMYFX1,          {  9, 18 } },
            { MT_KNIGHTAXE,         {  9, 18 } },
            { MT_REDAXE,            {  9, 18 } },
            { MT_BEASTBALL,         { 12, 20 } },
            { MT_WIZFX1,            { 18, 24 } },
            { MT_SNAKEPRO_A,        { 14, 20 } },
            { MT_SNAKEPRO_B,        { 14, 20 } },
            { MT_HEADFX1,           { 13, 20 } },
            { MT_HEADFX3,           { 10, 18 } },
            { MT_MNTRFX1,           { 20, 26 } },
            { MT_MNTRFX2,           { 14, 20 } },
            { MT_SRCRFX1,           { 20, 28 } },
            { MT_SOR2FX1,           { 20, 28 } },
#endif
        };
        static bool oldFast = false;

        // Only modify when the rule changes state.
        if(fast == oldFast) return;
        oldFast = fast;

        for(MissileData const &mdata : missileData)
        {
            MOBJINFO[mdata.type].speed = mdata.speed[dint( fast )];
        }
    }
#endif

    void applyCurrentRules()
    {
        if(rules.skill < SM_NOTHINGS)
            rules.skill = SM_NOTHINGS;
        if(rules.skill > NUM_SKILL_MODES - 1)
            rules.skill = skillmode_t( NUM_SKILL_MODES - 1 );

        if(!IS_NETGAME)
        {
#if !__JHEXEN__
            rules.deathmatch      = false;
            rules.respawnMonsters = dbyte( App::commandLine().has("-respawn") );

            rules.noMonsters      = dbyte( App::commandLine().has("-nomonsters") );
#endif
#if __JDOOM__ || __JHERETIC__
            // Is respawning enabled at all in nightmare skill?
            if(rules.skill == SM_NIGHTMARE)
            {
                rules.respawnMonsters = cfg.respawnMonstersNightmare;
            }
#endif
        }
        else if(IS_DEDICATED)
        {
#if !__JHEXEN__
            rules.deathmatch      = cfg.common.netDeathmatch;
            rules.respawnMonsters = cfg.netRespawn;

            rules.noMonsters      = cfg.common.netNoMonsters;
            /*rules.*/cfg.common.jumpEnabled = cfg.common.netJumping;
#else
            rules.randomClasses   = cfg.netRandomClass;
#endif
        }

        // Fast monsters?
#if __JDOOM__ || __JDOOM64__
        bool fastMonsters = CPP_BOOL(rules.fast);
# if __JDOOM__
        if(rules.skill == SM_NIGHTMARE)
        {
            fastMonsters = true;
        }
# endif
        applyRuleFastMonsters(fastMonsters);
#endif

        // Fast missiles?
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        bool fastMissiles = CPP_BOOL(rules.fast);
# if !__JDOOM64__
        if(rules.skill == SM_NIGHTMARE)
        {
            fastMissiles = true;
        }
# endif
        applyRuleFastMissiles(fastMissiles);
#endif

        NetSv_UpdateGameConfigDescription();

        // Update game status cvars:
        Con_SetInteger2("game-skill", rules.skill, SVF_WRITE_OVERRIDE);
    }

    /**
     * Constructs a MapStateReader for serialized map state format interpretation.
     */
    SavedSession::MapStateReader *makeMapStateReader(
        SavedSession const &session, String const &mapUriAsText)
    {
        de::Uri const mapUri(mapUriAsText, RC_NULL);
        auto const &mapStateFile = session.locateState<File const>(String("maps") / mapUri.path());
        if(!SV_OpenFileForRead(mapStateFile))
        {
            /// @throw Error The serialized map state file could not be opened for read.
            throw Error("GameSession::makeMapStateReader", "Failed to open \"" + mapStateFile.path() + "\" for read");
        }

        std::unique_ptr<SavedSession::MapStateReader> p;
        reader_s *reader = SV_NewReader();
        dint const magic = Reader_ReadInt32(reader);
        if(magic == MY_SAVE_MAGIC || MY_CLIENT_SAVE_MAGIC)  // Native format.
        {
            p.reset(new MapStateReader(session));
        }
#if __JDOOM__
        else if(magic == 0x1DEAD600)  // DoomV9
        {
            p.reset(new DoomV9MapStateReader(session));
        }
#endif
#if __JHERETIC__
        else if(magic == 0x7D9A1200)  // HereticV13
        {
            p.reset(new HereticV13MapStateReader(session));
        }
#endif
        SV_CloseFile();
        if(p.get())
        {
            return p.release();
        }

        /// @throw Error The format of the serialized map state was not recognized.
        throw Error("GameSession::makeMapStateReader", "Unrecognized map state format");
    }

    void loadSaved(String const &savePath)
    {
        ::briefDisabled = true;

        G_StopDemo();
        Hu_MenuCommand(MCMD_CLOSEFAST);
        FI_StackClear(); // Stop any running InFine scripts.

        M_ResetRandom();
        if(!IS_CLIENT)
        {
            for(player_t &plr : players)
            {
                if(!plr.plr->inGame) continue;

                // Force players to be initialized upon first map load.
                plr.playerState = PST_REBORN;
#if __JHEXEN__
                plr.worldTimer  = 0;
#else
                plr.didSecret   = false;
#endif
            }
        }

        inProgress = false;

        if(savePath.compareWithoutCase(internalSavePath))
        {
            // Perform necessary prep.
            cleanupInternalSave();

            // Copy the save to the internal savegame.
            Session::copySaved(internalSavePath, savePath);
        }

        //
        // SavedSession deserialization begins.
        //
        auto const &saved = App::rootFolder().locate<SavedSession>(internalSavePath);
        SessionMetadata const &metadata = saved.metadata();

        // Ensure a complete game ruleset is available.
        std::unique_ptr<GameRuleset> newRules;
        try
        {
            newRules.reset(GameRuleset::fromRecord(metadata.subrecord("gameRules")));
        }
        catch(Record::NotFoundError const &)
        {
            // The game rules are incomplete. Likely because they were missing from a savegame that
            // was converted from a vanilla format (in which most of these values are omitted).
            // Therefore we must assume the user has correctly configured the session accordingly.
            LOG_WARNING("Using current game rules as basis for loading savegame \"%s\"."
                        " (The original save format omits this information).")
                    << saved.path();

            // Use the current rules as our basis.
            newRules.reset(GameRuleset::fromRecord(metadata.subrecord("gameRules"), &rules));
        }
        rules = *newRules; // make a copy
        applyCurrentRules();
        setEpisode(metadata.gets("episode"));

        // Does the metadata include a visited maps breakdown?
        visitedMaps.clear();
        rememberVisitedMaps = metadata.has("visitedMaps");
        if(rememberVisitedMaps)
        {
            ArrayValue const &vistedMapsArray = metadata.geta("visitedMaps");
            for(Value const *value : vistedMapsArray.elements())
            {
                visitedMaps << de::Uri(value->as<TextValue>(), RC_NULL);
            }
        }

#if __JHEXEN__
        // Deserialize the world ACS state.
        if(File const *state = saved.tryLocateStateFile("ACScript"))
        {
            acscriptSys.readWorldState(de::Reader(*state).withHeader());
        }
#endif

        inProgress = true;

        setMap(de::Uri(metadata.gets("mapUri"), RC_NULL));
        //mapEntryPoint = ??; // not saved??

        reloadMap();
#if !__JHEXEN__
        ::mapTime = metadata.geti("mapTime");
#endif

        String const mapUriAsText = mapUri.compose();
        makeMapStateReader(saved, mapUriAsText)->read(mapUriAsText);
    }

    void setMap(de::Uri const &newMapUri)
    {
        DENG2_ASSERT(inProgress);

        mapUri = newMapUri;
        if(rememberVisitedMaps)
        {
            visitedMaps << mapUri;
        }

        // Update game status cvars:
        Con_SetUri2("map-id", reinterpret_cast<uri_s const *>(&mapUri), SVF_WRITE_OVERRIDE);

        String hubId;
        if(Record const *hubRec = defn::Episode(*self.episodeDef()).tryFindHubByMapId(mapUri.compose()))
        {
            hubId = hubRec->gets("id");
        }
        Con_SetString2("map-hub", hubId.toUtf8().constData(), SVF_WRITE_OVERRIDE);

        String mapAuthor = G_MapAuthor(mapUri);
        if(mapAuthor.isEmpty()) mapAuthor = "Unknown";
        Con_SetString2("map-author", mapAuthor.toUtf8().constData(), SVF_WRITE_OVERRIDE);

        String mapTitle = G_MapTitle(mapUri);
        if(mapTitle.isEmpty()) mapTitle = "Unknown";
        Con_SetString2("map-name", mapTitle.toUtf8().constData(), SVF_WRITE_OVERRIDE);
    }

    inline void setMapAndEntryPoint(de::Uri const &newMapUri, duint newMapEntryPoint)
    {
        setMap(newMapUri);
        mapEntryPoint = newMapEntryPoint;
    }

    /**
     * Reload the @em current map.
     *
     * @param revisit  @c true= load progress in this map from a previous visit in the current
     * game session. If no saved progress exists then the map will be in the default state.
     */
    void reloadMap(bool revisit = false)
    {
        DENG2_ASSERT(inProgress);

        Pause_End();

        // Close open HUDs.
        for(dint i = 0; i < MAXPLAYERS; ++i)
        {
            ST_CloseAll(i, true/*fast*/);
        }

        // Delete raw images to conserve texture memory.
        DD_Executef(true, "texreset raw");

        // Are we playing a briefing? (No, if we've already visited this map).
        if(revisit)
        {
            ::briefDisabled = true;
        }
        Record const *briefing = finaleBriefing(mapUri);

        // Restart the map music?
        if(!briefing)
        {
            S_MapMusic(mapUri);
            S_PauseMusic(true);
        }

        P_SetupMap(mapUri);

        if(revisit)
        {
            // We've been here before; deserialize the saved map state.
#if __JHEXEN__
            targetPlayerAddrs = nullptr; // player mobj redirection...
#endif

            String const mapUriAsText = mapUri.compose();
            auto const &saved = App::rootFolder().locate<SavedSession>(internalSavePath);
            std::unique_ptr<SavedSession::MapStateReader> reader(makeMapStateReader(saved, mapUriAsText));
            reader->read(mapUriAsText);
        }

        if(!briefing || !G_StartFinale(briefing->gets("script").toUtf8().constData(), 0, FIMODE_BEFORE, 0))
        {
            // No briefing; begin the map.
            HU_WakeWidgets(-1/* all players */);
            G_BeginMap();
        }

        Z_CheckHeap();
    }

#if __JHEXEN__
    struct playerbackup_t
    {
        player_t player;
        duint numInventoryItems[NUM_INVENTORYITEM_TYPES];
        inventoryitemtype_t readyItem;
    };

    void backupPlayersInHub(playerbackup_t playerBackup[MAXPLAYERS])
    {
        DENG2_ASSERT(playerBackup);

        for(dint i = 0; i < MAXPLAYERS; ++i)
        {
            playerbackup_t *pb  = &playerBackup[i];
            player_t const *plr = &players[i];

            std::memcpy(&pb->player, plr, sizeof(player_t));

            // Make a copy of the inventory states also.
            for(dint k = 0; k < NUM_INVENTORYITEM_TYPES; ++k)
            {
                pb->numInventoryItems[k] = P_InventoryCount(i, inventoryitemtype_t(k));
            }
            pb->readyItem = P_InventoryReadyItem(i);
        }
    }

    /**
     * @param playerBackup  Player state backup.
     */
    void restorePlayersInHub(playerbackup_t playerBackup[MAXPLAYERS])
    {
        DENG2_ASSERT(playerBackup);

        mobj_t *targetPlayerMobj = nullptr;

        for(dint i = 0; i < MAXPLAYERS; ++i)
        {
            playerbackup_t *pb = &playerBackup[i];
            player_t *plr      = &players[i];
            ddplayer_t *ddplr  = plr->plr;
            dint oldKeys = 0, oldPieces = 0;
            dd_bool oldWeaponOwned[NUM_WEAPON_TYPES];

            if(!ddplr->inGame) continue;

            std::memcpy(plr, &pb->player, sizeof(player_t));

            // Reset the inventory as it will now be restored from the backup.
            P_InventoryEmpty(i);

            for(dint k = 0; k < NUM_INVENTORYITEM_TYPES; ++k)
            {
                // Don't give back the wings of wrath if reborn.
                if(k == IIT_FLY && plr->playerState == PST_REBORN)
                    continue;

                for(duint m = 0; m < pb->numInventoryItems[k]; ++m)
                {
                    P_InventoryGive(i, inventoryitemtype_t(k), true);
                }
            }
            P_InventorySetReadyItem(i, pb->readyItem);

            ST_LogEmpty(i);
            plr->attacker = nullptr;
            plr->poisoner = nullptr;

            if(IS_NETGAME || rules.deathmatch)
            {
                // In a network game, force all players to be alive
                if(plr->playerState == PST_DEAD)
                {
                    plr->playerState = PST_REBORN;
                }

                if(!rules.deathmatch)
                {
                    // Cooperative net-play; retain keys and weapons.
                    oldKeys   = plr->keys;
                    oldPieces = plr->pieces;

                    for(dint k = 0; k < NUM_WEAPON_TYPES; ++k)
                    {
                        oldWeaponOwned[k] = plr->weapons[k].owned;
                    }
                }
            }

            bool wasReborn = (plr->playerState == PST_REBORN);

            if(rules.deathmatch)
            {
                de::zap(plr->frags);
                ddplr->mo = nullptr;
                G_DeathMatchSpawnPlayer(i);
            }
            else
            {
                if(playerstart_t const *start = P_GetPlayerStart(mapEntryPoint, i, false))
                {
                    mapspot_t const *spot = &mapSpots[start->spot];
                    P_SpawnPlayer(i, cfg.playerClass[i], spot->origin[0],
                                  spot->origin[1], spot->origin[2], spot->angle,
                                  spot->flags, false, true);
                }
                else
                {
                    P_SpawnPlayer(i, cfg.playerClass[i], 0, 0, 0, 0,
                                  MSF_Z_FLOOR, true, true);
                }
            }

            if(wasReborn && IS_NETGAME && !rules.deathmatch)
            {
                dint bestWeapon = 0;

                // Restore keys and weapons when reborn in co-op.
                plr->keys   = oldKeys;
                plr->pieces = oldPieces;

                for(dint k = 0; k < NUM_WEAPON_TYPES; ++k)
                {
                    if(oldWeaponOwned[k])
                    {
                        bestWeapon = k;
                        plr->weapons[k].owned = true;
                    }
                }

                plr->ammo[AT_BLUEMANA].owned  = 25;  /// @todo values.ded
                plr->ammo[AT_GREENMANA].owned = 25;  /// @todo values.ded

                // Bring up the best weapon.
                if(bestWeapon)
                {
                    plr->pendingWeapon = weapontype_t(bestWeapon);
                }
            }
        }

        for(player_t const &plr : players)
        {
            if(!plr.plr->inGame) continue;

            if(!targetPlayerMobj)
            {
                targetPlayerMobj = plr.plr->mo;
            }
        }

        // Redirect anything targeting a player mobj.
        /// @todo fixme: This only supports single player games!!
        if(targetPlayerAddrs)
        {
            for(targetplraddress_t *p = targetPlayerAddrs; p; p = p->next)
            {
                *(p->address) = targetPlayerMobj;
            }

            SV_ClearTargetPlayers();

            /*
             * When XG is available in Hexen, call this after updating target player
             * references (after a load) - ds
            // The activator mobjs must be set.
            XL_UpdateActivators();
            */
        }

        // Destroy all things touching players.
        P_TelefragMobjsTouchingPlayers();
    }
#endif // __JHEXEN__
};

GameSession::GameSession() : d(new Instance(this))
{}

GameSession::~GameSession()
{
    LOG_AS("~GameSession");
    d.reset();
    singleton = nullptr;
}

GameSession &GameSession::gameSession()
{
    DENG2_ASSERT(singleton);
    return *singleton;
}

bool GameSession::hasBegun() const
{
    return d->inProgress;
}

bool GameSession::loadingPossible()
{
    return !(IS_CLIENT && !Get(DD_PLAYBACK));
}

bool GameSession::savingPossible()
{
    if(IS_CLIENT || Get(DD_PLAYBACK)) return false;

    if(!hasBegun()) return false;
    if(GS_MAP != G_GameState()) return false;

    /// @todo fixme: What about splitscreen!
    player_t *player = &players[CONSOLEPLAYER];
    if(PST_DEAD == player->playerState) return false;

    return true;
}

Record const *GameSession::episodeDef() const
{
    if(hasBegun())
    {
        /// @todo cache this result?
        return Defs().episodes.tryFind("id", d->episodeId);
    }
    return nullptr;
}

String GameSession::episodeId() const
{
    return hasBegun()? d->episodeId : "";
}

Record const *GameSession::mapGraphNodeDef() const
{
    if(Record const *episode = episodeDef())
    {
        return defn::Episode(*episode).tryFindMapGraphNode(mapUri().compose());
    }
    return nullptr;
}

Record const &GameSession::mapInfo() const
{
    return G_MapInfoForMapUri(mapUri());
}

de::Uri GameSession::mapUri() const
{
    return hasBegun()? d->mapUri : de::Uri("Maps:", RC_NULL);
}

uint GameSession::mapEntryPoint() const
{
    return d->mapEntryPoint;
}

GameSession::VisitedMaps GameSession::allVisitedMaps() const
{
    if(hasBegun() && d->rememberVisitedMaps)
    {
        return d->visitedMaps.toList();
    }
    return VisitedMaps();
}

de::Uri GameSession::mapUriForNamedExit(String name) const
{
    LOG_AS("GameSession");
    if(Record const *mgNode = mapGraphNodeDef())
    {
        // Build a lookup table mapping exit ids to exit records.
        QMap<String, Record const *> exits;
        for(Value const *value : mgNode->geta("exit").elements())
        {
            Record const &exit = value->as<RecordValue>().dereference();
            String id = exit.gets("id");
            if(!id.isEmpty())
            {
                exits.insert(id, &exit);
            }
        }
        //qDebug() << "map exits" << exits;

        // Locate the named exit record.
        Record const *chosenExit = nullptr;
        if(exits.count() > 1)
        {
            auto found = exits.constFind(name.toLower());
            if(found != exits.constEnd())
            {
                chosenExit = found.value();
            }
            else
            {
                LOG_SCR_WARNING("Episode '%s' map \"%s\" defines no Exit with ID '%s'")
                        << d->episodeId << d->mapUri << name;
            }
        }
        else if(exits.count() == 1)
        {
            chosenExit = exits.values().first();
            String chosenExitId = chosenExit->gets("id");
            if(chosenExitId != name.toLower())
            {
                LOGDEV_SCR_NOTE("Exit ID:%s chosen instead of '%s'")
                        << chosenExitId << name;
            }
        }

        if(chosenExit)
        {
            return de::Uri(chosenExit->gets("targetMap"), RC_NULL);
        }
    }
    return de::Uri();
}

GameRuleset const &GameSession::rules() const
{
    return d->rules;
}

void GameSession::applyNewRules(GameRuleset const &newRules)
{
    LOG_AS("GameSession");

    d->rules = newRules;
    if(hasBegun())
    {
        d->applyCurrentRules();
        LOGDEV_WARNING("Applied new rules while in progress!");
    } // Otherwise deferred
}

bool GameSession::progressRestoredOnReload() const
{
    if(d->rules.deathmatch) return false; // Never.
#if __JHEXEN__
    return true; // Cannot be disabled.
#else
    return cfg.common.loadLastSaveOnReborn;
#endif
}

void GameSession::end()
{
    if(!hasBegun()) return;

    // Reset state of relevant subsystems.
#if __JHEXEN__
    d->acscriptSys.reset();
#endif
    if(!IS_DEDICATED)
    {
        G_ResetViewEffects();
    }

    Session::removeSaved(internalSavePath);

    d->inProgress = false;
    LOG_MSG("Game ended");
}

void GameSession::endAndBeginTitle()
{
    end();

    if(Record const *finale = Defs().finales.tryFind("id", "title"))
    {
        G_StartFinale(finale->gets("script").toUtf8().constData(), FF_LOCAL, FIMODE_NORMAL, "title");
        return;
    }
    /// @throw Error A title script must always be defined.
    throw Error("GameSession::endAndBeginTitle", "An InFine 'title' script must be defined");
}

void GameSession::begin(GameRuleset const &newRules, String const &episodeId,
    de::Uri const &mapUri, uint mapEntryPoint)
{
    if(hasBegun())
    {
        /// @throw InProgressError Cannot begin a new session before the current session has ended.
        throw InProgressError("GameSession::begin", "The game session has already begun");
    }

    // Ensure the episode id is good.
    if(!Defs().episodes.has("id", episodeId))
    {
        throw Error("GameSession::begin", "Episode '" + episodeId + "' is not known");
    }

    // Ensure the map truly exists.
    if(!P_MapExists(mapUri.compose().toUtf8().constData()))
    {
        throw Error("GameSession::begin", "Map \"" + mapUri.asText() + "\" does not exist");
    }

    LOG_MSG("Game begins...");

    d->resetStateForNewSession();

    // Configure the new session.
    d->rules = newRules; // make a copy
    d->applyCurrentRules();
    d->setEpisode(episodeId);
    d->visitedMaps.clear();
    d->rememberVisitedMaps = true;

    // Begin the session.
    d->inProgress = true;
    d->setMapAndEntryPoint(mapUri, mapEntryPoint);

    SessionMetadata metadata = d->metadata();

    // Print a session banner to the log.
    LOG_MSG(DE2_ESC(R));
    LOG_NOTE("Episode: " DE2_ESC(i) DE2_ESC(b) "%s" DE2_ESC(.) " (%s)")
            << G_EpisodeTitle(episodeId)
            << d->rules.description();
    LOG_VERBOSE("%s") << metadata.asStyledText();
    LOG_MSG(DE2_ESC(R));

    // Load the start map.
    d->reloadMap();

    // Create the internal .save session package.
    d->updateSavedSession(internalSavePath, metadata);
}

void GameSession::reloadMap()
{
    if(!hasBegun())
    {
        /// @throw InProgressError Cannot load a map unless the game is in progress.
        throw InProgressError("GameSession::reloadMap", "No game session is in progress");
    }

    if(progressRestoredOnReload())
    {
        try
        {
            d->loadSaved(internalSavePath);
            return;
        }
        catch(Error const &er)
        {
            LOG_AS("GameSession");
            LOG_WARNING("Error loading current map state:\n") << er.asText();
        }
        // If we ever reach here then there is an insurmountable problem...
        endAndBeginTitle();
    }
    else
    {
        // Restart the session entirely.
        bool oldBriefDisabled = ::briefDisabled;

        ::briefDisabled = true; // We won't brief again.

        end();
        d->resetStateForNewSession();

        // Begin the session.
        d->inProgress = true;
        d->reloadMap();

        // Create the internal .save session package.
        d->updateSavedSession(internalSavePath, d->metadata());

        ::briefDisabled = oldBriefDisabled;
    }
}

void GameSession::leaveMap(de::Uri const &nextMapUri, uint nextMapEntryPoint)
{
    if(!hasBegun())
    {
        /// @throw InProgressError Cannot leave a map unless the game is in progress.
        throw InProgressError("GameSession::leaveMap", "No game session is in progress");
    }

    // Ensure the map truly exists.
    if(!P_MapExists(nextMapUri.compose().toUtf8().constData()))
    {
        throw Error("GameSession::leaveMap", "Map \"" + nextMapUri.asText() + "\" does not exist");
    }

    // If there are any InFine scripts running, they must be stopped.
    FI_StackClear();

#if __JHEXEN__
    // Take a copy of the player objects (they will be cleared in the process
    // of calling @ref P_SetupMap() and we need to restore them after).
    Instance::playerbackup_t playerBackup[MAXPLAYERS];
    d->backupPlayersInHub(playerBackup);

    // Disable class randomization (all players must spawn as their existing class).
    dbyte oldRandomClassesRule = d->rules.randomClasses;
    d->rules.randomClasses = false;
#endif

    // Are we saving progress?
    SavedSession *saved = nullptr;
    if(!d->rules.deathmatch) // Never save in deathmatch.
    {
        saved = &App::rootFolder().locate<SavedSession>(internalSavePath);
        auto &mapsFolder = saved->locate<Folder>("maps");

        DENG2_ASSERT(saved->mode().testFlag(File::Write));
        DENG2_ASSERT(mapsFolder.mode().testFlag(File::Write));

        // Are we entering a new hub?
#if __JHEXEN__
        defn::Episode epsd(*episodeDef());
        Record const *currentHub = epsd.tryFindHubByMapId(d->mapUri.compose());
        if(!currentHub || currentHub != epsd.tryFindHubByMapId(nextMapUri.compose()))
#endif
        {
            // Clear all saved map states in the current hub.
            Folder::Contents contents = mapsFolder.contents();
            DENG2_FOR_EACH_CONST(Folder::Contents, i, contents)
            {
                mapsFolder.removeFile(i->first);
            }
        }
#if __JHEXEN__
        else
        {
            File &outFile = mapsFolder.replaceFile(d->mapUri.path() + "State");
            outFile << serializeCurrentMapState(true /*exclude players*/);
            // We'll flush whole package soon.
        }
#endif
        // Ensure changes are written to disk right away (otherwise would stay
        // in memory only).
        saved->flush();
    }

#if __JHEXEN__
    /// @todo Necessary?
    if(!IS_CLIENT)
    {
        // Force players to be initialized upon first map load.
        for(player_t &plr : players)
        {
            if(plr.plr->inGame)
            {
                plr.playerState = PST_REBORN;
                plr.worldTimer = 0;
            }
        }
    }
    //<- todo end.

    // In Hexen the RNG is re-seeded each time the map changes.
    M_ResetRandom();
#endif

    // Change the current map.
    d->setMapAndEntryPoint(nextMapUri, nextMapEntryPoint);

    // Are we revisiting a previous map?
    bool const revisit = saved && saved->hasState(String("maps") / d->mapUri.path());

    d->reloadMap(revisit);

    // On exit logic:
#if __JHEXEN__
    if(!revisit)
    {
        // First visit; destroy all freshly spawned players (??).
        for(player_t &plr : players)
        {
            if(plr.plr->inGame)
            {
                P_MobjRemove(plr.plr->mo, true);
            }
        }
    }

    d->restorePlayersInHub(playerBackup);

    // Restore the random class rule.
    d->rules.randomClasses = oldRandomClassesRule;

    // Launch waiting scripts.
    d->acscriptSys.runDeferredTasks(d->mapUri);
#endif

    if(saved)
    {
        DENG2_ASSERT(saved->mode().testFlag(File::Write));

        SessionMetadata metadata = d->metadata();

        /// @todo Use the existing sessionId?
        //metadata.set("sessionId", saved->metadata().geti("sessionId"));
        saved->replaceFile("Info") << composeSaveInfo(metadata).toUtf8();

#if __JHEXEN__
        // Save the world-state of the Script interpreter.
        de::Writer(saved->replaceFile("ACScriptState")).withHeader()
                << d->acscriptSys.serializeWorldState();
#endif

        // Save the state of the current map.
        auto &mapsFolder = saved->locate<Folder>("maps");
        DENG2_ASSERT(mapsFolder.mode().testFlag(File::Write));

        File &outFile = mapsFolder.replaceFile(d->mapUri.path() + "State");
        outFile << serializeCurrentMapState();

        saved->flush(); // Write all changes to the package.
        saved->cacheMetadata(metadata); // Avoid immediately reopening the .save package.
    }
}

String GameSession::userDescription()
{
    if(!hasBegun()) return "";
    return App::rootFolder().locate<SavedSession>(internalSavePath)
                                .metadata().gets("userDescription", "");
}

static String chooseSaveDescription(String const &savePath, String const &userDescription)
{
    // Use the user description given.
    if(!userDescription.isEmpty())
    {
        return userDescription;
    }
    // We'll generate a suitable description automatically.
    return G_DefaultSavedSessionUserDescription(savePath.fileNameWithoutExtension());
}

void GameSession::save(String const &saveName, String const &userDescription)
{
    if(!hasBegun())
    {
        /// @throw InProgressError Cannot save unless the game is in progress.
        throw InProgressError("GameSession::save", "No game session is in progress");
    }

    String const savePath = d->userSavePath(saveName);
    LOG_MSG("Saving game to \"%s\"...") << savePath;

    try
    {
        // Compose the session metadata.
        SessionMetadata metadata = d->metadata();
        metadata.set("userDescription", chooseSaveDescription(savePath, userDescription));

        // Update the existing internal .save package.
        d->updateSavedSession(internalSavePath, metadata);

        // In networked games the server tells the clients to save also.
        NetSv_SaveGame(metadata.geti("sessionId"));

        // Copy the internal saved session to the destination slot.
        Session::copySaved(savePath, internalSavePath);

        P_SetMessage(&players[CONSOLEPLAYER], TXT_GAMESAVED);

        // Notify the engine that the game was saved.
        /// @todo After the engine has the primary responsibility of saving the game,
        /// this notification is unnecessary.
        Plug_Notify(DD_NOTIFY_GAME_SAVED, nullptr);
    }
    catch(Error const &er)
    {
        LOG_RES_WARNING("Error saving game session to '%s':\n")
                << savePath << er.asText();
    }
}

/// @todo Use busy mode here.
void GameSession::load(String const &saveName)
{
    String const savePath = d->userSavePath(saveName);
    LOG_MSG("Loading game from \"%s\"...") << savePath;
    d->loadSaved(savePath);
    P_SetMessage(&players[CONSOLEPLAYER], "Game loaded");
}

void GameSession::copySaved(String const &destName, String const &sourceName)
{
    Session::copySaved(d->userSavePath(destName), d->userSavePath(sourceName));
    LOG_MSG("Copied savegame \"%s\" to \"%s\"") << sourceName << destName;
}

void GameSession::removeSaved(String const &saveName)
{
    Session::removeSaved(d->userSavePath(saveName));
}

String GameSession::savedUserDescription(String const &saveName)
{
    String const savePath = d->userSavePath(saveName);
    if(auto const *saved = App::rootFolder().tryLocate<SavedSession>(savePath))
    {
        return saved->metadata().gets("userDescription", "");
    }
    return ""; // Not found.
}

acs::System &GameSession::acsSystem()
{
    return d->acscriptSys;
}

namespace {
dint gsvRuleSkill;
char *gsvEpisode = (char *)"";
uri_s *gsvMap;
char *gsvHub = (char *)"";
}

void GameSession::consoleRegister()  // static
{
#define READONLYCVAR  (CVF_READ_ONLY | CVF_NO_MAX | CVF_NO_MIN | CVF_NO_ARCHIVE)

    C_VAR_INT    ("game-skill",     &gsvRuleSkill,  READONLYCVAR, 0, 0);
    C_VAR_CHARPTR("map-episode",    &gsvEpisode,    READONLYCVAR, 0, 0);
    C_VAR_CHARPTR("map-hub",        &gsvHub,        READONLYCVAR, 0, 0);
    C_VAR_URIPTR ("map-id",         &gsvMap,        READONLYCVAR, 0, 0);

#undef READONLYCVAR
}

}  // namespace common
