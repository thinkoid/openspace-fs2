/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#include <globalincs/pstypes.hh>
#include <globalincs/systemvars.hh>
#include <cmeasure/cmeasure.hh>
#include <freespace2/freespace.hh>
#include <math/vecmat.hh>
#include <graphics/2d.hh>
#include <render/3d.hh>
#include <model/model.hh>
#include <physics/physics.hh>
#include <math/floating.hh>
#include <model/model.hh>
#include <ship/ship.hh>
#include <io/timer.hh>
#include <fireball/fireballs.hh>
#include <radar/radar.hh>
#include <mission/missionparse.hh> // For MAX_SPECIES_NAMES
#include <gamesnd/gamesnd.hh>
#include <object/objectsnd.hh>
#include <sound/sound.hh>
#include <math/staticrand.hh>

cmeasure_info Cmeasure_info[MAX_CMEASURE_TYPES];
cmeasure Cmeasures[MAX_CMEASURES];

int Num_cmeasure_types = 0;
int Num_cmeasures = 0;
int Cmeasure_inited = 0;
int Cmeasures_homing_check = 0;
int Countermeasures_enabled =
    1; //       Debug, set to 0 means no one can fire countermeasures.

// This will get called at the start of each level.
void
cmeasure_init()
{
    int i;

    if (!Cmeasure_inited) {
        Cmeasure_inited = 1;

        /*              // Do all the processing that happens only once
                if ( Debris_model < 0 )         {
                        if (Debris_model>-1)    {
                                polymodel * pm;
                                pm = model_get(Debris_model);
                                Debris_num_submodels = pm->n_models;
                        }
                }

                for (i=0; i<MAX_SPECIES_NAMES; i++ )    {
                        Debris_textures[i] = bm_load( Debris_texture_files[i] );
                        if ( Debris_textures[i] < 0 ) { 
                                Warning( LOCATION, "Couldn't load species %d debris\ntexture, '%s'\n", i, Debris_texture_files[i] );
                        }
                }
*/
    }

    // Reset everything between levels
    Num_cmeasures = 0;

    for (i = 0; i < MAX_CMEASURES; i++) {
        Cmeasures[i].subtype = CMEASURE_UNUSED;
    }
}

void
cmeasure_render(object *objp)
{
    // JAS TODO: Replace with proper fireball
    cmeasure *cmp;
    cmeasure_info *cmip;

    cmp = &Cmeasures[objp->instance];
    cmip = &Cmeasure_info[cmp->subtype];

    if (cmp->subtype == CMEASURE_UNUSED) {
        Int3(); //      Hey, what are we doing in here?
        return;
    }

    //  float                           size = -1.0f;
    //  vertex                  p;
    //  g3_rotate_vertex(&p, &objp->pos );
    //  if ( rand() > RAND_MAX/2 )      {
    //          gr_set_color( 255, 0, 0 );
    //  } else {
    //          gr_set_color( 255, 255, 255 );
    //  }
    //  g3_draw_sphere(&p, 100.0f );

    if (cmip->model_num > -1) {
        model_clear_instance(cmip->model_num);
        model_render(cmip->model_num, &objp->orient, &objp->pos, MR_NO_LIGHTING);
    }
    else {
        mprintf(("Not rendering countermeasure because model_num is negative\n"));
    }

    /*
        // JAS TODO: Replace with proper fireball
        int                             framenum = -1;
        float                           size = -1.0f;
        vertex                  p;
        cmeasure                        *cmp;

        fireball_data   *fd;

        cmp = &Cmeasures[objp->instance];
        fd = &Fireball_data[FIREBALL_SHIP_EXPLODE1];

        switch (cmp->subtype) {
        case CMEASURE_UNUSED:
                Int3(); //      Hey, what are we doing in here?
                break;
        default:
                framenum = (int) (fd->num_frames * Cmeasures[objp->instance].lifeleft*4) % fd->num_frames;
                size = objp->radius;
                break;
        }

        Assert(framenum != -1);
        Assert(size != -1.0f);

        gr_set_bitmap(fd->bitmap_id + framenum);
        g3_rotate_vertex(&p, &objp->pos );
        g3_draw_bitmap(&p, 0, size*0.5f, TMAP_FLAG_TEXTURED );
*/
}

void
cmeasure_delete(object *objp)
{
    int num;

    num = objp->instance;

    //  Assert( Cmeasures[num].objnum == OBJ_INDEX(objp));

    Cmeasures[num].subtype = CMEASURE_UNUSED;
    Num_cmeasures--;
    Assert(Num_cmeasures >= 0);
}

// broke cmeasure_move into two functions -- process_pre and process_post (as was done with
// all *_move functions).  Nothing to do for process_pre

void
cmeasure_process_pre(object *objp, float frame_time)
{ }

void
cmeasure_process_post(object *objp, float frame_time)
{
    int num;
    num = objp->instance;

    //  Assert( Cmeasures[num].objnum == objnum );
    cmeasure *cmp = &Cmeasures[num];

    if (cmp->lifeleft >= 0.0f) {
        cmp->lifeleft -= frame_time;
        if (cmp->lifeleft < 0.0f) {
            objp->flags |= OF_SHOULD_BE_DEAD;
            //                  demo_do_flag_dead(OBJ_INDEX(objp));
        }
    }
}

float Skill_level_cmeasure_life_scale[NUM_SKILL_LEVELS] = { 3.0f, 2.0f, 1.5f,
                                                            1.25f, 1.0f };

// creates one countermeasure.  A ship fires 1 of these per launch.  rand_val is used
// in multiplayer.  If -1, then create a random number.  If non-negative, use this
// number for static_rand functions
int
cmeasure_create(object *source_obj, vector *pos, int cm_type, int rand_val)
{
    int n, objnum, parent_objnum, arand;
    object *obj;
    ship *shipp;
    cmeasure *cmp;
    cmeasure_info *cmeasurep;

#ifndef NDEBUG
    if (!Countermeasures_enabled || !Ai_firing_enabled)
        return -1;
#endif

    Cmeasures_homing_check =
        2; //   Tell homing code to scan everything for two frames.  If only one frame, get sync problems due to objects being created at end of frame!

    parent_objnum = OBJ_INDEX(source_obj);

    Assert(source_obj->type == OBJ_SHIP);
    Assert(source_obj->instance >= 0 && source_obj->instance < MAX_SHIPS);

    shipp = &Ships[source_obj->instance];

    if (Num_cmeasures >= MAX_CMEASURES)
        return -1;

    for (n = 0; n < MAX_CMEASURES; n++)
        if (Cmeasures[n].subtype == CMEASURE_UNUSED)
            break;
    if (n == MAX_CMEASURES)
        return -1;

    nprintf(("Network", "Cmeasure created by %s\n",
             Ships[source_obj->instance].ship_name));

    cmp = &Cmeasures[n];
    cmeasurep = &Cmeasure_info[cm_type];

    if (pos == NULL)
        pos = &source_obj->pos;

    objnum = obj_create(OBJ_CMEASURE, parent_objnum, n, &source_obj->orient, pos,
                        1.0f, OF_RENDERS | OF_PHYSICS);

    Assert(objnum >= 0 && objnum < MAX_OBJECTS);

    // Create Debris piece n!
    if (rand_val == -1)
        arand =
            myrand(); // use a random number to get lifeleft, and random vector for displacement from ship
    else
        arand = rand_val;

    cmp->lifeleft = static_randf(arand) *
                    (cmeasurep->life_max - cmeasurep->life_min) /
                    cmeasurep->life_min;
    if (source_obj->flags & OF_PLAYER_SHIP) {
        cmp->lifeleft *= Skill_level_cmeasure_life_scale[Game_skill_level];
    }
    cmp->lifeleft = cmeasurep->life_min +
                    cmp->lifeleft * (cmeasurep->life_max - cmeasurep->life_min);

    //  cmp->objnum = objnum;
    cmp->team = shipp->team;
    cmp->subtype = cm_type;
    cmp->objnum = objnum;
    cmp->source_objnum = parent_objnum;
    cmp->source_sig = Objects[objnum].signature;

    cmp->flags = 0;

    nprintf(("Jim", "Frame %i: Launching countermeasure #%i\n", Framecount,
             Objects[objnum].signature));

    obj = &Objects[objnum];

    Num_cmeasures++;

    vector vel, rand_vec;

    vm_vec_scale_add(&vel, &source_obj->phys_info.vel, &source_obj->orient.fvec,
                     -25.0f);

    static_randvec(arand + 1, &rand_vec);

    vm_vec_scale_add2(&vel, &rand_vec, 2.0f);

    obj->phys_info.vel = vel;

    vm_vec_zero(&obj->phys_info.rotvel);

    // blow out his reverse thrusters. Or drag, same thing.
    obj->phys_info.rotdamp = 10000.0f;
    obj->phys_info.side_slip_time_const = 10000.0f;

    vm_vec_zero(
        &obj->phys_info
             .max_vel); // make so he can't turn on his own VOLITION anymore.
    obj->phys_info.max_vel.z = -25.0f;
    vm_vec_copy_scale(&obj->phys_info.desired_vel, &obj->orient.fvec,
                      obj->phys_info.max_vel.z);

    vm_vec_zero(
        &obj->phys_info
             .max_rotvel); // make so he can't change speed on his own VOLITION anymore.

    //  obj->phys_info.flags |= PF_USE_VEL;

    return arand; // need to return this value for multiplayer purposes
}

void
cmeasure_select_next(object *objp)
{
    ship *shipp;

    Assert(objp->type == OBJ_SHIP);

    shipp = &Ships[objp->instance];
    shipp->current_cmeasure++;

    if (shipp->current_cmeasure >= Num_cmeasure_types)
        shipp->current_cmeasure = 0;

    //snd_play( &Snds[SND_CMEASURE_CYCLE] );

    mprintf(("Countermeasure type set to %i in frame %i\n",
             shipp->current_cmeasure, Framecount));
}
