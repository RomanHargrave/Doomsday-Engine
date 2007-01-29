/* Generated by ../../engine/scripts/makedmt.py */

#ifndef __DOOMSDAY_PLAY_PUBLIC_MAP_DATA_TYPES_H__
#define __DOOMSDAY_PLAY_PUBLIC_MAP_DATA_TYPES_H__

#define DMT_VERTEX_POS DDVT_FLOAT

#define DMT_SEG_V DDVT_PTR             // [Start, End] of the segment.
#define DMT_SEG_LENGTH DDVT_FLOAT      // Accurate length of the segment (v1 -> v2).
#define DMT_SEG_OFFSET DDVT_FLOAT
#define DMT_SEG_SIDEDEF DDVT_PTR
#define DMT_SEG_LINEDEF DDVT_PTR
#define DMT_SEG_SEC DDVT_PTR
#define DMT_SEG_ANGLE DDVT_ANGLE
#define DMT_SEG_FLAGS DDVT_BYTE

#define DMT_SUBSECTOR_SECTOR DDVT_PTR
#define DMT_SUBSECTOR_SEGCOUNT DDVT_UINT
#define DMT_SUBSECTOR_FIRSTSEG DDVT_PTR
#define DMT_SUBSECTOR_POLY DDVT_PTR    // NULL, if there is no polyobj.
#define DMT_SUBSECTOR_FLAGS DDVT_BYTE

#define DMT_SURFACE_FLAGS DDVT_INT     // SUF_ flags
#define DMT_SURFACE_TEXTURE DDVT_SHORT
#define DMT_SURFACE_TEXMOVE DDVT_FLOAT // Texture movement X and Y
#define DMT_SURFACE_OFFX DDVT_FLOAT    // Texture x offset
#define DMT_SURFACE_OFFY DDVT_FLOAT    // Texture y offset
#define DMT_SURFACE_RGBA DDVT_BYTE     // Surface color tint

#define DMT_PLANE_HEIGHT DDVT_FLOAT    // Current height
#define DMT_PLANE_GLOW DDVT_FLOAT      // Glow amount
#define DMT_PLANE_GLOWRGB DDVT_BYTE    // Glow color
#define DMT_PLANE_TARGET DDVT_FLOAT    // Target height
#define DMT_PLANE_SPEED DDVT_FLOAT     // Move speed
#define DMT_PLANE_SOUNDORG DDVT_PTR    // Sound origin for plane
#define DMT_PLANE_SECTOR DDVT_PTR      // Owner of the plane (temp)

#define DMT_SECTOR_LIGHTLEVEL DDVT_SHORT
#define DMT_SECTOR_RGB DDVT_BYTE
#define DMT_SECTOR_VALIDCOUNT DDVT_INT // if == validcount, already checked.
#define DMT_SECTOR_THINGLIST DDVT_PTR  // List of mobjs in the sector.
#define DMT_SECTOR_LINECOUNT DDVT_UINT
#define DMT_SECTOR_LINES DDVT_PTR      // [linecount] size.
#define DMT_SECTOR_SUBSCOUNT DDVT_UINT
#define DMT_SECTOR_SUBSECTORS DDVT_PTR // [subscount] size.
#define DMT_SECTOR_SOUNDORG DDVT_PTR
#define DMT_SECTOR_REVERB DDVT_FLOAT
#define DMT_SECTOR_PLANECOUNT DDVT_UINT

#define DMT_SIDE_BLENDMODE DDVT_BLENDMODE
#define DMT_SIDE_SECTOR DDVT_PTR
#define DMT_SIDE_FLAGS DDVT_SHORT

#define DMT_LINE_V DDVT_PTR
#define DMT_LINE_FLAGS DDVT_SHORT
#define DMT_LINE_SEC DDVT_PTR          // [front, back] sectors.
#define DMT_LINE_DX DDVT_FLOAT
#define DMT_LINE_DY DDVT_FLOAT
#define DMT_LINE_SLOPETYPE DDVT_INT
#define DMT_LINE_VALIDCOUNT DDVT_INT
#define DMT_LINE_SIDES DDVT_PTR
#define DMT_LINE_BBOX DDVT_FIXED

#define DMT_POLYOBJ_NUMSEGS DDVT_UINT
#define DMT_POLYOBJ_SEGS DDVT_PTR
#define DMT_POLYOBJ_VALIDCOUNT DDVT_INT
#define DMT_POLYOBJ_STARTSPOT DDVT_PTR
#define DMT_POLYOBJ_ANGLE DDVT_ANGLE
#define DMT_POLYOBJ_TAG DDVT_INT       // reference tag assigned in HereticEd
#define DMT_POLYOBJ_BBOX DDVT_FIXED
#define DMT_POLYOBJ_SPEED DDVT_INT     // Destination XY and speed.
#define DMT_POLYOBJ_DESTANGLE DDVT_ANGLE // Destination angle.
#define DMT_POLYOBJ_ANGLESPEED DDVT_ANGLE // Rotation speed.
#define DMT_POLYOBJ_CRUSH DDVT_BOOL    // should the polyobj attempt to crush mobjs?
#define DMT_POLYOBJ_SEQTYPE DDVT_INT
#define DMT_POLYOBJ_SIZE DDVT_FIXED    // polyobj size (area of POLY_AREAUNIT == size of FRACUNIT)
#define DMT_POLYOBJ_SPECIALDATA DDVT_PTR // pointer a thinker, if the poly is moving

#define DMT_NODE_X DDVT_FLOAT          // Partition line.
#define DMT_NODE_Y DDVT_FLOAT          // Partition line.
#define DMT_NODE_DX DDVT_FLOAT         // Partition line.
#define DMT_NODE_DY DDVT_FLOAT         // Partition line.
#define DMT_NODE_BBOX DDVT_FLOAT       // Bounding box for each child.
#define DMT_NODE_CHILDREN DDVT_UINT    // If NF_SUBSECTOR it's a subsector.

#endif
