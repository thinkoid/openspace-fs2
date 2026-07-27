/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef _TMAPPER_H
#define _TMAPPER_H

// call this to reinit the scanline function pointers.
extern void tmapper_setup();

// Used to tell the tmapper what the current lighting values are
// if the TMAP_FLAG_RAMP or TMAP_FLAG_RGB are set and the TMAP_FLAG_GOURAUD
// isn't set.
void tmapper_set_light(vertex *v, uint flags);

// DO NOT CALL grx_tmapper DIRECTLY!!!! Only use the
// gr_tmapper equivalent!!!!
extern void grx_tmapper(int nv, vertex *verts[], uint flags);

#define TMAP_MAX_VERTS 25 // Max number of vertices per polygon

// Flags to pass to g3_draw_??? routines
#define TMAP_FLAG_TEXTURED (1 << 0) // Uses texturing (Interpolate uv's)
#define TMAP_FLAG_CORRECT (1 << 1) // Perspective correct (Interpolate sw)
#define TMAP_FLAG_RAMP (1 << 2) // Use RAMP lighting (interpolate L)
#define TMAP_FLAG_RGB (1 << 3) // Use RGB lighting (interpolate RGB)
#define TMAP_FLAG_GOURAUD                                                        \
    (1 << 4) // Lighting values differ on each vertex.
        // If this is not set, then the texture mapper will use
        // the lighting parameters in each vertex, otherwise it
        // will use the ones specified in tmapper_set_??
#define TMAP_FLAG_XPARENT (1 << 5) // texture could have transparency
#define TMAP_FLAG_TILED (1 << 6) // This means uv's can be > 1.0
#define TMAP_FLAG_NEBULA                                                         \
    (1                                                                           \
     << 7) // Must be used with RAMP and GOURAUD.  Means l 0-1 is 0-31 palette entries

#define TMAP_HIGHEST_FLAG_BIT 7 // The highest bit used in the TMAP_FLAGS
#define TMAP_MAX_SCANLINES (1 << (TMAP_HIGHEST_FLAG_BIT + 1))

// Add any entries that don't work for software under here:
// Make sure to disable them at top of grx_tmapper
#define TMAP_FLAG_ALPHA (1 << 8) // Has an alpha component
#define TMAP_FLAG_NONDARKENING (1 << 9) // RGB=255,255,255 doesn't darken

// flags for full nebula effect
#define TMAP_FLAG_PIXEL_FOG                                                      \
    (1                                                                           \
     << 10) // fog the polygon based upon the average pixel colors of the backbuffer behind it

// bitmap section
#define TMAP_FLAG_BITMAP_SECTION (1 << 11)

#endif
