/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef __SHOCKWAVE_H__
#define __SHOCKWAVE_H__

#include "globalincs/linklist.hh"
#include <cfile/cfile.hh>
#include <object/object.hh>

#define SW_USED (1 << 0)
#define SW_WEAPON (1 << 1)
#define SW_SHIP_DEATH (1 << 2)
// Shockwave created when weapon destroyed by another
#define SW_WEAPON_KILL (1 << 3)

#define MAX_SHOCKWAVES 16
#define MAX_SHOCKWAVE_TYPES 1
#define SW_MAX_OBJS_HIT 64

typedef struct shockwave_info
{
    int bitmap_id;
    int num_frames;
    int fps;
} shockwave_info;

typedef struct shockwave : list_links_t< shockwave >
{
    int flags;
    int objnum; // index into Objects[] for shockwave
    int num_objs_hit;
    int obj_sig_hitlist[SW_MAX_OBJS_HIT];
    float speed, radius;
    float inner_radius, outer_radius, damage;
    int weapon_info_index; // -1 if shockwave not caused by weapon
    vector pos;
    float blast; // amount of blast to apply
    int next_blast; // timestamp for when to apply next blast damage
    int shockwave_info_index;
    int current_bitmap;
    float time_elapsed; // in seconds
    float total_time; // total lifetime of animation in seconds
    int delay_stamp; // for delayed shockwaves
    float rot_angle;
} shockwave;

typedef struct shockwave_create_info
{
    float inner_rad;
    float outer_rad;
    float damage;
    float blast;
    float speed;
    float rot_angle;
} shockwave_create_info;

extern shockwave Shockwaves[MAX_SHOCKWAVES];
extern shockwave_info Shockwave_info[MAX_SHOCKWAVE_TYPES];

void shockwave_close();
void shockwave_level_init();
void shockwave_level_close();
void shockwave_delete(object *objp);
void shockwave_move_all(float frametime);
int shockwave_create(int parent_objnum, vector *pos, shockwave_create_info *sci,
                     int flag, int delay = -1);
void shockwave_render(object *objp);
int shockwave_weapon_index(int index);
float shockwave_max_radius(int index);

#endif /* __SHOCKWAVE_H__ */
