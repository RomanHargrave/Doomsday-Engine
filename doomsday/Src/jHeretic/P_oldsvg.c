// Backwards compatible savegame reader.

#include "DoomDef.h"
#include "P_local.h"

#define VERSIONSIZE 16
#define SAVE_GAME_TERMINATOR 0x1d

byte *savebuffer, *save_p;

static byte SV_ReadByte()
{
	return *save_p++;
}

static short SV_ReadShort()
{
	save_p += 2;
	return *(short*) (save_p-2);
}

static long SV_ReadLong()
{
	save_p += 4;
	return *(int*) (save_p-4);
}

static void SV_Read(void *data, int len)
{
	memcpy(data, save_p, len);
	save_p += len;
}

/*// Writes out a backwards compatible version of player_t.
void SV_WritePlayer(player_t *pl)
{
	int temp[] = { 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef };
	ddplayer_t *ddpl = pl->plr;

	SV_WriteLong(0);					// mo
	SV_WriteLong(pl->playerstate);	
	SV_Write(temp, 10);					// ticcmd_t
	SV_WriteLong(ddpl->viewz);
	SV_WriteLong(pl->plr->viewheight);
	SV_WriteLong(pl->deltaviewheight);
	SV_WriteLong(pl->bob);
	SV_WriteLong(pl->flyheight);
	SV_WriteLong(ddpl->lookdir);
	SV_WriteLong(pl->centering);
	SV_WriteLong(pl->health);
	SV_WriteLong(pl->armorpoints);
	SV_WriteLong(pl->armortype);
	SV_Write(pl->inventory, 4 * 2*14);
	SV_WriteLong(pl->readyArtifact);
	SV_WriteLong(pl->artifactCount);
	SV_WriteLong(pl->inventorySlotNum);
	SV_Write(pl->powers, 4 * 9);
	SV_Write(pl->keys, 4 * 3);
	SV_WriteLong(pl->backpack);
	SV_Write(pl->frags, 4 * 4);
	SV_WriteLong(pl->readyweapon);
	SV_WriteLong(pl->pendingweapon);
	SV_Write(pl->weaponowned, 4 * 9);
	SV_Write(pl->ammo, 4 * 6);
	SV_Write(pl->maxammo, 4 * 6);
	SV_WriteLong(pl->attackdown);
	SV_WriteLong(pl->usedown);
	SV_WriteLong(pl->cheats);
	SV_WriteLong(pl->refire);
	SV_WriteLong(pl->killcount);
	SV_WriteLong(pl->itemcount);
	SV_WriteLong(pl->secretcount);
	SV_WriteLong(0);					// message, char*
	SV_WriteLong(pl->messageTics);
	SV_WriteLong(pl->damagecount);
	SV_WriteLong(pl->bonuscount);
	SV_WriteLong(pl->flamecount);
	SV_WriteLong(0);					// attacker
	SV_WriteLong(ddpl->extralight);
	SV_WriteLong(ddpl->fixedcolormap);
	SV_WriteLong(pl->colormap);
	SV_Write(pl->psprites, 16*2);
	SV_WriteLong(pl->didsecret);
	SV_WriteLong(pl->chickenTics);
	SV_WriteLong(pl->chickenPeck);
	SV_WriteLong(0);					// rain1
	SV_WriteLong(0);					// rain2
}*/

static void SV_ReadPlayer(player_t *pl)
{
	byte temp[12];
	ddplayer_t *ddpl = pl->plr;

	SV_ReadLong();						// mo
	pl->playerstate = SV_ReadLong();	
	SV_Read(temp, 10);					// ticcmd_t
	ddpl->viewz = SV_ReadLong();
	ddpl->viewheight = SV_ReadLong();
	ddpl->deltaviewheight = SV_ReadLong();
	pl->bob = SV_ReadLong();
	pl->flyheight = SV_ReadLong();
	ddpl->lookdir = SV_ReadLong();
	pl->centering = SV_ReadLong();
	pl->health = SV_ReadLong();
	pl->armorpoints = SV_ReadLong();
	pl->armortype = SV_ReadLong();
	SV_Read(pl->inventory, 4 * 2 * 14);
	pl->readyArtifact = SV_ReadLong();
	pl->artifactCount = SV_ReadLong();
	pl->inventorySlotNum = SV_ReadLong();
	SV_Read(pl->powers, 4 * 9);
	SV_Read(pl->keys, 4 * 3);
	pl->backpack = SV_ReadLong();
	SV_Read(pl->frags, 4 * 4);
	pl->readyweapon = SV_ReadLong();
	pl->pendingweapon = SV_ReadLong();
	SV_Read(pl->weaponowned, 4 * 9);
	SV_Read(pl->ammo, 4 * 6);
	SV_Read(pl->maxammo, 4 * 6);
	pl->attackdown = SV_ReadLong();
	pl->usedown = SV_ReadLong();
	pl->cheats = SV_ReadLong();
	pl->refire = SV_ReadLong();
	pl->killcount = SV_ReadLong();
	pl->itemcount = SV_ReadLong();
	pl->secretcount = SV_ReadLong();
	SV_ReadLong();					// message, char*
	pl->messageTics = SV_ReadLong();
	pl->damagecount = SV_ReadLong();
	pl->bonuscount = SV_ReadLong();
	pl->flamecount = SV_ReadLong();
	SV_ReadLong();					// attacker
	ddpl->extralight = SV_ReadLong();
	ddpl->fixedcolormap = SV_ReadLong();
	pl->colormap = SV_ReadLong();
	SV_Read(pl->psprites, 16*2);
	pl->didsecret = SV_ReadLong();
	pl->chickenTics = SV_ReadLong();
	pl->chickenPeck = SV_ReadLong();
	SV_ReadLong();					// rain1
	SV_ReadLong();					// rain2
}

/*void SV_WriteMobj(mobj_t *mo)
{
	// The thinker is 3 ints long.
	SV_WriteLong(0);
	SV_WriteLong(0);
	SV_WriteLong(0);
	
	SV_WriteLong(mo->x);
	SV_WriteLong(mo->y);
	SV_WriteLong(mo->z);

	// Sector links.
	SV_WriteLong(0);
	SV_WriteLong(0);

	SV_WriteLong(mo->angle);
	SV_WriteLong(mo->sprite);
	SV_WriteLong(mo->frame);

	// Block links.
	SV_WriteLong(0);
	SV_WriteLong(0);

	// Subsector.
	SV_WriteLong(0);

	SV_WriteLong(mo->floorz);
	SV_WriteLong(mo->ceilingz);
	SV_WriteLong(mo->radius);
	SV_WriteLong(mo->height);
	SV_WriteLong(mo->momx);
	SV_WriteLong(mo->momy);
	SV_WriteLong(mo->momz);

	SV_WriteLong(mo->validcount);

	SV_WriteLong(mo->type);
	SV_WriteLong(0);	// info
	SV_WriteLong(mo->tics);
	SV_WriteLong((int)mo->state);	
	SV_WriteLong(mo->damage);
	SV_WriteLong(mo->flags);
	SV_WriteLong(mo->flags2);
	SV_WriteLong(mo->special1);
	SV_WriteLong(mo->special2);
	SV_WriteLong(mo->health);
	SV_WriteLong(mo->movedir);
	SV_WriteLong(mo->movecount);
	SV_WriteLong(0);	// target
	SV_WriteLong(mo->reactiontime);
	SV_WriteLong(mo->threshold);
	SV_WriteLong((int)mo->player);
	SV_WriteLong(mo->lastlook);
	SV_Write(&mo->spawnpoint, 10);
}*/

static void SV_ReadMobj(mobj_t *mo)
{
	// Clear everything first.
	memset(mo, 0, sizeof(*mo));

	// The thinker is 3 ints long.
	SV_ReadLong();
	SV_ReadLong();
	SV_ReadLong();
	
	mo->x = SV_ReadLong();
	mo->y = SV_ReadLong();
	mo->z = SV_ReadLong();

	// Sector links.
	SV_ReadLong();
	SV_ReadLong();

	mo->angle = SV_ReadLong();
	mo->sprite = SV_ReadLong();
	mo->frame = SV_ReadLong();

	// Block links.
	SV_ReadLong();
	SV_ReadLong();

	// Subsector.
	SV_ReadLong();

	mo->floorz = SV_ReadLong();
	mo->ceilingz = SV_ReadLong();
	mo->radius = SV_ReadLong();
	mo->height = SV_ReadLong();
	mo->momx = SV_ReadLong();
	mo->momy = SV_ReadLong();
	mo->momz = SV_ReadLong();

	mo->valid = SV_ReadLong();

	mo->type = SV_ReadLong();
	SV_ReadLong();	// info
	mo->tics = SV_ReadLong();
	mo->state = (state_t*) SV_ReadLong();	
	mo->damage = SV_ReadLong();
	mo->flags = SV_ReadLong();
	mo->flags2 = SV_ReadLong();
	mo->special1 = SV_ReadLong();
	mo->special2 = SV_ReadLong();
	mo->health = SV_ReadLong();
	mo->movedir = SV_ReadLong();
	mo->movecount = SV_ReadLong();
	SV_ReadLong();	// target
	mo->reactiontime = SV_ReadLong();
	mo->threshold = SV_ReadLong();
	mo->player = (player_t*) SV_ReadLong();
	mo->lastlook = SV_ReadLong();
	SV_Read(&mo->spawnpoint, 10);
}

/*
====================
=
= P_UnArchivePlayers
=
====================
*/

void P_v13_UnArchivePlayers (void)
{
	int				i,j;

	for (i=0 ; i<4; i++)
	{
		if (!players[i].plr->ingame)
			continue;
		//SV_PlayerConverter(players+i, (saveplayer_t*) save_p, false);
		//save_p += sizeof(saveplayer_t);
		SV_ReadPlayer(players+i);
		players[i].plr->mo = NULL;		// will be set when unarc thinker
		players[i].message = NULL;
		players[i].attacker = NULL;
		for (j=0 ; j<NUMPSPRITES ; j++)
			if (players[i].psprites[j].state)
				players[i].psprites[j].state = &states[ (int)players[i].psprites[j].state ];
	}
}

//=============================================================================


/*
====================
=
= P_UnArchiveWorld
=
====================
*/

void P_v13_UnArchiveWorld (void)
{
	int			i,j;
	sector_t	*sec;
	line_t		*li;
	side_t		*si;
	short		*get;
	int			firstflat = W_CheckNumForName("F_START") + 1;
	
	get = (short *)save_p;
		
//
// do sectors
//
	for (i=0, sec = sectors ; i<numsectors ; i++,sec++)
	{
		sec->floorheight = *get++ << FRACBITS;
		sec->ceilingheight = *get++ << FRACBITS;
		sec->floorpic = *get++ + firstflat;
		sec->ceilingpic = *get++ + firstflat;
		sec->lightlevel = *get++;
		sec->special = *get++;	// needed?
		sec->tag = *get++;		// needed?
		sec->specialdata = 0;
		sec->soundtarget = 0;
	}	

//
// do lines
//
	for (i=0, li = lines ; i<numlines ; i++,li++)
	{
		li->flags = *get++;
		li->special = *get++;
		li->tag = *get++;
		for (j=0 ; j<2 ; j++)
		{
			if (li->sidenum[j] == -1)
				continue;
			si = &sides[li->sidenum[j]];
			si->textureoffset = *get++ << FRACBITS;
			si->rowoffset = *get++ << FRACBITS;
			si->toptexture = *get++;
			si->bottomtexture = *get++;
			si->midtexture = *get++;
		}
	}
		
	save_p = (byte *)get;	
}

//=============================================================================

typedef enum
{
	tc_end,
	tc_mobj
} thinkerclass_t;

/*
====================
=
= P_ArchiveThinkers
=
====================
*/

/*void P_ArchiveThinkers(void)
{
	thinker_t *th;
	mobj_t mobj;

	for(th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		if(th->function == P_MobjThinker)
		{
			SV_WriteByte(tc_mobj);
			//SV_MobjConverter( (mobj_t*) th, &mobj, true);
			memcpy(&mobj, th, sizeof(mobj));
			mobj.state = (state_t *)(mobj.state-states);
			if(mobj.player)
			{
				mobj.player = (player_t *)((mobj.player-players)+1);
			}
			//SV_Write(&mobj, sizeof(mobj));
			SV_WriteMobj(&mobj);
			continue;
		}
		//Con_Error("P_ArchiveThinkers: Unknown thinker function");
	}

	// Add a terminating marker
	SV_WriteByte(tc_end);
}*/

/*
====================
=
= P_UnArchiveThinkers
=
====================
*/

void P_v13_UnArchiveThinkers (void)
{
	byte		tclass;
	thinker_t	*currentthinker, *next;
	mobj_t		*mobj;

//
// remove all the current thinkers
//
	currentthinker = thinkercap.next;
	while (currentthinker != &thinkercap)
	{
		next = currentthinker->next;
		if (currentthinker->function == P_MobjThinker)
			P_RemoveMobj ((mobj_t *)currentthinker);
		else
			Z_Free (currentthinker);
		currentthinker = next;
	}
	P_InitThinkers ();
	
// read in saved thinkers
	while (1)
	{
		tclass = *save_p++;
		switch (tclass)
		{
		case tc_end:
			return;			// end of list
			
		case tc_mobj:
			mobj = Z_Malloc (sizeof(*mobj), PU_LEVEL, NULL);
			//SV_MobjConverter(mobj, (savemobj_t*) save_p, false);
			//save_p += sizeof(savemobj_t);
			SV_ReadMobj(mobj);
			mobj->state = &states[(int)mobj->state];
			mobj->target = NULL;
			if (mobj->player)
			{
				mobj->player = &players[(int)mobj->player-1];
				mobj->player->plr->mo = mobj;
				mobj->player->plr->mo->dplayer = mobj->player->plr;
				mobj->player->plr->clAngle = mobj->angle;
				mobj->player->plr->clLookDir = mobj->player->plr->lookdir;
			}
			P_SetThingPosition (mobj);
			mobj->info = &mobjinfo[mobj->type];
			mobj->floorz = mobj->subsector->sector->floorheight;
			mobj->ceilingz = mobj->subsector->sector->ceilingheight;
			mobj->thinker.function = P_MobjThinker;
			P_AddThinker (&mobj->thinker);
			break;
			
		default:
			Con_Error ("Unknown tclass %i in savegame",tclass);
		}
	
	}

}

//=============================================================================


/*
====================
=
= P_ArchiveSpecials
=
====================
*/
enum
{
	tc_ceiling,
	tc_door,
	tc_floor,
	tc_plat,
	tc_flash,
	tc_strobe,
	tc_glow,
	tc_endspecials
} specials_e;	

#if 0
void P_ArchiveSpecials(void)
{
/*
T_MoveCeiling, (ceiling_t: sector_t * swizzle), - active list
T_VerticalDoor, (vldoor_t: sector_t * swizzle),
T_MoveFloor, (floormove_t: sector_t * swizzle),
T_LightFlash, (lightflash_t: sector_t * swizzle),
T_StrobeFlash, (strobe_t: sector_t *),
T_Glow, (glow_t: sector_t *),
T_PlatRaise, (plat_t: sector_t *), - active list
*/

	thinker_t *th;
	ceiling_t ceiling;
	vldoor_t door;
	floormove_t floor;
	plat_t plat;
	lightflash_t flash;
	strobe_t strobe;
	glow_t glow;

	for(th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		if(th->function == T_MoveCeiling)
		{
			SV_WriteByte(tc_ceiling);
			memcpy(&ceiling, th, sizeof(ceiling_t));
			ceiling.sector = (sector_t *)(ceiling.sector-sectors);
			SV_Write(&ceiling, sizeof(ceiling_t));
			continue;
		}
		if(th->function == T_VerticalDoor)
		{
			SV_WriteByte(tc_door);
			memcpy(&door, th, sizeof(vldoor_t));
			door.sector = (sector_t *)(door.sector-sectors);
			SV_Write(&door, sizeof(vldoor_t));
			continue;
		}
		if(th->function == T_MoveFloor)
		{
			SV_WriteByte(tc_floor);
			memcpy(&floor, th, sizeof(floormove_t));
			floor.sector = (sector_t *)(floor.sector-sectors);
			SV_Write(&floor, sizeof(floormove_t));
			continue;
		}
		if(th->function == T_PlatRaise)
		{
			SV_WriteByte(tc_plat);
			memcpy(&plat, th, sizeof(plat_t));
			plat.sector = (sector_t *)(plat.sector-sectors);
			SV_Write(&plat, sizeof(plat_t));
			continue;
		}
		if(th->function == T_LightFlash)
		{
			SV_WriteByte(tc_flash);
			memcpy(&flash, th, sizeof(lightflash_t));
			flash.sector = (sector_t *)(flash.sector-sectors);
			SV_Write(&flash, sizeof(lightflash_t));
			continue;
		}
		if(th->function == T_StrobeFlash)
		{
			SV_WriteByte(tc_strobe);
			memcpy(&strobe, th, sizeof(strobe_t));
			strobe.sector = (sector_t *)(strobe.sector-sectors);
			SV_Write(&strobe, sizeof(strobe_t));
			continue;
		}
		if(th->function == T_Glow)
		{
			SV_WriteByte(tc_glow);
			memcpy(&glow, th, sizeof(glow_t));
			glow.sector = (sector_t *)(glow.sector-sectors);
			SV_Write(&glow, sizeof(glow_t));
			continue;
		}
	}
	// Add a terminating marker
	SV_WriteByte(tc_endspecials);
}
#endif

/*
====================
=
= P_UnArchiveSpecials
=
====================
*/

void P_v13_UnArchiveSpecials (void)
{
	byte		tclass;
	ceiling_t	*ceiling;
	vldoor_t	*door;
	floormove_t	*floor;
	plat_t		*plat;
	lightflash_t *flash;
	strobe_t	*strobe;
	glow_t		*glow;
	
	
// read in saved thinkers
	while (1)
	{
		tclass = *save_p++;
		switch (tclass)
		{
			case tc_endspecials:
				return;			// end of list
			
			case tc_ceiling:
				ceiling = Z_Malloc (sizeof(*ceiling), PU_LEVEL, NULL);
				memcpy (ceiling, save_p, sizeof(*ceiling));
				save_p += sizeof(*ceiling);
				ceiling->sector = &sectors[(int)ceiling->sector];
				ceiling->sector->specialdata = T_MoveCeiling;
				if (ceiling->thinker.function)
					ceiling->thinker.function = T_MoveCeiling;
				P_AddThinker (&ceiling->thinker);
				P_AddActiveCeiling(ceiling);
				break;

			case tc_door:
				door = Z_Malloc (sizeof(*door), PU_LEVEL, NULL);
				memcpy (door, save_p, sizeof(*door));
				save_p += sizeof(*door);
				door->sector = &sectors[(int)door->sector];
				door->sector->specialdata = door;
				door->thinker.function = T_VerticalDoor;
				P_AddThinker (&door->thinker);
				break;

			case tc_floor:
				floor = Z_Malloc (sizeof(*floor), PU_LEVEL, NULL);
				memcpy (floor, save_p, sizeof(*floor));
				save_p += sizeof(*floor);
				floor->sector = &sectors[(int)floor->sector];
				floor->sector->specialdata = T_MoveFloor;
				floor->thinker.function = T_MoveFloor;
				P_AddThinker (&floor->thinker);
				break;
				
			case tc_plat:
				plat = Z_Malloc (sizeof(*plat), PU_LEVEL, NULL);
				memcpy (plat, save_p, sizeof(*plat));
				save_p += sizeof(*plat);
				plat->sector = &sectors[(int)plat->sector];
				plat->sector->specialdata = T_PlatRaise;
				if (plat->thinker.function)
					plat->thinker.function = T_PlatRaise;
				P_AddThinker (&plat->thinker);
				P_AddActivePlat(plat);
				break;
				
			case tc_flash:
				flash = Z_Malloc (sizeof(*flash), PU_LEVEL, NULL);
				memcpy (flash, save_p, sizeof(*flash));
				save_p += sizeof(*flash);
				flash->sector = &sectors[(int)flash->sector];
				flash->thinker.function = T_LightFlash;
				P_AddThinker (&flash->thinker);
				break;
				
			case tc_strobe:
				strobe = Z_Malloc (sizeof(*strobe), PU_LEVEL, NULL);
				memcpy (strobe, save_p, sizeof(*strobe));
				save_p += sizeof(*strobe);
				strobe->sector = &sectors[(int)strobe->sector];
				strobe->thinker.function = T_StrobeFlash;
				P_AddThinker (&strobe->thinker);
				break;
				
			case tc_glow:
				glow = Z_Malloc (sizeof(*glow), PU_LEVEL, NULL);
				memcpy (glow, save_p, sizeof(*glow));
				save_p += sizeof(*glow);
				glow->sector = &sectors[(int)glow->sector];
				glow->thinker.function = T_Glow;
				P_AddThinker (&glow->thinker);
				break;
				
			default:
				Con_Error ("P_UnarchiveSpecials:Unknown tclass %i "
							"in savegame",tclass);
		}
	}
}

// In Heretic's case this should actually be v13...
void SV_v13_LoadGame(char *savename)
{
	int length;
	int i;
	int a, b, c;
	char vcheck[VERSIONSIZE];

	length = M_ReadFile(savename, &savebuffer);
	save_p = savebuffer + SAVESTRINGSIZE;
	// Skip the description field
	memset(vcheck, 0, sizeof(vcheck));
	sprintf(vcheck, "version %i", SAVE_VERSION);
	if (strcmp (save_p, vcheck))
	{ // Bad version
		Con_Message( "Savegame ID '%s': incompatible?\n", save_p);
	}
	save_p += VERSIONSIZE;
	gameskill = *save_p++;
	gameepisode = *save_p++;
	gamemap = *save_p++;
	for(i = 0; i < 4; i++)
	{
		players[i].plr->ingame = *save_p++;
	}
	// Load a base level
	G_InitNew(gameskill, gameepisode, gamemap);

	// Create leveltime
	a = *save_p++;
	b = *save_p++;
	c = *save_p++;
	leveltime = (a<<16)+(b<<8)+c;

	// De-archive all the modifications
	P_v13_UnArchivePlayers();
	P_v13_UnArchiveWorld();
	P_v13_UnArchiveThinkers();
	P_v13_UnArchiveSpecials();

	if(*save_p != SAVE_GAME_TERMINATOR)
	{ // Missing savegame termination marker
		Con_Error("Bad savegame");
	}
	Z_Free(savebuffer);

	// Spawn particle generators.
	R_SetupLevel("", DDSLF_AFTER_LOADING);
}