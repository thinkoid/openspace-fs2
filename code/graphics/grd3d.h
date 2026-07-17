/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#ifndef _GRD3D_H
#define _GRD3D_H

void gr_d3d_init();
void gr_d3d_cleanup();

// call this to safely fill in the texture shift and scale values for the specified texture type (Gr_t_*)
void gr_d3d_get_tex_format(int alpha);

// bitmap functions
void gr_d3d_bitmap(int x, int y);
void gr_d3d_bitmap_ex(int x, int y, int w, int h, int sx, int sy);

// create all rendering objects (surfaces, d3d device, viewport, etc)
int gr_d3d_create_rendering_objects(int clear);
void gr_d3d_release_rendering_objects();


void gr_d3d_set_initial_render_state();

#endif
