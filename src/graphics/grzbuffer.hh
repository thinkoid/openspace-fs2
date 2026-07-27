/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef _GRZBUFFER_H
#define _GRZBUFFER_H

// Z-buffer stuff
extern uint *gr_zbuffer;
extern uint gr_zbuffer_offset; // Add this to pixel location to get zbuffer location
extern int gr_zoffset; // add this to w before interpolation

extern int gr_zbuffering, gr_zbuffering_mode;
extern int gr_global_zbuffering;

// (2^31)/GR_Z_COUNT
#define GR_Z_RANGE 0x400000
// How many frames between zbuffer clear.
// The bigger, the less precise.
#define GR_Z_COUNT 500

// If mode is FALSE, turn zbuffer off the entire frame,
// no matter what people pass to gr_zbuffer_set.
void gr8_zbuffer_clear(int mode);
int gr8_zbuffer_get();
int gr8_zbuffer_set(int mode);

#endif //_GRZBUFFER_H
