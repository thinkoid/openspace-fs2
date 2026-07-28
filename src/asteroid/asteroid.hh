/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef __ASTEROID_H__
#define __ASTEROID_H__

#include "globalincs/linklist.hh"
#include <ship/ship.hh>
#include <parse/parselo.hh> // for NAME_LENGTH

struct object;
struct polymodel;
struct collision_info_struct;

#define MAX_ASTEROIDS 256

// DEBRIS TYPES
#define MAX_DEBRIS_TYPES 12
#define ASTEROID_TYPE_SMALL 0
#define ASTEROID_TYPE_MEDIUM 1
#define ASTEROID_TYPE_BIG 2
//
#define DEBRIS_TERRAN_SMALL 3
#define DEBRIS_TERRAN_MEDIUM 4
#define DEBRIS_TERRAN_LARGE 5
//
#define DEBRIS_VASUDAN_SMALL 6
#define DEBRIS_VASUDAN_MEDIUM 7
#define DEBRIS_VASUDAN_LARGE 8
//
#define DEBRIS_SHIVAN_SMALL 9
#define DEBRIS_SHIVAN_MEDIUM 10
#define DEBRIS_SHIVAN_LARGE 11
// END DEBRIS TYPES

typedef struct debris_struct
{
    int index;
    char *name;
} debris_struct;

// Data structure to track the active asteroids
typedef struct asteroid_obj : list_links_t< asteroid_obj >
{
    int flags, objnum;
} asteroid_obj;
extern list_t< asteroid_obj > Asteroid_obj_list;

extern debris_struct Field_debris_info[];

// Max number of POFs per asteroid type
#define MAX_ASTEROID_POFS 3

// Set means used.
#define AF_USED (1 << 0)

typedef struct asteroid_info
{
    char name[NAME_LENGTH]; // name for the asteroid
    char pof_files[MAX_ASTEROID_POFS]
                  [NAME_LENGTH]; // POF files to load/associate with ship
    int num_detail_levels; // number of detail levels for this ship
    int detail_distance[MAX_SHIP_DETAIL_LEVELS]; // distance to change detail levels at
    float max_speed; // cap on speed for asteroid
    float inner_rad; // radius within which maximum area effect damage is applied
    float outer_rad; // radius at which no area effect damage is applied
    float damage; // maximum damage applied from area effect explosion
    float blast; // maximum blast impulse from area effect explosion
    float initial_hull_strength; // starting strength of asteroid
    polymodel *modelp[MAX_ASTEROID_POFS];
    int model_num[MAX_ASTEROID_POFS];
} asteroid_info;

typedef struct asteroid
{
    int flags;
    int objnum;
    int type; //  In 0..Num_asteroid_types
    int asteroid_subtype; // Which index into asteroid_info for modelnum and modelp
    int check_for_wrap; // timestamp to check for asteroid wrapping around field
    int check_for_collide; // timestamp to check for asteroid colliding with escort ships
    int final_death_time; // timestamp to swap in new models after explosion starts
    int collide_objnum; // set to objnum that asteroid will be impacting soon
    int collide_objsig; // object signature corresponding to collide_objnum
    vector death_hit_pos; // hit pos that caused death
    int target_objnum; //  Yes, hah!  Asteroids can have targets.  See asteroid_aim_at_target().
} asteroid;

// TYPEDEF FOR SPECIES OF DEBRIS - BITFIELD
#define DS_TERRAN 0x01
#define DS_VASUDAN 0x02
#define DS_SHIVAN 0x04

// TYPEDEF FOR DEBRIS TYPE
typedef enum { DG_ASTEROID, DG_SHIP } debris_genre;

// TYPEDEF FOR FIELD TYPE
typedef enum { FT_ACTIVE, FT_PASSIVE } field_type;

#define MAX_ACTIVE_DEBRIS_TYPES 3

typedef struct asteroid_field
{
    vector min_bound; //   Minimum range of field.
    vector max_bound; //   Maximum range of field.
    int has_inner_bound;
    vector inner_min_bound;
    vector inner_max_bound;
    vector vel; //   Average asteroid moves at this velocity.
    float speed; // Average speed of field
    int num_initial_asteroids; //   Number of asteroids at creation.
    field_type field_type; // active throws and wraps, passive does not
    debris_genre debris_genre; // type of debris (ship or asteroid)  [generic type]
    int field_debris_type
        [MAX_ACTIVE_DEBRIS_TYPES]; // one of the debris type defines above
} asteroid_field;

extern asteroid_info Asteroid_info[MAX_DEBRIS_TYPES];
extern asteroid Asteroids[MAX_ASTEROIDS];
extern asteroid_field Asteroid_field;

extern int Num_asteroid_types;
extern int Num_asteroids;
extern int Asteroids_enabled;

void asteroid_init();
void asteroid_level_init();
void asteroid_level_close();
void asteroid_create_all();
void asteroid_render(object *asteroid_objp);
void asteroid_delete(object *asteroid_objp);
void asteroid_process_pre(object *asteroid_objp, float frame_time);
void asteroid_process_post(object *asteroid_objp, float frame_time);
int asteroid_check_collision(object *asteroid_objp, object *other_obj,
                             vector *hitpos,
                             collision_info_struct *asteroid_hit_info = NULL);
void asteroid_hit(object *asteroid_objp, object *other_objp, vector *hitpos,
                  float damage);
int asteroid_count();
int asteroid_collide_objnum(object *asteroid_objp);
float asteroid_time_to_impact(object *asteroid_objp);
void asteroid_show_brackets();
void asteroid_target_closest_danger();
int asteroid_get_random_in_cone(vector *pos, vector *dir, float ang,
                                int danger = 0);

// need to extern for multiplayer
void asteroid_sub_create(object *parent_objp, int asteroid_type, vector *relvec);

void asteroid_frame();

#endif // __ASTEROID_H__
