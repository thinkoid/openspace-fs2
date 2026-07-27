/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#ifndef _LIGHTING_H
#define _LIGHTING_H

// Light stuff works like this:
// At the start of the frame, call light_reset.
// For each light source, call light_add_??? functions.
// To calculate lighting, do:
// call light_filter_reset or light_filter.
// set up matrices with g3 functions
// call light_rotatate_all to rotate all valid
// lights into current coordinates.
// call light_apply to fill in lighting for a point.

void light_reset();
void light_set_ambient(float ambient_light);

// Intensity - how strong the light is.  1.0 will cast light around 5meters or so.
// r,g,b - only used for colored lighting. Ignored currently.
void light_add_directional( vector *dir, float intensity, float r, float g, float b );
void light_add_point( vector * pos, float r1, float r2, float intensity, float r, float g, float b, int ignore_objnum );
void light_add_point_unique( vector * pos, float r1, float r2, float intensity, float r, float g, float b, int affected_objnum);
void light_add_tube(vector *p0, vector *p1, float r1, float r2, float intensity, float r, float g, float b, int affected_objnum);
void light_rotate_all();

// Reset the list of lights to point to all lights.
void light_filter_reset();

// Makes a list of only the lights that will affect
// the sphere specified by 'pos' and 'rad' and 'objnum'.
// Returns number of lights active.
int light_filter_push( int objnum, vector *pos, float rad );
int light_filter_push_box( vector *min, vector *max );
void light_filter_pop();

// Applies light to a vertex.   In order for this to work, 
// it assumes that one of light_filter or light_filter_reset
// have been called.  It only uses 'vert' to fill in it's light
// fields.  'pos' is position of point, 'norm' is the norm.
ubyte light_apply( vector *pos, vector * norm, float static_light_val );

// Same as above only does RGB.
void light_apply_rgb( ubyte *param_r, ubyte *param_g, ubyte *param_b, vector *pos, vector * norm, float static_light_val );

// return the # of global light sources
int light_get_global_count();

// Fills direction of global light source N in pos.
// Returns 0 if there is no global light.
int light_get_global_dir(vector *pos, int n);

// Set to non-zero if we're in a shadow.
void light_set_shadow( int state );


#endif
