/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef _SHIPFX_H
#define _SHIPFX_H

struct object;
struct ship;
struct ship_subsys;

// Make sparks fly off of ship n
// sn = spark number to spark, corrosponding to element in
//      ship->hitpos array.  If this isn't -1, it is a just
//      got hit by weapon spark, otherwise pick one randomally.
void shipfx_emit_spark(int n, int sn);

// Does the special effects to blow a subsystem off a ship
extern void shipfx_blow_off_subsystem(object *ship_obj, ship *ship_p,
                                      ship_subsys *subsys, vector *exp_center);

// Creates "ndebris" pieces of debris on random verts of the the "submodel" in the
// ship's model.
extern void shipfx_blow_up_model(object *obj, int model, int submodel,
                                 int ndebris, vector *exp_center);

// put here for multiplayer purposes
void shipfx_blow_up_hull(object *obj, int model, vector *exp_center);

//          SHIP WARP IN EFFECT STUFF

// When a ship warps in, this gets called to start the effect
extern void shipfx_warpin_start(object *objp);

// During a ship warp in, this gets called each frame to move the ship
extern void shipfx_warpin_frame(object *objp, float frametime);

// When a ship warps out, this gets called to start the effect
extern void shipfx_warpout_start(object *objp);

// During a ship warp out, this gets called each frame to move the ship
extern void shipfx_warpout_frame(object *objp, float frametime);

//          SHIP SHADOW EFFECT STUFF

// Given point p0, in object's frame of reference, find if
// it can see the sun.
int shipfx_point_in_shadow(vector *p0, matrix *src_orient, vector *src_pos,
                           float radius);

// Given an ship see if it is in a shadow.
int shipfx_in_shadow(object *src_obj);

// Given world point see if it is in a shadow.
int shipfx_eye_in_shadow(vector *eye_pos, object *src_obj, int sun_n);

//          SHIP GUN FLASH EFFECT STUFF

// Resets the ship flash stuff. Call before
// each level.
void shipfx_flash_init();

// Given that a ship fired a weapon, light up the model
// accordingly.
// Set is_primary to non-zero if this is a primary weapon.
// Gun_pos should be in object's frame of reference, not world!!!
void shipfx_flash_create(object *objp, ship *shipp, vector *gun_pos,
                         vector *gun_dir, int is_primary, int weapon_info_index);

// Sets the flash lights in the model used by this
// ship to the appropriate values.  There might not
// be any flashes linked to this ship in which
// case this function does nothing.
void shipfx_flash_light_model(object *objp, ship *shipp);

// Does whatever processing needs to be done each frame.
void shipfx_flash_do_frame(float frametime);

//          LARGE SHIP EXPLOSION EFFECT STUFF

// Call between levels
void shipfx_large_blowup_level_init();

// Returns 0 if couldn't init
int shipfx_large_blowup_init(ship *shipp);

// Returns 1 when explosion is done
int shipfx_large_blowup_do_frame(ship *shipp, float frametime);

void shipfx_large_blowup_render(ship *shipp);

// sound manager fore big ship sub explosions sounds
void do_sub_expl_sound(float radius, vector *sound_pos, int *sound_handle);

// do all shockwaves for a ship blowing up
void shipfx_do_shockwave_stuff(ship *shipp, shockwave_create_info *sci);

//          ELECTRICAL SPARKS ON DAMAGED SHIPS EFFECT STUFF
void shipfx_do_damaged_arcs_frame(ship *shipp);

//          NEBULA LIGHTNING.
void shipfx_do_lightning_frame(ship *shipp);

// engine wash level init
void shipfx_engine_wash_level_init();

// pause engine wash sounds
void shipfx_stop_engine_wash_sound();

#endif
