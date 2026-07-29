/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#include <stdlib.h>

#include <math/vecmat.hh>
#include <graphics/tmapper.hh>
#include <graphics/2d.hh>
#include <render/3d.hh>
#include <bmpman/bmpman.hh>
#include <model/model.hh>
#include <io/key.hh>
#include <physics/physics.hh>
#include <math/floating.hh>
#include <model/model.hh>
#include <lighting/lighting.hh>
#include <object/object.hh>
#include <ship/ship.hh>
#include <globalincs/systemvars.hh>
#include <fireball/fireballs.hh>
#include <globalincs/linklist.hh>
#include <gamesnd/gamesnd.hh>
#include <localization/localize.hh>

// time for warphole to reach max size (also time to shrink to nothing once it begins to shrink)
#define WARPHOLE_GROW_TIME (1.5f)

#define MAX_FIREBALL_LOD 4

typedef struct fireball_lod
{
    char filename[MAX_FILENAME_LEN];
    int bitmap_id;
    int num_frames;
    int fps;
} fireball_lod;

typedef struct fireball_info
{
    int lod_count;
    fireball_lod lod[4];
} fireball_info;

// flag values for fireball struct flags member
#define FBF_WARP_CLOSE_SOUND_PLAYED (1 << 0)
#define FBF_WARP_CAPTIAL_SIZE (1 << 1)
#define FBF_WARP_CRUISER_SIZE (1 << 2)

typedef struct fireball
{
    int objnum; // If -1 this object is unused
    int fireball_info_index; // Index into Fireball_info array
    int current_bitmap;
    int orient; // For fireballs, which orientation.  For warps, 0 is warpin, 1 is warpout
    int flags; // see #define FBF_*
    char lod; // current LOD
    float time_elapsed; // in seconds
    float total_time; // total lifetime of animation in seconds
} fireball;

#define MAX_FIREBALLS 200

fireball Fireballs[MAX_FIREBALLS];

fireball_info Fireball_info[MAX_FIREBALL_TYPES];

int Num_fireballs = 0;

int fireballs_inited = 0;

int Warp_glow_bitmap = -1;

#define FB_INDEX(fb) (fb - Fireballs)

// play warp in sound for warp effect
void
fireball_play_warphole_open_sound(int ship_class, fireball *fb)
{
    int sound_index;
    float range_multiplier = 1.0f;
    object *fireball_objp;

    Assert((fb != NULL) && (fb->objnum >= 0));
    if ((fb == NULL) || (fb->objnum < 0)) {
        return;
    }
    fireball_objp = &Objects[fb->objnum];

    sound_index = SND_WARP_IN;

    if ((ship_class >= 0) && (ship_class < Num_ship_types)) {
        if (Ship_info[ship_class].flags & SIF_HUGE_SHIP) {
            sound_index = SND_CAPITAL_WARP_IN;
            fb->flags |= FBF_WARP_CAPTIAL_SIZE;
        }
        else if (Ship_info[ship_class].flags & SIF_BIG_SHIP) {
            range_multiplier = 6.0f;
            fb->flags |= FBF_WARP_CRUISER_SIZE;
        }
    }

    snd_play_3d(&Snds[sound_index], &fireball_objp->pos, &Eye_position,
                fireball_objp->radius, NULL, 0, 1.0f,
                SND_PRIORITY_DOUBLE_INSTANCE, NULL,
                range_multiplier); // play warp sound effect
}

// play warp out sound for warp effect
void
fireball_play_warphole_close_sound(fireball *fb)
{
    int sound_index;

    object *fireball_objp;

    fireball_objp = &Objects[fb->objnum];

    sound_index = SND_WARP_OUT;

    if (fb->flags & FBF_WARP_CAPTIAL_SIZE) {
        sound_index = SND_CAPITAL_WARP_OUT;
    }
    else {
        // AL 27-3-98: Decided that warphole closing is only required for captial ship sized warp effects.
        return;
    }

    snd_play_3d(&Snds[sound_index], &fireball_objp->pos, &Eye_position,
                fireball_objp->radius); // play warp sound effect
}

void
fireball_parse_tbl()
{
    int rval, idx;
    char base_filename[256] = "";

    // open localization
    lcl_ext_open();

    if ((rval = setjmp(parse_abort)) != 0) {
        Error(LOCATION, "Unable to parse fireball.tbl!  Code = %i.\n", rval);
    }
    else {
        read_file_text(NOX("fireball.tbl"));
        reset_parse();
    }

    int ntypes = 0;
    required_string("#Start");
    while (required_string_either("#End", "$Name:")) {
        Assert(ntypes < MAX_FIREBALL_TYPES);

        // base filename
        required_string("$Name:");
        stuff_string(base_filename, F_NAME, NULL);

        // # of lod levels - make sure old fireball.tbl is compatible
        Fireball_info[ntypes].lod_count = 1;
        if (optional_string("$LOD:")) {
            stuff_int(&Fireball_info[ntypes].lod_count);
        }

        // stuff default filename
        strcpy(Fireball_info[ntypes].lod[0].filename, base_filename);

        // stuff LOD level filenames
        for (idx = 1; idx < Fireball_info[ntypes].lod_count; idx++) {
            if (idx >= MAX_FIREBALL_LOD) {
                break;
            }

            // bounded: filename is MAX_FILENAME_LEN, the table name may not be
            sprintf(Fireball_info[ntypes].lod[idx].filename, "%.*s_%d",
                    MAX_FILENAME_LEN - 4, base_filename, idx);
        }

        ntypes++;
    }
    required_string("#End");

    // close localization
    lcl_ext_close();
}

void
fireball_load_data()
{
    int i, idx;
    fireball_info *fd;

    for (i = 0; i < MAX_FIREBALL_TYPES; i++) {
        fd = &Fireball_info[i];

        for (idx = 0; idx < fd->lod_count; idx++) {
            fd->lod[idx].bitmap_id = bm_load_animation(fd->lod[idx].filename,
                                                       &fd->lod[idx].num_frames,
                                                       &fd->lod[idx].fps, 1);
            if (fd->lod[idx].bitmap_id < 0) {
                Error(LOCATION, "Could not load %s anim file\n",
                      fd->lod[idx].filename);
            }
        }
    }

    if (Warp_glow_bitmap == -1) {
        Warp_glow_bitmap = bm_load(NOX("warpglow01"));
    }
}

void
fireball_preload()
{
    // Do nothing.  Called before level init, this used to page in warp effect.
    // Not needed with new BmpMan system.
}

// This will get called at the start of each level.
void
fireball_init()
{
    int i;

    if (!fireballs_inited) {
        fireballs_inited = 1;

        // Do all the processing that happens only once
        fireball_parse_tbl();
        fireball_load_data();
    }

    // Reset everything between levels
    Num_fireballs = 0;
    for (i = 0; i < MAX_FIREBALLS; i++) {
        Fireballs[i].objnum = -1;
    }
}

MONITOR(NumFireballsRend);

void
fireball_render(object *obj)
{
    int num;
    vertex p;
    fireball *fb;

    MONITOR_INC(NumFireballsRend, 1);

    num = obj->instance;
    fb = &Fireballs[num];

    if (Fireballs[num].current_bitmap < 0)
        return;

    //   gr_set_color( 0, 100, 0 );
    //   g3_draw_sphere_ez( &obj->pos, obj->radius );
    //   return;

    // turn off fogging
    if (The_mission.flags & MISSION_FLAG_FULLNEB) {
        gr_fog_set(GR_FOGMODE_NONE, 0, 0, 0);
    }

    g3_rotate_vertex(&p, &obj->pos);

    switch (fb->fireball_info_index) {
    case FIREBALL_EXPLOSION_MEDIUM:
        gr_set_bitmap(Fireballs[num].current_bitmap, GR_ALPHABLEND_FILTER,
                      GR_BITBLT_MODE_NORMAL, 1.3f);
        g3_draw_bitmap(&p, fb->orient, obj->radius, TMAP_FLAG_TEXTURED);
        break;

    case FIREBALL_EXPLOSION_LARGE1:
    case FIREBALL_EXPLOSION_LARGE2:
        // case FIREBALL_EXPLOSION_LARGE3:

    case FIREBALL_ASTEROID:
        // Make the big explosions rotate with the viewer.
        gr_set_bitmap(Fireballs[num].current_bitmap, GR_ALPHABLEND_FILTER,
                      GR_BITBLT_MODE_NORMAL, 1.3f);
        g3_draw_rotated_bitmap(&p, (i2fl(fb->orient) * PI) / 180.0f, obj->radius,
                               TMAP_FLAG_TEXTURED);
        break;

    case FIREBALL_WARP_EFFECT:
    case FIREBALL_KNOSSOS_EFFECT: {
        float percent_life = fb->time_elapsed / fb->total_time;

        float rad;

        // Code to make effect grow/shrink.
        float t = fb->time_elapsed;

        if (t < WARPHOLE_GROW_TIME) {
            rad = (float)pow(t / WARPHOLE_GROW_TIME, 0.4f) * obj->radius;
            //rad = t*obj->radius/WARPHOLE_GROW_TIME;
            //mprintf(( "T=%.2f, Rad = %.2f\n", t, rad ));
        }
        else if (t < fb->total_time - WARPHOLE_GROW_TIME) {
            rad = obj->radius;
        }
        else {
            rad = (float)pow((fb->total_time - t) / WARPHOLE_GROW_TIME, 0.4f) *
                  obj->radius;
            //rad = (fb->total_time - t )*obj->radius/WARPHOLE_GROW_TIME;
        }
        //rad = obj->radius;

        warpin_render(&obj->orient, &obj->pos, Fireballs[num].current_bitmap, rad,
                      percent_life, obj->radius);
    } break;

    default:
        Int3();
    }
}

// -----------------------------------------------------------------
// fireball_delete()
//
// Delete a fireball.  Called by object_delete() code... do not call
// directly.
//
void
fireball_delete(object *obj)
{
    int num;
    fireball *fb;

    num = obj->instance;
    fb = &Fireballs[num];

    Assert(fb->objnum == OBJ_INDEX(obj));

    Fireballs[num].objnum = -1;
    Num_fireballs--;
    Assert(Num_fireballs >= 0);
}

// -----------------------------------------------------------------
// fireball_delete_all()
//
// Delete all active fireballs, by calling obj_delete directly.
//
void
fireball_delete_all()
{
    fireball *fb;
    int i;

    for (i = 0; i < MAX_FIREBALLS; i++) {
        fb = &Fireballs[i];
        if (fb->objnum != -1) {
            obj_delete(fb->objnum);
        }
    }
}

void
fireball_set_framenum(int num)
{
    int framenum;
    fireball *fb;
    fireball_info *fd;
    fireball_lod *fl;

    fb = &Fireballs[num];
    fd = &Fireball_info[Fireballs[num].fireball_info_index];

    // valid lod?
    fl = NULL;
    if ((fb->lod >= 0) && (fb->lod < fd->lod_count)) {
        fl = &Fireball_info[Fireballs[num].fireball_info_index].lod[(int)fb->lod];
    }
    if (fl == NULL) {
        // argh
        return;
    }

    if (fb->fireball_info_index == FIREBALL_WARP_EFFECT ||
        fb->fireball_info_index == FIREBALL_KNOSSOS_EFFECT) {
        float total_time = i2fl(fl->num_frames) / fl->fps; // in seconds

        framenum = fl2i(fb->time_elapsed * fl->num_frames / total_time + 0.5);

        if (framenum < 0)
            framenum = 0;

        framenum = framenum % fl->num_frames;

        if (fb->orient) {
            // warp out effect plays backwards
            framenum = fl->num_frames - framenum - 1;
            fb->current_bitmap = fl->bitmap_id + framenum;
        }
        else {
            fb->current_bitmap = fl->bitmap_id + framenum;
        }
    }
    else {
        framenum = fl2i(fb->time_elapsed / fb->total_time * fl->num_frames + 0.5);

        // ensure we don't go past the number of frames of animation
        if (framenum > (fl->num_frames - 1)) {
            framenum = (fl->num_frames - 1);
            Objects[fb->objnum].flags |= OF_SHOULD_BE_DEAD;
        }

        if (framenum < 0)
            framenum = 0;
        fb->current_bitmap = fl->bitmap_id + framenum;
    }
}

int
fireball_is_perishable(object *obj)
{
    //   return 1;
    int num, objnum;
    fireball *fb;

    num = obj->instance;
    objnum = OBJ_INDEX(obj);
    Assert(Fireballs[num].objnum == objnum);

    fb = &Fireballs[num];

    if (fb->fireball_info_index == FIREBALL_EXPLOSION_MEDIUM)
        return 1;

    if (!((fb->fireball_info_index == FIREBALL_WARP_EFFECT) ||
          (fb->fireball_info_index == FIREBALL_KNOSSOS_EFFECT))) {
        if (!(obj->flags & OF_WAS_RENDERED)) {
            return 1;
        }
    }

    return 0;
}

// -----------------------------------------------------------------
// fireball_free_one()
//
// There are too many fireballs, so delete the oldest small one
// to free up a slot.  Returns the fireball slot freed.
//
int
fireball_free_one()
{
    fireball *fb;
    int i;

    int oldest_objnum = -1, oldest_slotnum = -1;
    float lifeleft, oldest_lifeleft = 0.0f;

    for (i = 0; i < MAX_FIREBALLS; i++) {
        fb = &Fireballs[i];

        // only remove the ones that aren't warp effects
        if ((fb->objnum > -1) && fireball_is_perishable(&Objects[fb->objnum])) {
            lifeleft = fb->total_time - fb->time_elapsed;
            if ((oldest_objnum < 0) || (lifeleft < oldest_lifeleft)) {
                oldest_slotnum = i;
                oldest_lifeleft = lifeleft;
                oldest_objnum = fb->objnum;
            }
            break;
        }
    }

    if (oldest_objnum > -1) {
        obj_delete(oldest_objnum);
    }
    return oldest_slotnum;
}

// broke fireball_move into fireball_process_pre and fireball_process_post as was done
// with all *_move functions on 8/13 by Mike K. and Mark A.
void
fireball_process_pre(object *objp, float frame_time)
{ }

int
fireball_is_warp(object *obj)
{
    int num, objnum;
    fireball *fb;

    num = obj->instance;
    objnum = OBJ_INDEX(obj);
    Assert(Fireballs[num].objnum == objnum);

    fb = &Fireballs[num];

    if (fb->fireball_info_index == FIREBALL_WARP_EFFECT ||
        fb->fireball_info_index == FIREBALL_KNOSSOS_EFFECT)
        return 1;

    return 0;
}

// mabye play sound effect for warp hole closing
void
fireball_maybe_play_warp_close_sound(fireball *fb)
{
    float life_left;

    // If not a warphole fireball, do a quick out
    if (!(fb->fireball_info_index == FIREBALL_WARP_EFFECT ||
          fb->fireball_info_index == FIREBALL_KNOSSOS_EFFECT)) {
        return;
    }

    // If the warhole close sound has been played, don't play it again!
    if (fb->flags & FBF_WARP_CLOSE_SOUND_PLAYED) {
        return;
    }

    life_left = fb->total_time - fb->time_elapsed;

    if (life_left < WARPHOLE_GROW_TIME) {
        fireball_play_warphole_close_sound(fb);
        fb->flags |= FBF_WARP_CLOSE_SOUND_PLAYED;
    }
}

MONITOR(NumFireballs);

void
fireball_process_post(object *obj, float frame_time)
{
    int num, objnum;
    fireball *fb;

    MONITOR_INC(NumFireballs, 1);

    num = obj->instance;
    objnum = OBJ_INDEX(obj);
    Assert(Fireballs[num].objnum == objnum);

    fb = &Fireballs[num];

    fb->time_elapsed += frame_time;
    if (fb->time_elapsed > fb->total_time) {
        obj->flags |= OF_SHOULD_BE_DEAD;
    }

    fireball_maybe_play_warp_close_sound(fb);

    fireball_set_framenum(num);
}

// Returns life left of a fireball in seconds
float
fireball_lifeleft(object *obj)
{
    int num, objnum;
    fireball *fb;

    num = obj->instance;
    objnum = OBJ_INDEX(obj);
    Assert(Fireballs[num].objnum == objnum);

    fb = &Fireballs[num];

    return fb->total_time - fb->time_elapsed;
}

// Returns life left of a fireball in percent
float
fireball_lifeleft_percent(object *obj)
{
    int num, objnum;
    fireball *fb;

    num = obj->instance;
    objnum = OBJ_INDEX(obj);
    Assert(Fireballs[num].objnum == objnum);

    fb = &Fireballs[num];

    return (fb->total_time - fb->time_elapsed) / fb->total_time;
}

// determine LOD to use
int
fireball_get_lod(vector *pos, fireball_info *fd, float size)
{
    vertex v;
    int x, y, w, h, bm_size;
    int must_stop = 0;
    int ret_lod = 1;
    int behind = 0;

    // bogus
    if (fd == NULL) {
        return 1;
    }

    // start the frame
    extern float Viewer_zoom;
    extern int G3_count;

    if (!G3_count) {
        g3_start_frame(1);
        must_stop = 1;
    }
    g3_set_view_matrix(&Eye_position, &Eye_matrix, Viewer_zoom);

    // get extents of the rotated bitmap
    g3_rotate_vertex(&v, pos);

    // if vertex is behind, find size if in front, then drop down 1 LOD
    if (v.codes & CC_BEHIND) {
        float dist = vm_vec_dist_quick(&Eye_position, pos);
        vector temp;

        behind = 1;
        vm_vec_scale_add(&temp, &Eye_position, &Eye_matrix.fvec, dist);
        g3_rotate_vertex(&v, &temp);

        // if still behind, bail and go with default
        if (v.codes & CC_BEHIND) {
            behind = 0;
        }
    }

    if (!g3_get_bitmap_dims(fd->lod[0].bitmap_id, &v, size, &x, &y, &w, &h,
                            &bm_size)) {
        if (Detail.hardware_textures == 4) {
            // straight LOD
            if (w <= bm_size / 8) {
                ret_lod = 3;
            }
            else if (w <= bm_size / 2) {
                ret_lod = 2;
            }
            else if (w <= (1.56 * bm_size)) {
                ret_lod = 1;
            }
            else {
                ret_lod = 0;
            }
        }
        else {
            // less aggressive LOD for lower detail settings
            if (w <= bm_size / 8) {
                ret_lod = 3;
            }
            else if (w <= bm_size / 3) {
                ret_lod = 2;
            }
            else if (w <= (1.2 * bm_size)) {
                ret_lod = 1;
            }
            else {
                ret_lod = 0;
            }
        }
    }

    // if it's behind, bump up LOD by 1
    if (behind) {
        ret_lod++;
    }

    // end the frame
    if (must_stop) {
        g3_end_frame();
    }

    // return the best lod
    return min(ret_lod, fd->lod_count - 1);
}

// Create a fireball, return object index.
int
fireball_create(vector *pos, int fireball_type, int parent_obj, float size,
                int reverse, vector *velocity, float warp_lifetime,
                int ship_class, matrix *orient_override, int low_res)
{
    int n, objnum, fb_lod;
    object *obj;
    fireball *fb;
    fireball_info *fd;
    fireball_lod *fl;

    Assert(fireball_type > -1);
    Assert(fireball_type < MAX_FIREBALL_TYPES);

    fd = &Fireball_info[fireball_type];

    if (!(Game_detail_flags & DETAIL_FLAG_FIREBALLS)) {
        if (!((fireball_type == FIREBALL_WARP_EFFECT) ||
              (fireball_type == FIREBALL_KNOSSOS_EFFECT))) {
            return -1;
        }
    }

    if ((Num_fireballs >= MAX_FIREBALLS) || (num_objects >= MAX_OBJECTS)) {
        // who cares if we don't create a spark.
        // JAS - Should this code be in?  Is it better to remove an old spark
        // and start a new one, or just not start the new one?
        //if ( fd->type == FIREBALL_TYPE_SMALL )   {
        //  return -1;
        //}

        //mprintf(( "Out of fireball slots, trying to free one up!\n" ));
        // out of slots, so free one up.
        n = fireball_free_one();
        if (n < 0) {
            // If there's still no free slots, then exit
            //mprintf(( "ERROR: Couldn't free one up!!\n" ));
            return -1;
        }
        else {
            //mprintf(( "Freed one up just fine!!\n" ));
        }
    }
    else {
        for (n = 0; n < MAX_FIREBALLS; n++) {
            if (Fireballs[n].objnum < 0) {
                break;
            }
        }
        Assert(n != MAX_FIREBALLS);
    }

    fb = &Fireballs[n];

    // get an lod to use
    fb_lod = fireball_get_lod(pos, fd, size);

    // change lod if low res is desired
    if (low_res) {
        fb_lod++;
        fb_lod = min(fb_lod, fd->lod_count - 1);
    }

    // if this is a warpout fireball, never go higher than LOD 1
    if (fireball_type == FIREBALL_WARP_EFFECT) {
        /*
      if(fb_lod > 1){
         fb_lod = 1;
      }
      */
        fb_lod = 0;
    }
    fl = &fd->lod[fb_lod];

    fb->lod = (char)fb_lod;
    fb->flags = 0;
    matrix orient;
    if (orient_override != NULL) {
        orient = *orient_override;
    }
    else {
        if (parent_obj < 0) {
            orient = vmd_identity_matrix;
        }
        else {
            orient = Objects[parent_obj].orient;
        }
    }

    objnum = obj_create(OBJ_FIREBALL, parent_obj, n, &orient, pos, size,
                        OF_RENDERS);

    if (objnum < 0) {
        Int3(); // Get John, we ran out of objects for fireballs
        return objnum;
    }

    obj = &Objects[objnum];

    fb->fireball_info_index = fireball_type;
    fb->time_elapsed = 0.0f;
    fb->objnum = objnum;
    fb->current_bitmap = -1;

    switch (fb->fireball_info_index) {
    case FIREBALL_EXPLOSION_MEDIUM:
        fb->orient = (myrand() >> 8) & 7; // 0 - 7
        break;

    case FIREBALL_EXPLOSION_LARGE1:
    case FIREBALL_EXPLOSION_LARGE2:
    // case FIREBALL_EXPLOSION_LARGE3:
    case FIREBALL_ASTEROID:
        fb->orient = (myrand() >> 8) % 360; // 0 - 359
        break;

    case FIREBALL_WARP_EFFECT:
    case FIREBALL_KNOSSOS_EFFECT:
        // Play sound effect for warp hole opening up
        fireball_play_warphole_open_sound(ship_class, fb);

        // warp in type
        if (reverse) {
            fb->orient = 1;
            // if warp out, then reverse the orientation
            vm_vec_scale(&obj->orient.fvec, -1.0f); // Reverse the forward vector
            vm_vec_scale(&obj->orient.rvec, -1.0f); // Reverse the right vector
        }
        else {
            fb->orient = 0;
        }
        break;

    default:
        Int3();
        break;
    }

    if (fb->fireball_info_index == FIREBALL_WARP_EFFECT ||
        fb->fireball_info_index == FIREBALL_KNOSSOS_EFFECT) {
        Assert(warp_lifetime > 4.0f); // Warp lifetime must be at least 4 seconds!
        fb->total_time = warp_lifetime; // in seconds
    }
    else {
        fb->total_time = i2fl(fl->num_frames) / fl->fps; // in seconds
    }

    fireball_set_framenum(n);

    if (velocity) {
        // Make the explosion move at a constant velocity.
        obj->flags |= OF_PHYSICS;
        obj->phys_info.mass = 1.0f;
        obj->phys_info.side_slip_time_const = 0.0f;
        obj->phys_info.rotdamp = 0.0f;
        obj->phys_info.vel = *velocity;
        obj->phys_info.max_vel = *velocity;
        obj->phys_info.desired_vel = *velocity;
        obj->phys_info.speed = vm_vec_mag(velocity);
        vm_vec_zero(&obj->phys_info.max_rotvel);
    }

    Num_fireballs++;
    return objnum;
}

// -----------------------------------------------------------------
// fireball_close()
//
// Called at game shutdown to clean up the fireball system
//
void
fireball_close()
{
    if (!fireballs_inited)
        return;

    fireball_delete_all();
}

// -----------------------------------------------------------------
// fireball_level_close()
//
// Called when a mission ends... frees up any animations that might
// be partially played
//
void
fireball_level_close()
{
    if (!fireballs_inited)
        return;

    fireball_delete_all();
}

void
fireballs_page_in()
{
    int i, idx;
    fireball_info *fd;

    for (i = 0; i < MAX_FIREBALL_TYPES; i++) {
        fd = &Fireball_info[i];

        for (idx = 0; idx < fd->lod_count; idx++) {
            bm_page_in_texture(fd->lod[idx].bitmap_id, fd->lod[idx].num_frames);
        }
    }

    bm_page_in_texture(Warp_glow_bitmap);
}
