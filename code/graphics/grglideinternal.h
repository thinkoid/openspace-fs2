/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#ifndef _GRGLIDEINTERNAL_H
#define _GRGLIDEINTERNAL_H

#include "grinternal.h"

void glide_tcache_init();
void glide_tcache_cleanup();
void glide_tcache_flush();
void glide_tcache_frame();

// Bitmap_type see TCACHE_ defines in GrInternal.h
int glide_tcache_set(int bitmap_id, int bitmap_type, float *u_ratio, float *v_ratio, int fail_on_full = 0, int sx = -1, int sy = -1, int force = 0);

extern int Glide_textures_in;

#endif //_GRGLIDEINTERNAL_H
