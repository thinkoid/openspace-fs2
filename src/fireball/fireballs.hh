/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef _FIREBALLS_H
#define _FIREBALLS_H

#include <object/object.hh>
#include <cfile/cfile.hh>

// these values correspond to the fireball.tbl entries
// Used for the 4 little explosions before a ship explodes
#define FIREBALL_EXPLOSION_MEDIUM 0
// Used for the warp in / warp out effect
#define FIREBALL_WARP_EFFECT 1
// Used for the KNOSSOS warp in / warp out effect
#define FIREBALL_KNOSSOS_EFFECT 2
#define FIREBALL_ASTEROID 3
// Used for the big explosion when a ship breaks into pieces
#define FIREBALL_EXPLOSION_LARGE1 4
// Used for the big explosion when a ship breaks into pieces
#define FIREBALL_EXPLOSION_LARGE2 5
// #define FIREBALL_EXPLOSION_LARGE3   6           // Used for the big explosion when a ship breaks into pieces
// How many types there are
#define MAX_FIREBALL_TYPES 6

#define FIREBALL_NUM_LARGE_EXPLOSIONS 2

void fireball_init();
void fireball_render(object *obj);
void fireball_delete(object *obj);
void fireball_process_pre(object *obj, float frame_time);
void fireball_process_post(object *obj, float frame_time);

// reversed is for warp_in/out effects
// Velocity: If not NULL, the fireball will move at a constant velocity.
// warp_lifetime: If warp_lifetime > 0.0f then makes the explosion loop so it lasts this long.  Only works for warp effect
int fireball_create(vector *pos, int fireball_type, int parent_obj, float size,
                    int reversed = 0, vector *velocity = NULL,
                    float warp_lifetime = 0.0f, int ship_class = -1,
                    matrix *orient = NULL, int low_res = 0);
void fireball_render_plane(int plane);
void fireball_close();
void fireball_level_close();
void fireball_preload(); // page in warpout effect data

// Returns 1 if you can remove this fireball
int fireball_is_perishable(object *obj);

// Returns 1 if this fireball is a warp
int fireball_is_warp(object *obj);

// Returns life left of a fireball in seconds
float fireball_lifeleft(object *obj);

// Returns life left of a fireball in percent
float fireball_lifeleft_percent(object *obj);

// internal function to draw warp grid.
extern void warpin_render(matrix *orient, vector *pos, int texture_bitmap_num,
                          float radius, float life_percent, float max_radius);
extern int Warp_glow_bitmap; // Internal

#endif /* _FIREBALLS_H */
