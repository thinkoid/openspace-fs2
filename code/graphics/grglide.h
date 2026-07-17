/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#ifndef _GRGLIDE_H
#define _GRGLIDE_H

void gr_glide_init();
void gr_glide_cleanup();

void gr_glide_bitmap(int x, int y);
void gr_glide_bitmap_ex(int x, int y, int w, int h, int sx, int sy);

#endif
