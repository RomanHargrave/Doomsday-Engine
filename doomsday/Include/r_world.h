//===========================================================================
// R_WORLD.H
//===========================================================================
#ifndef __DOOMSDAY_REFRESH_WORLD_H__
#define __DOOMSDAY_REFRESH_WORLD_H__

// Map Info flags.
#define MIF_FOG				0x1		// Fog is used in the level.

void R_SetupLevel(char *level_id, int flags);
void R_SetupFog(void);
void R_SetupSky(void);
void R_SetSectorLinks(sector_t *sec);
sector_t *R_GetLinkedSector(sector_t *startsec, boolean getfloor);
void R_UpdatePlanes(void);
void R_ClearSectorFlags(void);

#endif 