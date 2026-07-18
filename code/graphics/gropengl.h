/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell
 * or otherwise commercially exploit the source or things you created based on the
 * source.
 *
*/

#ifndef _GROPENGL_H
#define _GROPENGL_H

void gr_opengl_init();
void gr_opengl_cleanup();
void gr_opengl_activate(int active);
void gr_opengl_force_windowed();

void gr_opengl_bitmap(int x, int y);
void gr_opengl_bitmap_ex(int x, int y, int w, int h, int sx, int sy);

#endif
