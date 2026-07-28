/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#include <graphics/2d.hh>
#include <render/3d.hh>
#include <weapon/weapon.hh>
#include <ship/ship.hh>
#include <freespace2/freespace.hh> // for colors
#include <weapon/shockwave.hh>
#include <io/timer.hh>
#include <anim/animplay.hh>
#include <bmpman/bmpman.hh>
#include <globalincs/linklist.hh>
#include <ship/shiphit.hh>
#include <gamesnd/gamesnd.hh>
#include <asteroid/asteroid.hh>

// -----------------------------------------------------------
// Data structures
// -----------------------------------------------------------

// -----------------------------------------------------------
// Module-wide globals
// -----------------------------------------------------------

static char *Shockwave_filenames[MAX_SHOCKWAVE_TYPES] = {
    //XSTR:OFF
    "shockwave01"
    //XSTR:ON
};

shockwave Shockwaves[MAX_SHOCKWAVES];
shockwave_info Shockwave_info[MAX_SHOCKWAVE_TYPES];

list_t< shockwave > Shockwave_list;
int Shockwave_inited = 0;

// -----------------------------------------------------------
// Function macros
// -----------------------------------------------------------
#define SW_INDEX(sw) (sw - Shockwaves)

// -----------------------------------------------------------
// Externals
// -----------------------------------------------------------
extern int Show_area_effect;

// ------------------------------------------------------------------------------------
// shockwave_create()
//
// Call to create a shockwave
//
// input:   parent_objnum  => object number of object spawning the shockwave
//          pos            => vector specifing global position of shockwave center
//          speed          => speed at which shockwave expands (m/s)
//          inner_radius   => radius at which damage applied is at maximum
//          outer_radius   => damage decreases linearly to zero from inner_radius to
//                            outer_radius.  Outside outer_radius, damage is 0.
//          damage         => the maximum damage (ie within inner_radius)
//          blast          => the maximux blast (within inner_radius)
//          sw_flag        => indicates whether shockwave is from weapon or ship explosion
//          delay          => delay in ms before the shockwave actually starts
//
// return:  success        => object number of shockwave
//          failure        => -1
//
int
shockwave_create(int parent_objnum, vector *pos, shockwave_create_info *sci,
                 int flag, int delay)
{
    int i, objnum, real_parent;
    shockwave *sw;
    shockwave_info *si;
    matrix orient;

    for (i = 0; i < MAX_SHOCKWAVES; i++) {
        if (!(Shockwaves[i].flags & SW_USED)) {
            break;
        }
    }

    if (i == MAX_SHOCKWAVES) {
        return -1;
    }

    // real_parent is the guy who caused this shockwave to happen
    if (Objects[parent_objnum].type == OBJ_WEAPON) {
        real_parent = Objects[parent_objnum].parent;
    }
    else {
        real_parent = parent_objnum;
    }

    sw = &Shockwaves[i];
    sw->flags = (SW_USED | flag);
    sw->speed = sci->speed;
    sw->inner_radius = sci->inner_rad;
    sw->outer_radius = sci->outer_rad;
    sw->damage = sci->damage;
    sw->blast = sci->blast;
    sw->radius = 1.0f;
    sw->pos = *pos;
    sw->num_objs_hit = 0;
    sw->shockwave_info_index =
        0; // only one type for now... type could be passed is as a parameter
    sw->current_bitmap = -1;

    sw->time_elapsed = 0.0f;
    sw->delay_stamp = delay;

    sw->rot_angle = sci->rot_angle;

    si = &Shockwave_info[sw->shockwave_info_index];
    //   sw->total_time = i2fl(si->num_frames) / si->fps;   // in seconds
    sw->total_time = sw->outer_radius / sw->speed;

    if (Objects[parent_objnum].type == OBJ_WEAPON) {
        sw->weapon_info_index =
            Weapons[Objects[parent_objnum].instance].weapon_info_index;
    }
    else {
        sw->weapon_info_index = -1;
    }

    orient = vmd_identity_matrix;

    objnum = obj_create(OBJ_SHOCKWAVE, real_parent, i, &orient, &sw->pos,
                        sw->outer_radius, OF_RENDERS);

    if (objnum == -1) {
        Int3();
    }

    sw->objnum = objnum;

    list_append(&Shockwave_list, sw);

    return objnum;
}

// ------------------------------------------------------------------------------------
// shockwave_delete()
//
// Delete a shockwave
//
// input:   object *objp   =>    pointer to shockwave object
//
void
shockwave_delete(object *objp)
{
    Assert(objp->type == OBJ_SHOCKWAVE);
    Assert(objp->instance >= 0 && objp->instance < MAX_SHOCKWAVES);

    Shockwaves[objp->instance].flags = 0;
    Shockwaves[objp->instance].objnum = -1;
    list_remove(&Shockwaves[objp->instance]);
}

// ------------------------------------------------------------------------------------
// shockwave_delete_all()
//
//
void
shockwave_delete_all()
{
    shockwave *sw, *next;

    sw = GET_FIRST(&Shockwave_list);
    while (sw != END_OF_LIST(&Shockwave_list)) {
        next = sw->next;
        Assert(sw->objnum != -1);
        Objects[sw->objnum].flags |= OF_SHOULD_BE_DEAD;
        sw = next;
    }
}

// Set the correct frame of animation for the shockwave
void
shockwave_set_framenum(int index)
{
    int framenum;
    shockwave *sw;
    shockwave_info *si;

    sw = &Shockwaves[index];
    si = &Shockwave_info[sw->shockwave_info_index];

    framenum = fl2i(sw->time_elapsed / sw->total_time * si->num_frames + 0.5);

    // ensure we don't go past the number of frames of animation
    if (framenum > (si->num_frames - 1)) {
        framenum = (si->num_frames - 1);
        Objects[sw->objnum].flags |= OF_SHOULD_BE_DEAD;
    }

    if (framenum < 0) {
        framenum = 0;
    }

    sw->current_bitmap = si->bitmap_id + framenum;
}

// ------------------------------------------------------------------------------------
// shockwave_move()
//
// Simulate a single shockwave.  If the shockwave radius exceeds outer_radius, then
// delete the shockwave.
//
// input:      ojbp        =>    object pointer that points to shockwave object
//             frametime   =>    time to simulate shockwave
//
void
shockwave_move(object *shockwave_objp, float frametime)
{
    shockwave *sw;
    object *objp;
    float blast, damage;
    int i;

    Assert(shockwave_objp->type == OBJ_SHOCKWAVE);
    Assert(shockwave_objp->instance >= 0 &&
           shockwave_objp->instance < MAX_SHOCKWAVES);
    sw = &Shockwaves[shockwave_objp->instance];

    // if the shockwave has a delay on it
    if (sw->delay_stamp != -1) {
        if (timestamp_elapsed(sw->delay_stamp)) {
            sw->delay_stamp = -1;
        }
        else {
            return;
        }
    }

    sw->time_elapsed += frametime;
    /*
   if ( sw->time_elapsed > sw->total_time ) {
      shockwave_objp->flags |= OF_SHOULD_BE_DEAD;
   }
*/

    shockwave_set_framenum(shockwave_objp->instance);

    sw->radius += (frametime * sw->speed);
    if (sw->radius > sw->outer_radius) {
        sw->radius = sw->outer_radius;
        shockwave_objp->flags |= OF_SHOULD_BE_DEAD;
        return;
    }

    // blast ships and asteroids
    for (objp = GET_FIRST(&obj_used_list); objp != END_OF_LIST(&obj_used_list);
         objp = GET_NEXT(objp)) {
        if ((objp->type != OBJ_SHIP) && (objp->type != OBJ_ASTEROID)) {
            continue;
        }

        if (objp->type == OBJ_SHIP) {
            // don't blast navbuoys
            if (ship_get_SIF(objp->instance) & SIF_NAVBUOY) {
                continue;
            }
        }

        // only apply damage to a ship once from a shockwave
        for (i = 0; i < sw->num_objs_hit; i++) {
            if (objp->signature == sw->obj_sig_hitlist[i]) {
                break;
            }
        }
        if (i < sw->num_objs_hit) {
            continue;
        }

        if (weapon_area_calc_damage(objp, &sw->pos, sw->inner_radius,
                                    sw->outer_radius, sw->blast, sw->damage,
                                    &blast, &damage, sw->radius) == -1) {
            continue;
        }

        // okay, we have damage applied, record the object signature so we don't repeatedly apply damage
        Assert(sw->num_objs_hit < SW_MAX_OBJS_HIT);
        if (sw->num_objs_hit >= SW_MAX_OBJS_HIT) {
            sw->num_objs_hit--;
        }

        switch (objp->type) {
        case OBJ_SHIP:
            sw->obj_sig_hitlist[sw->num_objs_hit++] = objp->signature;
            ship_apply_global_damage(objp, shockwave_objp, &sw->pos, damage);
            weapon_area_apply_blast(NULL, objp, &sw->pos, blast, 1);
            break;
        case OBJ_ASTEROID:
            asteroid_hit(objp, NULL, NULL, damage);
            break;
        default:
            Int3();
            break;
        }

        // If this shockwave hit the player, play shockwave impact sound
        if (objp == Player_obj) {
            snd_play(&Snds[SND_SHOCKWAVE_IMPACT], 0.0f,
                     max(0.4f,
                         damage / Weapon_info[sw->weapon_info_index].damage));
        }

    } // end for
}

// ------------------------------------------------------------------------------------
// shockwave_render()
//
// Draw the shockwave identified by handle
//
// input:   objp  =>    pointer to shockwave object
//
void
shockwave_render(object *objp)
{
    shockwave *sw;
    shockwave_info *si;
    vertex p;

    Assert(objp->type == OBJ_SHOCKWAVE);
    Assert(objp->instance >= 0 && objp->instance < MAX_SHOCKWAVES);

    sw = &Shockwaves[objp->instance];
    si = &Shockwave_info[sw->shockwave_info_index];

    if ((sw->delay_stamp != -1) && !timestamp_elapsed(sw->delay_stamp)) {
        return;
    }

    if (sw->current_bitmap < 0) {
        return;
    }

    // turn off fogging
    if (The_mission.flags & MISSION_FLAG_FULLNEB) {
        gr_fog_set(GR_FOGMODE_NONE, 0, 0, 0);
    }

    g3_rotate_vertex(&p, &sw->pos);

    gr_set_bitmap(sw->current_bitmap, GR_ALPHABLEND_FILTER, GR_BITBLT_MODE_NORMAL,
                  1.3f);
    g3_draw_rotated_bitmap(&p, fl_radian(sw->rot_angle), sw->radius,
                           TMAP_FLAG_TEXTURED);
}

// ------------------------------------------------------------------------------------
// shockwave_init()
//
// Call once at the start of each level (mission)
//
void
shockwave_level_init()
{
    int i;
    shockwave_info *si;

    // load in shockwaves
    for (i = 0; i < MAX_SHOCKWAVE_TYPES; i++) {
        si = &Shockwave_info[i];
        si->bitmap_id = bm_load_animation(Shockwave_filenames[i], &si->num_frames,
                                          &si->fps, 1);
        if (si->bitmap_id < 0) {
            Error(LOCATION, "Could not load %s anim file\n",
                  Shockwave_filenames[i]);
        }
    }

    list_init(&Shockwave_list);

    for (i = 0; i < MAX_SHOCKWAVES; i++) {
        Shockwaves[i].flags = 0;
        Shockwaves[i].objnum = -1;
    }

    Shockwave_inited = 1;
}

// ------------------------------------------------------------------------------------
// shockwave_level_close()
//
// Call at the close of each level (mission)
void
shockwave_level_close()
{
    if (Shockwave_inited) {
        shockwave_delete_all();
    }
    Shockwave_inited = 0;
}

// ------------------------------------------------------------------------------------
// shockwave_close()
//
// Called at game-shutdown to
//
void
shockwave_close()
{ }

// ------------------------------------------------------------------------------------
// shockwave_move_all()
//
// Simulate all shockwaves in Shockwave_list
//
// input:   frametime   =>    time for last frame in ms
//
void
shockwave_move_all(float frametime)
{
    shockwave *sw, *next;

    sw = GET_FIRST(&Shockwave_list);
    while (sw != END_OF_LIST(&Shockwave_list)) {
        next = sw->next;
        Assert(sw->objnum != -1);
        shockwave_move(&Objects[sw->objnum], frametime);
        sw = next;
    }
}

// ------------------------------------------------------------------------------------
// shockwave_render_all()
//
//
void
shockwave_render_all()
{
    shockwave *sw, *next;

    sw = GET_FIRST(&Shockwave_list);
    while (sw != END_OF_LIST(&Shockwave_list)) {
        next = sw->next;
        Assert(sw->objnum != -1);
        shockwave_render(&Objects[sw->objnum]);
        sw = next;
    }
}

// return the weapon_info_index field for a shockwave
int
shockwave_weapon_index(int index)
{
    return Shockwaves[index].weapon_info_index;
}

// return the maximum radius for specified shockwave
float
shockwave_max_radius(int index)
{
    return Shockwaves[index].outer_radius;
}

void
shockwave_page_in()
{
    int i;
    shockwave_info *si;

    // load in shockwaves
    for (i = 0; i < MAX_SHOCKWAVE_TYPES; i++) {
        si = &Shockwave_info[i];
        bm_page_in_texture(si->bitmap_id, si->num_frames);
    }
}
