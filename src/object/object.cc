/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#include <string.h> // for memset
#include <stdio.h>

#include <globalincs/pstypes.hh>
#include <object/object.hh>
#include <weapon/weapon.hh>
#include <ship/ship.hh>
#include <fireball/fireballs.hh>
#include <debris/debris.hh>
#include <math/fvi.hh>
#include <physics/physics.hh>
#include <model/model.hh>
#include <globalincs/linklist.hh>
#include <freespace2/freespace.hh>
#include <ship/ai.hh>
#include <object/objectsnd.hh>
#include <gamesnd/gamesnd.hh>
#include <playerman/player.hh>
#include <cmeasure/cmeasure.hh>
#include <io/timer.hh>
#include <globalincs/systemvars.hh>
#include <render/3d.hh>
#include <weapon/shockwave.hh>
#include <ship/afterburner.hh>
#include <io/timer.hh>
#include <object/objcollide.hh>
#include <lighting/lighting.hh>
#include <observer/observer.hh>
#include <asteroid/asteroid.hh>
#include <radar/radar.hh>
#include <jumpnode/jumpnode.hh>
#include <nebula/neb.hh>
#include <weapon/beam.hh>
#include <weapon/swarm.hh>

/*
 *  Global variables
 */

list_t< object > obj_free_list;
list_t< object > obj_used_list;
list_t< object > obj_create_list;

object *Player_obj = NULL;
object *Viewer_obj = NULL;

//Data for objects
object Objects[MAX_OBJECTS];

#ifdef OBJECT_CHECK
typedef struct checkobject
{
    int type;
    int signature;
    uint flags;
    int parent_sig;
    int parent_type;
} checkobject;
checkobject CheckObjects[MAX_OBJECTS];
#endif

int num_objects = -1;
int Highest_object_index = -1;
int Highest_ever_object_index = 0;
int Object_next_signature = 1; //0 is bogus, start at 1
int Object_next_ship_signature = OBJECT_SIG_SHIP_START;
int Object_inited = 0;
int Show_waypoints = 0;

#ifndef NDEBUG
char *Object_type_names[MAX_OBJECT_TYPES] = {
    //XSTR:OFF
    "NONE",       "SHIP",     "WEAPON",   "FIREBALL",  "START",     "WAYPOINT",
    "DEBRIS",     "CMEASURE", "GHOST",    "POINT",     "SHOCKWAVE", "WING",
    "GHOST SAVE", "OBSERVER", "ASTEROID", "JUMP NODE",
    //XSTR:ON
};
#endif

//-----------------------------------------------------------------------------
// Scan the object list, freeing down to num_used objects
// Returns number of slots freed.
int
free_object_slots(int num_used)
{
    int i, olind, deleted_weapons;
    int obj_list[MAX_OBJECTS];
    int num_already_free, num_to_free, original_num_to_free;
    object *objp;

    olind = 0;

    // calc num_already_free by walking the obj_free_list
    num_already_free = 0;
    for (objp = GET_FIRST(&obj_free_list); objp != END_OF_LIST(&obj_free_list);
         objp = GET_NEXT(objp))
        num_already_free++;

    if (MAX_OBJECTS - num_already_free < num_used)
        return 0;

    for (objp = GET_FIRST(&obj_used_list); objp != END_OF_LIST(&obj_used_list);
         objp = GET_NEXT(objp)) {
        if (objp->flags & OF_SHOULD_BE_DEAD) {
            num_already_free++;
            if (MAX_OBJECTS - num_already_free < num_used)
                return num_already_free;
        }
        else
            switch (objp->type) {
            case OBJ_NONE:
                num_already_free++;
                if (MAX_OBJECTS - num_already_free < num_used)
                    return 0;
                break;
            case OBJ_FIREBALL:
            case OBJ_WEAPON:
            case OBJ_DEBRIS:
            case OBJ_CMEASURE:
                obj_list[olind++] = OBJ_INDEX(objp);
                break;

            case OBJ_GHOST:
            case OBJ_SHIP:
            case OBJ_START:
            case OBJ_WAYPOINT:
            case OBJ_POINT:
            case OBJ_SHOCKWAVE:
            case OBJ_WING:
            case OBJ_OBSERVER:
            case OBJ_ASTEROID:
            case OBJ_JUMP_NODE:
            case OBJ_BEAM:
                break;
            default:
                Int3(); // Hey, what kind of object is this?  Unknown!
                break;
            }
    }

    num_to_free = MAX_OBJECTS - num_used - num_already_free;
    original_num_to_free = num_to_free;

    if (num_to_free > olind) {
        nprintf(("allender",
                 "Warning: Asked to free %i objects, but can only free %i.\n",
                 num_to_free, olind));
        num_to_free = olind;
    }

    for (i = 0; i < num_to_free; i++)
        if ((Objects[obj_list[i]].type == OBJ_DEBRIS) &&
            (Debris[Objects[obj_list[i]].instance].flags & DEBRIS_EXPIRE)) {
            num_to_free--;
            nprintf(("allender", "Freeing   DEBRIS object %3i\n", obj_list[i]));
            Objects[obj_list[i]].flags |= OF_SHOULD_BE_DEAD;
        }

    if (!num_to_free)
        return original_num_to_free;

    //JAS - I removed this because small fireballs are now particles, which aren't objects.
    //JAS   for (i=0; i<num_to_free; i++)
    //JAS      if ( (Objects[obj_list[i]].type == OBJ_FIREBALL) && (Fireball_data[Objects[obj_list[i]].instance].type == FIREBALL_TYPE_SMALL) ) {
    //JAS         num_to_free--;
    //JAS         nprintf(("allender", "Freeing FIREBALL object %3i\n", obj_list[i]));
    //JAS         Objects[obj_list[i]].flags |= OF_SHOULD_BE_DEAD;
    //JAS      }
    //JAS
    //JAS   if (!num_to_free)
    //JAS      return original_num_to_free;

    for (i = 0; i < num_to_free; i++) {
        object *tmp_obj = &Objects[obj_list[i]];
        if ((tmp_obj->type == OBJ_FIREBALL) &&
            (fireball_is_perishable(tmp_obj))) {
            num_to_free--;
            nprintf(("allender", "Freeing FIREBALL object %3i\n", obj_list[i]));
            tmp_obj->flags |= OF_SHOULD_BE_DEAD;
        }
    }

    if (!num_to_free) {
        return original_num_to_free;
    }

    deleted_weapons = collide_remove_weapons();
    num_to_free -= deleted_weapons;
    if (!num_to_free) {
        return original_num_to_free;
    }

    for (i = 0; i < num_to_free; i++) {
        if (Objects[obj_list[i]].type == OBJ_WEAPON) {
            num_to_free--;
            Objects[obj_list[i]].flags |= OF_SHOULD_BE_DEAD;
        }
    }

    if (!num_to_free) {
        return original_num_to_free;
    }

    return original_num_to_free - num_to_free;
}

float
get_shield_strength(object *objp)
{
    int i;
    float strength;

    strength = 0.0f;

    // no shield system, no strength!
    if (objp->flags & OF_NO_SHIELDS) {
        return strength;
    }

    for (i = 0; i < MAX_SHIELD_SECTIONS; i++) {
        strength += objp->shields[i];
    }

    return strength;
}

void
set_shield_strength(object *objp, float strength)
{
    int i;

    if ((strength - Ship_info[Ships[objp->instance].ship_info_index].shields) >
        0.1) {
        Int3();
    }

    for (i = 0; i < MAX_SHIELD_SECTIONS; i++) {
        objp->shields[i] = strength / MAX_SHIELD_SECTIONS;
    }
}

// Recharge whole shield.
// Apply delta/MAX_SHIELD_SECTIONS to each shield section.
void
add_shield_strength(object *objp, float delta)
{
    int i;
    float section_max;

    section_max = Ship_info[Ships[objp->instance].ship_info_index].shields /
                  MAX_SHIELD_SECTIONS;

    for (i = 0; i < MAX_SHIELD_SECTIONS; i++) {
        objp->shields[i] += delta / MAX_SHIELD_SECTIONS;
        if (objp->shields[i] > section_max)
            objp->shields[i] = section_max;
        else if (objp->shields[i] < 0.0f)
            objp->shields[i] = 0.0f;
    }
}

//sets up the free list & init player & whatever else
void
obj_init()
{
    int i, idx;
    object *objp;

    Object_inited = 1;
    memset(Objects, 0, sizeof(object) * MAX_OBJECTS);
    Viewer_obj = NULL;

    list_init(&obj_free_list);
    list_init(&obj_used_list);
    list_init(&obj_create_list);

    // Link all object slots into the free list
    objp = Objects;
    for (i = 0; i < MAX_OBJECTS; i++) {
        objp->type = OBJ_NONE;
        objp->signature = i + 100;

        // zero all object sounds
        for (idx = 0; idx < MAX_OBJECT_SOUNDS; idx++) {
            objp->objsnd_num[idx] = -1;
        }

        list_append(&obj_free_list, objp);
        objp++;
    }

    Object_next_signature = 1; //0 is invalid, others start at 1
    Object_next_ship_signature = OBJECT_SIG_SHIP_START;
    num_objects = 0;
    Highest_object_index = 0;

    obj_reset_pairs();
}

int num_objects_hwm = 0;

//returns the number of a free object, updating Highest_object_index.
//Generally, obj_create() should be called to get an object, since it
//fills in important fields and does the linking.
//returns -1 if no free objects
int
obj_allocate(void)
{
    int objnum;
    object *objp;

    if (!Object_inited)
        obj_init();

    if (num_objects >= MAX_OBJECTS - 10) {
        int num_freed;

        num_freed = free_object_slots(MAX_OBJECTS - 10);
        nprintf(("warning", " *** Freed %i objects\n", num_freed));
    }

    if (num_objects >= MAX_OBJECTS) {
#ifndef NDEBUG
        mprintf(("Object creation failed - too many objects!\n"));
#endif
        return -1;
    }

    // Find next available object
    objp = GET_FIRST(&obj_free_list);
    Assert(objp !=
           END_OF_LIST(&obj_free_list)); // shouldn't have the dummy element

    // remove objp from the free list
    list_remove(objp);

    // insert objp onto the end of create list
    list_append(&obj_create_list, objp);

    // increment counter
    num_objects++;

    if (num_objects > num_objects_hwm) {
        //nprintf(("AI", "*** MAX Num Objects = %i\n", num_objects));
        num_objects_hwm = num_objects;
    }

    // get objnum
    objnum = OBJ_INDEX(objp);

    if (objnum > Highest_object_index) {
        Highest_object_index = objnum;
        if (Highest_object_index > Highest_ever_object_index)
            Highest_ever_object_index = Highest_object_index;
    }

    return objnum;
}

//frees up an object.  Generally, obj_delete() should be called to get
//rid of an object.  This function deallocates the object entry after
//the object has been unlinked
void
obj_free(int objnum)
{
    object *objp;

    if (!Object_inited)
        obj_init();

    Assert(objnum >= 0); // Trying to free bogus object!!!

    // get object pointer
    objp = &Objects[objnum];

    // remove objp from the used list
    list_remove(objp);

    // add objp to the end of the free
    list_append(&obj_free_list, objp);

    // decrement counter
    num_objects--;

    Objects[objnum].type = OBJ_NONE;

    Assert(num_objects >= 0);

    if (objnum == Highest_object_index)
        while (Objects[--Highest_object_index].type == OBJ_NONE)
            ;
}

//initialize a new object.  adds to the list for the given segment.
//returns the object number.  The object will be a non-rendering, non-physics
//object.   Pass -1 if no parent.
int
obj_create(ubyte type, int parent_obj, int instance, matrix *orient, vector *pos,
           float radius, uint flags)
{
    int objnum, idx;
    object *obj;

    // Find next free object
    objnum = obj_allocate();

    if (objnum == -1) //no free objects
        return -1;

    obj = &Objects[objnum];
    Assert(obj->type == OBJ_NONE); //make sure unused

    // Zero out object structure to keep weird bugs from happening
    // in uninitialized fields.
    //   memset( obj, 0, sizeof(object) );

    if (obj->type == OBJ_SHIP) {
        obj->signature = Object_next_ship_signature++;

        if (!Object_next_ship_signature) {
            Object_next_ship_signature = OBJECT_SIG_SHIP_START; // 0 is bogus!
        }
    }
    else {
        obj->signature = Object_next_signature++;

        if (!Object_next_signature) {
            Object_next_signature = 1; // 0 is bogus!
        }
    }

    obj->type = type;
    obj->instance = instance;
    obj->parent = parent_obj;
    if (obj->parent != -1) {
        obj->parent_sig = Objects[parent_obj].signature;
        obj->parent_type = Objects[parent_obj].type;
    }
    else {
        obj->parent_sig = obj->signature;
        obj->parent_type = obj->type;
    }

    obj->flags = flags | OF_NOT_IN_COLL;
    if (pos) {
        obj->pos = *pos;
        obj->last_pos = *pos;
    }

    obj->orient = orient ? *orient : vmd_identity_matrix;
    obj->last_orient = obj->orient;
    obj->radius = radius;

    obj->flags &= ~OF_INVULNERABLE; // Make vulnerable.
    physics_init(&obj->phys_info);

    for (idx = 0; idx < MAX_OBJECT_SOUNDS; idx++) {
        obj->objsnd_num[idx] = -1;
    }
    obj->num_pairs = 0;
    obj->net_signature =
        0; // be sure to reset this value so new objects don't take on old signatures.

    return objnum;
}

//remove object from the world
// If Player_obj, don't remove it!
void
obj_delete(int objnum)
{
    object *objp;

    Assert(objnum >= 0 && objnum < MAX_OBJECTS);
    objp = &Objects[objnum];
    Assert(objp->type != OBJ_NONE);

    // Remove all object pairs
    obj_remove_pairs(objp);

    switch (objp->type) {
    case OBJ_WEAPON:
        weapon_delete(objp);
        break;
    case OBJ_SHIP:
        if ((objp == Player_obj) && !Fred_running) {
            objp->type = OBJ_GHOST;
            objp->flags &= ~(OF_SHOULD_BE_DEAD);

            // we have to traverse the ship_obj list and remove this guy from it as well
            ship_obj *moveup = GET_FIRST(&Ship_obj_list);
            while (moveup != END_OF_LIST(&Ship_obj_list)) {
                if (OBJ_INDEX(objp) == moveup->objnum) {
                    list_remove(moveup);
                    break;
                }
                moveup = GET_NEXT(moveup);
            }

            physics_init(&objp->phys_info);

            obj_snd_delete(OBJ_INDEX(objp));
            return;
        }
        else
            ship_delete(objp);
        break;
    case OBJ_FIREBALL:
        fireball_delete(objp);
        break;
    case OBJ_SHOCKWAVE:
        shockwave_delete(objp);
        break;
    case OBJ_START:
    case OBJ_WAYPOINT:
    case OBJ_POINT:
    case OBJ_JUMP_NODE:
        Assert(Fred_running);
        break; // requires no action, handled by the Fred code.
    case OBJ_DEBRIS:
        debris_delete(objp);
        break;
    case OBJ_ASTEROID:
        asteroid_delete(objp);
        break;
    case OBJ_CMEASURE:
        cmeasure_delete(objp);
        break;
    case OBJ_GHOST:
        // retail guarded this with (!Game_mode & GM_MULTIPLAYER) -- a precedence
        // bug that made the warn-and-refuse branch unreachable, so the shipped
        // behavior was to always delete ghosts.  Keep the shipped behavior.
        break;
    case OBJ_OBSERVER:
        observer_delete(objp);
        break;
    case OBJ_BEAM:
        break;
    case OBJ_NONE:
        Int3();
        break;
    default:
        Error(LOCATION,
              "Unhandled object type %d in obj_delete_all_that_should_be_dead",
              objp->type);
    }

    // if a persistant sound has been created, delete it
    obj_snd_delete(OBJ_INDEX(objp));

    objp->type = OBJ_NONE; //unused!
    objp->signature = 0;

    obj_free(objnum);
}

// ------------------------------------------------------------------------------------------------------------------
void
obj_delete_all_that_should_be_dead()
{
    object *objp, *temp;

    if (!Object_inited)
        obj_init();

    // Move all objects
    objp = GET_FIRST(&obj_used_list);
    while (objp != END_OF_LIST(&obj_used_list)) {
        temp = GET_NEXT(objp);
        if (objp->flags & OF_SHOULD_BE_DEAD)
            obj_delete(OBJ_INDEX(
                objp)); // MWA says that john says that let obj_delete handle everything because of the editor
        objp = temp;
    }
}

// Add all newly created objects to the end of the used list and create their
// object pairs for collision detection
void
obj_merge_created_list(void)
{
    // The old way just merged the two.   This code takes one out of the create list,
    // creates object pairs for it, and then adds it to the used list.
    //   OLD WAY: list_merge( &obj_used_list, &obj_create_list );
    object *objp = GET_FIRST(&obj_create_list);
    while (objp != END_OF_LIST(&obj_create_list)) {
        list_remove(objp);

        // Add it to the object pairs array
        obj_add_pairs(OBJ_INDEX(objp));

        // Then add it to the object used list
        list_append(&obj_used_list, objp);

        objp = GET_FIRST(&obj_create_list);
    }

    // Make sure the create list is empty.
    list_init(&obj_create_list);
}

int physics_paused = 0, ai_paused = 0;

extern void call_doa(object *obj1, object *obj2, ship_info *sip1);

// If this is a cargo container or a repair ship, move it along with the ship it's docked to.
void
move_docked_objects(object *objp)
{
    ai_info *aip;
    ai_info *other_aip;

    if (objp->type != OBJ_SHIP)
        return;

    Assert((objp->instance >= 0) && (objp->instance < MAX_SHIPS));

    aip = &Ai_info[Ships[objp->instance].ai_index];

    if (aip->ai_flags & AIF_DOCKED) {
        ship_info *sip;
        sip = &Ship_info[Ships[objp->instance].ship_info_index];
        if ((sip->flags & SIF_SUPPORT) || (sip->flags & SIF_CARGO)) {
            Assert(!(
                (sip->flags & SIF_SUPPORT) &&
                (sip->flags & SIF_CARGO))); // Ship can't be both repair and cargo
            if (aip->dock_objnum != -1) {
                if (aip->mode == AIM_DOCK) {
                    if (aip->submode < AIS_UNDOCK_1)
                        call_doa(objp, &Objects[aip->dock_objnum], sip);
                }
                else {
                    // if I am not in dock mode then I need to check the guy that I'm docked with
                    // and only move with him if he isn't undocking.
                    other_aip = &Ai_info[Ships[Objects[aip->dock_objnum].instance]
                                             .ai_index];
                    if (other_aip->mode == AIM_DOCK) {
                        if (other_aip->submode < AIS_UNDOCK_1)
                            call_doa(objp, &Objects[aip->dock_objnum], sip);
                    }
                    else {
                        call_doa(objp, &Objects[aip->dock_objnum], sip);
                    }
                }
            }
        }
        else {
            if (aip->dock_objnum != -1) {
                Assert(aip->dock_objnum != -1);
                other_aip =
                    &Ai_info[Ships[Objects[aip->dock_objnum].instance].ai_index];

                // if the other object that I am docked with is undocking, then don't do anything.
                if (!((other_aip->mode == AIM_DOCK) &&
                      (other_aip->submode >= AIS_UNDOCK_1))) {
                    if ((aip->mode != AIM_DOCK) && (aip->mode != AIM_WARP_OUT)) {
                        object *objp1, *objp2;

                        objp1 = &Objects[aip->dock_objnum];
                        objp2 = objp;

                        if (objp1->phys_info.speed > objp2->phys_info.speed) {
                            object *t = objp1;
                            objp1 = objp2;
                            objp2 = t;
                        }

                        //nprintf(("AI", "Calling doa, frame %i: %s, %s\n", Framecount, Ships[objp1->instance].ship_name, Ships[objp2->instance].ship_name));
                        call_doa(objp1, objp2, sip);
                    }
                }
            }
        }
    }
}

/*
float Last_fire_time = 0.0f;
int Avg_delay_count = 0;
float Avg_delay_total;
*/

// function to deal with firing player things like lasers, missiles, etc.
void
obj_player_fire_stuff(object *objp, control_info ci)
{
    ship *shipp;

    Assert(objp->flags & OF_PLAYER_SHIP);

    // try and get the ship pointer
    shipp = NULL;
    if ((objp->type == OBJ_SHIP) && (objp->instance >= 0) &&
        (objp->instance < MAX_SHIPS)) {
        shipp = &Ships[objp->instance];
    }

    if (ci.fire_primary_count) {
        // flag the ship as having the trigger down
        if (shipp != NULL) {
            shipp->flags |= SF_TRIGGER_DOWN;
        }

        // fire non-streaming primaries here
        ship_fire_primary(objp, 0);
    }
    else {
        // unflag the ship as having the trigger down
        if (shipp != NULL) {
            shipp->flags &= ~(SF_TRIGGER_DOWN);
        }
    }

    if (ci.fire_countermeasure_count) {
        ship_launch_countermeasure(objp);
    }

    if (ci.fire_secondary_count) {
        ship_fire_secondary(objp);

        // kill the secondary count
        ci.fire_secondary_count = 0;
    }

    // everyone does the following for their own ships.
    if (ci.afterburner_start) {
        if (ship_get_subsystem_strength(&Ships[objp->instance],
                                        SUBSYSTEM_ENGINE)) {
            afterburners_start(objp);
        }
    }

    if (ci.afterburner_stop) {
        afterburners_stop(objp, 1);
    }
}

void
obj_move_call_physics(object *objp, float frametime)
{
    //   Do physics for objects with OF_PHYSICS flag set and with some engine strength remaining.
    if (objp->flags & OF_PHYSICS) {
        // only set phys info if ship is not dead
        if ((objp->type == OBJ_SHIP) &&
            !(Ships[objp->instance].flags & SF_DYING)) {
            float engine_strength;
            engine_strength = ship_get_subsystem_strength(&Ships[objp->instance],
                                                          SUBSYSTEM_ENGINE);
            if (ship_subsys_disrupted(&Ships[objp->instance], SUBSYSTEM_ENGINE)) {
                engine_strength = 0.0f;
            }

            if (engine_strength ==
                0.0f) { // All this is necessary to make ship gradually come to a stop after engines are blown.
                vm_vec_zero(&objp->phys_info.desired_vel);
                vm_vec_zero(&objp->phys_info.desired_rotvel);
                objp->phys_info.flags |= (PF_REDUCED_DAMP | PF_DEAD_DAMP);
                objp->phys_info.side_slip_time_const =
                    Ship_info[Ships[objp->instance].ship_info_index].damp * 4.0f;
            } // else {
            // DA: comment out lines that resets PF_DEAD_DAMP after every frame.
            // This is now reset during engine repair.
            // objp->phys_info.flags &= ~(PF_REDUCED_DAMP | PF_DEAD_DAMP);
            // objp->phys_info.side_slip_time_const = Ship_info[Ships[objp->instance].ship_info_index].damp;
            // }
        }

        // if a weapon is flagged as dead, kill its engines just like a ship
        if ((objp->type == OBJ_WEAPON) &&
            (Weapons[objp->instance].weapon_flags & WF_DEAD_IN_WATER)) {
            vm_vec_zero(&objp->phys_info.desired_vel);
            vm_vec_zero(&objp->phys_info.desired_rotvel);
            objp->phys_info.flags |= (PF_REDUCED_DAMP | PF_DEAD_DAMP);
            objp->phys_info.side_slip_time_const =
                Ship_info[Ships[objp->instance].ship_info_index].damp * 4.0f;
        }

        if (physics_paused) {
            if (objp == Player_obj) {
                physics_sim(&objp->pos, &objp->orient, &objp->phys_info,
                            frametime); // simulate the physics
            }
        }
        else {
            // Hack for dock mode.
            // If docking with a ship, we don't obey the normal ship physics, we can slew about.
            if (objp->type == OBJ_SHIP) {
                ai_info *aip = &Ai_info[Ships[objp->instance].ai_index];

                //   Note: This conditional for using PF_USE_VEL (instantaneous acceleration) is probably too loose.
                //   A ships awaiting support will fly towards the support ship with instantaneous acceleration.
                //   But we want to have ships in the process of docking have quick acceleration, or they overshoot their goals.
                //   Probably can not key off dock_objnum, but then need to add some other condition.  Live with it for now. -- MK, 2/19/98
                if ((aip->dock_objnum != -1) ||
                    ((aip->mode == AIM_DOCK) &&
                     ((aip->submode == AIS_DOCK_2) ||
                      (aip->submode == AIS_DOCK_3) ||
                      (aip->submode == AIS_UNDOCK_0))) ||
                    ((aip->mode == AIM_WARP_OUT) &&
                     (aip->submode >= AIS_WARP_3))) {
                    if (ship_get_subsystem_strength(&Ships[objp->instance],
                                                    SUBSYSTEM_ENGINE) > 0.0f) {
                        objp->phys_info.flags |= PF_USE_VEL;
                    }
                    else {
                        objp->phys_info.flags &=
                            ~PF_USE_VEL; //  If engine blown, don't PF_USE_VEL, or ships stop immediately
                    }
                }
                else {
                    objp->phys_info.flags &= ~PF_USE_VEL;
                }
            }

            if ((objp->type == OBJ_ASTEROID) && Model_caching) {
                // If we're doing model caching, don't rotate asteroids
                vector tmp = objp->phys_info.rotvel;

                objp->phys_info.rotvel = vmd_zero_vector;
                physics_sim(&objp->pos, &objp->orient, &objp->phys_info,
                            frametime); // simulate the physics
                objp->phys_info.rotvel = tmp;
            }
            else {
                physics_sim(&objp->pos, &objp->orient, &objp->phys_info,
                            frametime); // simulate the physics
            }

            // if the object is the player object, do things that need to be done after the ship
            // is moved (like firing weapons, etc).  We must find the player object to get to
            // the control info field
            if ((objp->flags & OF_PLAYER_SHIP) && (objp->type != OBJ_OBSERVER) &&
                (objp == Player_obj)) {
                player *pp;
                if (Player != NULL) {
                    pp = Player;
                    obj_player_fire_stuff(objp, pp->ci);
                }
            }

            // fire streaming weapons for ships in here - ALL PLAYERS, regardless of client, single player, server, whatever.
            // do stream weapon firing for all ships themselves.
            if (objp->type == OBJ_SHIP) {
                ship_fire_primary(objp, 1, 0);
            }
        }
    }
}

#define IMPORTANT_FLAGS (OF_COLLIDES)

#ifdef OBJECT_CHECK

void
obj_check_object(object *obj)
{
    int objnum = OBJ_INDEX(obj);

    // PROGRAMMERS: If one of these Int3() gets hit, then someone
    // is changing a value in the object structure that might cause
    // collision detection to not work.  See John for more info if
    // you are hitting one of these.

    if (CheckObjects[objnum].type != obj->type) {
        if ((obj->type == OBJ_WAYPOINT) &&
            (CheckObjects[objnum].type == OBJ_SHIP)) {
            // We know about ships changing into waypoints and that is
            // ok.
            CheckObjects[objnum].type = OBJ_WAYPOINT;
        }
        else if ((obj->type == OBJ_SHIP) &&
                 (CheckObjects[objnum].type == OBJ_GHOST)) {
            // We know about player changing into a ghost after dying and that is
            // ok.
            CheckObjects[objnum].type = OBJ_GHOST;
        }
        else if ((obj->type == OBJ_GHOST) &&
                 (CheckObjects[objnum].type == OBJ_SHIP)) {
            // We know about player changing into a ghost after dying and that is
            // ok.
            CheckObjects[objnum].type = OBJ_SHIP;
        }
        else {
            mprintf(("Object type changed!\n"));
            Int3();
        }
    }
    if (CheckObjects[objnum].signature != obj->signature) {
        mprintf(("Object signature changed!\n"));
        Int3();
    }
    if ((CheckObjects[objnum].flags & IMPORTANT_FLAGS) !=
        (obj->flags & IMPORTANT_FLAGS)) {
        mprintf(("Object flags changed!\n"));
        Int3();
    }
    if (CheckObjects[objnum].parent_sig != obj->parent_sig) {
        mprintf(("Object parent sig changed!\n"));
        Int3();
    }
    if (CheckObjects[objnum].parent_type != obj->parent_type) {
        mprintf(("Object's parent type changed!\n"));
        Int3();
    }
}
#endif

// Call this if you want to change an object flag so that the
// object code knows what's going on.  For instance if you turn
// off OF_COLLIDES, the object code needs to know this in order to
// actually turn the object collision detection off.  By calling
// this you shouldn't get Int3's in the checkobject code.  If you
// do, then put code in here to correctly handle the case.
void
obj_set_flags(object *obj, uint new_flags)
{
    int objnum = OBJ_INDEX(obj);

    // turning collision detection off
    if ((obj->flags & OF_COLLIDES) && (!(new_flags & OF_COLLIDES))) {
        // Remove all object pairs
        obj_remove_pairs(obj);

        // update object flags properly
        obj->flags = new_flags;
        obj->flags |= OF_NOT_IN_COLL;
#ifdef OBJECT_CHECK
        CheckObjects[objnum].flags = new_flags;
        CheckObjects[objnum].flags |= OF_NOT_IN_COLL;
#endif
        return;
    }

    // turning collision detection on
    if ((!(obj->flags & OF_COLLIDES)) && (new_flags & OF_COLLIDES)) {
        // observers can't collide or be hit, and they therefore have no hit or collide functions
        // So, don't allow this bit to be set
        if (obj->type == OBJ_OBSERVER) {
            mprintf(("Illegal to set collision bit for OBJ_OBSERVER!!\n"));
            Int3();
        }

        obj->flags |= OF_COLLIDES;

        // Turn on collision detection
        obj_add_pairs(objnum);

        obj->flags = new_flags;
        obj->flags &= ~(OF_NOT_IN_COLL);
#ifdef OBJECT_CHECK
        CheckObjects[objnum].flags = new_flags;
        CheckObjects[objnum].flags &= ~(OF_NOT_IN_COLL);
#endif
        return;
    }

    // Check for unhandled flag changing
    if ((new_flags & IMPORTANT_FLAGS) != (obj->flags & IMPORTANT_FLAGS)) {
        mprintf(("Unhandled flag changing in obj_set_flags!!\n"));
        mprintf(("Add code to support it, see John for questions!!\n"));
        Int3();
    }
    else {
        // Since it wasn't an important flag, just bash it.
        obj->flags = new_flags;
#ifdef OBJECT_CHECK
        CheckObjects[objnum].flags = new_flags;
#endif
    }
}

void
obj_move_all_pre(object *objp, float frametime)
{
    switch (objp->type) {
    case OBJ_WEAPON:
        if (!physics_paused) {
            weapon_process_pre(objp, frametime);
        }
        break;
    case OBJ_SHIP:
        if (!physics_paused || (objp == Player_obj)) {
            ship_process_pre(objp, frametime);
        }
        break;
    case OBJ_FIREBALL:
        if (!physics_paused) {
            fireball_process_pre(objp, frametime);
        }
        break;
    case OBJ_SHOCKWAVE:
        // all shockwaves are moved via shockwave_move_all()
        break;
    case OBJ_DEBRIS:
        if (!physics_paused) {
            debris_process_pre(objp, frametime);
        }
        break;
    case OBJ_ASTEROID:
        if (!physics_paused) {
            asteroid_process_pre(objp, frametime);
        }
        break;
    case OBJ_CMEASURE:
        if (!physics_paused) {
            cmeasure_process_pre(objp, frametime);
        }
        break;
    case OBJ_WAYPOINT:
        break; // waypoints don't move..
    case OBJ_GHOST:
        break;
    case OBJ_OBSERVER:
    case OBJ_JUMP_NODE:
        break;
    case OBJ_BEAM:
        break;
    case OBJ_NONE:
        Int3();
        break;
    default:
        Error(LOCATION, "Unhandled object type %d in obj_move_one\n", objp->type);
    }
}

// Used to tell if a particular group of lasers has cast light yet.
ubyte Obj_weapon_group_id_used[WEAPON_MAX_GROUP_IDS];

// Called once a frame to mark all weapon groups as not having cast light yet.
void
obj_clear_weapon_group_id_list()
{
    memset(Obj_weapon_group_id_used, 0, sizeof(Obj_weapon_group_id_used));
}

int Arc_light = 1; // If set, electrical arcs on debris cast light
DCF_BOOL(arc_light, Arc_light)

void
obj_move_all_post(object *objp, float frametime)
{
    switch (objp->type) {
    case OBJ_WEAPON:
        if (!physics_paused) {
            weapon_process_post(objp, frametime);
        }
        // Cast light
        if (Detail.lighting > 2) {
            // Weapons cast light

            int group_id = Weapons[objp->instance].group_id;
            int cast_light = 1;

            if ((group_id > -1) && (Obj_weapon_group_id_used[group_id] == 0)) {
                // Mark this group as done
                Obj_weapon_group_id_used[group_id]++;
            }
            else {
                // This group has already done its light casting
                cast_light = 0;
            }

            if (cast_light) {
                light_add_point(&objp->pos, 10.0f, 20.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                                objp->parent);
            }
        }
        break;
    case OBJ_SHIP:
        if (!physics_paused || (objp == Player_obj))
            ship_process_post(objp, frametime);

        // Make any electrical arcs on ships cast light
        if (Arc_light) {
            if (Detail.lighting > 2) {
                int i;
                ship *shipp;
                shipp = &Ships[objp->instance];

                for (i = 0; i < MAX_SHIP_ARCS; i++) {
                    if (timestamp_valid(shipp->arc_timestamp[i])) {
                        // Move arc endpoints into world coordinates
                        vector tmp1, tmp2;
                        vm_vec_unrotate(&tmp1, &shipp->arc_pts[i][0],
                                        &objp->orient);
                        vm_vec_add2(&tmp1, &objp->pos);

                        vm_vec_unrotate(&tmp2, &shipp->arc_pts[i][1],
                                        &objp->orient);
                        vm_vec_add2(&tmp2, &objp->pos);

                        light_add_point(&tmp1, 10.0f, 20.0f, frand(), 1.0f, 1.0f,
                                        1.0f, -1);
                        light_add_point(&tmp2, 10.0f, 20.0f, frand(), 1.0f, 1.0f,
                                        1.0f, -1);
                    }
                }
            }
        }

        break;
    case OBJ_FIREBALL:
        if (!physics_paused) {
            fireball_process_post(objp, frametime);
        }
        if (Detail.lighting > 3) {
            // Make explosions cast light
            float p = fireball_lifeleft_percent(objp);
            if (p > 0.5f) {
                p = 1.0f - p;
            }
            p *= 2.0f;
            // P goes from 0 to 1 to 0 over the life of the explosion
            float rad = p * (1.0f + frand() * 0.05f) * objp->radius;

            light_add_point(&objp->pos, rad * 2.0f, rad * 5.0f, 1.0f, 1.0f, 1.0f,
                            1.0f, -1);
        }
        break;
    case OBJ_SHOCKWAVE:
        // all shockwaves are moved via shockwave_move_all()
        break;
    case OBJ_DEBRIS:
        if (!physics_paused)
            debris_process_post(objp, frametime);

        // Make any electrical arcs on debris cast light
        if (Arc_light) {
            if (Detail.lighting > 2) {
                int i;
                debris *db;
                db = &Debris[objp->instance];

                for (i = 0; i < MAX_DEBRIS_ARCS; i++) {
                    if (timestamp_valid(db->arc_timestamp[i])) {
                        // Move arc endpoints into world coordinates
                        vector tmp1, tmp2;
                        vm_vec_unrotate(&tmp1, &db->arc_pts[i][0], &objp->orient);
                        vm_vec_add2(&tmp1, &objp->pos);

                        vm_vec_unrotate(&tmp2, &db->arc_pts[i][1], &objp->orient);
                        vm_vec_add2(&tmp2, &objp->pos);

                        light_add_point(&tmp1, 10.0f, 20.0f, frand(), 1.0f, 1.0f,
                                        1.0f, -1);
                        light_add_point(&tmp2, 10.0f, 20.0f, frand(), 1.0f, 1.0f,
                                        1.0f, -1);
                    }
                }
            }
        }
        break;
    case OBJ_ASTEROID:
        if (!physics_paused)
            asteroid_process_post(objp, frametime);
        break;
    case OBJ_CMEASURE:
        if (!physics_paused)
            cmeasure_process_post(objp, frametime);
        break;
    case OBJ_WAYPOINT:
        break; // waypoints don't move..
    case OBJ_GHOST:
        break;
    case OBJ_OBSERVER:
        break;
    case OBJ_JUMP_NODE:
        radar_plot_object(objp);
        break;
    case OBJ_BEAM:
        break;
    case OBJ_NONE:
        Int3();
        break;
    default:
        Error(LOCATION, "Unhandled object type %d in obj_move_one\n", objp->type);
    }
}

int Collisions_enabled = 1;

DCF_BOOL(collisions, Collisions_enabled)

MONITOR(NumObjects);

//--------------------------------------------------------------------
//move all objects for the current frame
void
obj_move_all(float frametime)
{
    object *objp;

    obj_delete_all_that_should_be_dead();

    obj_merge_created_list();

    // Clear the table that tells which groups of weapons have cast light so far.
    obj_clear_weapon_group_id_list();

    MONITOR_INC(NumObjects, num_objects);

    objp = GET_FIRST(&obj_used_list);
    while (objp != END_OF_LIST(&obj_used_list)) {
        // skip objects which should be dead
        if (!(objp->flags & OF_SHOULD_BE_DEAD)) {
            vector cur_pos = objp->pos; // Save the current position

            // if this is an observer object, skip it
            if (objp->type == OBJ_OBSERVER) {
                objp = GET_NEXT(objp);
                continue;
            }

#ifdef OBJECT_CHECK
            obj_check_object(objp);
#endif

            // pre-move
            obj_move_all_pre(objp, frametime);

            // store last pos and orient
            objp->last_pos = cur_pos;
            objp->last_orient = objp->orient;

            // physics
            obj_move_call_physics(objp, frametime);

            // move post
            obj_move_all_post(objp, frametime);
        }
        objp = GET_NEXT(objp);
    }

    //   After all objects have been moved, move all docked objects.
    objp = GET_FIRST(&obj_used_list);
    while (objp != END_OF_LIST(&obj_used_list)) {
        if (objp->type == OBJ_SHIP) {
            move_docked_objects(objp);
        }

        // unflag all objects as being updates
        objp->flags &= ~OF_JUST_UPDATED;

        objp = GET_NEXT(objp);
    }

    // Now that all objects have moved, we should calculate the
    // velocities from how far they moved.
    // DA: Commented out 2/23, unnecessary since colliding objects calculate their post collision velocities through physics.
    /*
   objp = GET_FIRST(&obj_used_list);
   while( objp !=END_OF_LIST(&obj_used_list) )  {
      if ( !(objp->flags&OF_SHOULD_BE_DEAD) && (objp->type != OBJ_OBSERVER) && (objp->type != OBJ_ASTEROID) && (objp->type != OBJ_DEBRIS))   {
         objp->phys_info.vel.x = (objp->pos.x - objp->last_pos.x) / frametime;
         objp->phys_info.vel.y = (objp->pos.y - objp->last_pos.y) / frametime;
         objp->phys_info.vel.z = (objp->pos.z - objp->last_pos.z) / frametime;
      }
      objp = GET_NEXT(objp);
   } */

    find_homing_object_cmeasures(); // If any cmeasures fired, maybe steer away homing missiles

    // do pre-collision stuff for beam weapons
    beam_move_all_pre();

    if (Collisions_enabled) {
        obj_check_all_collisions();
    }

    turret_swarm_check_validity();

    // do post-collision stuff for beam weapons
    beam_move_all_post();

    // update artillery locking info now
    ship_update_artillery_lock();
}

MONITOR(NumObjectsRend);

// -----------------------------------------------------------------------------
// Render an object.  Calls one of several routines based on type
void
obj_render(object *obj)
{
    if (obj->flags & OF_SHOULD_BE_DEAD)
        return;
    //   if ( obj == Viewer_obj ) return;

    MONITOR_INC(NumObjectsRend, 1);

    switch (obj->type) {
    case OBJ_NONE:
#ifndef NDEBUG
        mprintf(("ERROR!!!! Bogus obj %d is rendering!\n", obj - Objects));
        Int3();
#endif
        break;
    case OBJ_WEAPON:
        weapon_render(obj);
        break;
    case OBJ_SHIP:
        ship_render(obj);
        break;
    case OBJ_FIREBALL:
        fireball_render(obj);
        break;
    case OBJ_SHOCKWAVE:
        shockwave_render(obj);
        break;
    case OBJ_DEBRIS:
        debris_render(obj);
        break;
    case OBJ_ASTEROID:
        asteroid_render(obj);
        break;
    case OBJ_CMEASURE:
        cmeasure_render(obj);
        break;
    case OBJ_JUMP_NODE:
        jumpnode_render(obj, &obj->pos, &Eye_position);
        break;
    case OBJ_WAYPOINT:
        if (Show_waypoints) {
            //ship_render(obj);
            gr_set_color(128, 128, 128);
            g3_draw_sphere_ez(&obj->pos, 5.0f);
        }
        break;
    case OBJ_GHOST:
        break;
    case OBJ_BEAM:
        break;
    default:
        Error(LOCATION, "Unhandled obj type %d in obj_render", obj->type);
    }
}

void
obj_init_all_ships_physics()
{
    object *objp;

    for (objp = GET_FIRST(&obj_used_list); objp != END_OF_LIST(&obj_used_list);
         objp = GET_NEXT(objp)) {
        if (objp->type == OBJ_SHIP)
            physics_ship_init(objp);
    }
}

// function to return a vector of the average position of all ships in the mission.
void
obj_get_average_ship_pos(vector *pos)
{
    int count;
    object *objp;

    vm_vec_zero(pos);

    // average up all ship positions
    count = 0;
    for (objp = GET_FIRST(&obj_used_list); objp != END_OF_LIST(&obj_used_list);
         objp = GET_NEXT(objp)) {
        if (objp->type != OBJ_SHIP)
            continue;
        vm_vec_add2(pos, &objp->pos);
        count++;
    }

    if (count)
        vm_vec_scale(pos, 1.0f / (float)count);
}

int
obj_get_SIF(object *objp)
{
    if ((objp->type == OBJ_SHIP) || (objp->type == OBJ_START))
        return Ship_info[Ships[objp->instance].ship_info_index].flags;

    Int3();
    return 0;
}

int
obj_get_SIF(int obj)
{
    if ((Objects[obj].type == OBJ_SHIP) || (Objects[obj].type == OBJ_START))
        return Ship_info[Ships[Objects[obj].instance].ship_info_index].flags;

    Int3();
    return 0;
}

// Return the team for the object passed as a parameter
//
// input:      objp => pointer to object that you want team for
//
// exit:       success => enumerated team ( TEAM_HOSTILE, TEAM_FRIENDLY, TEAM_NEUTRAL, etc )
//             failure => -1 (for objects that don't have teams)
int
obj_team(object *objp)
{
    Assert(objp != NULL);
    int team = -1;

    switch (objp->type) {
    case OBJ_SHIP:
        Assert(objp->instance >= 0 && objp->instance < MAX_SHIPS);
        team = Ships[objp->instance].team;
        break;

    case OBJ_DEBRIS:
        team = debris_get_team(objp);
        Assert(team != -1);
        break;

    case OBJ_CMEASURE:
        Assert(objp->instance >= 0 && objp->instance < MAX_CMEASURES);
        team = Cmeasures[objp->instance].team;
        break;

    case OBJ_WEAPON:
        Assert(objp->instance >= 0 && objp->instance < MAX_WEAPONS);
        team = Weapons[objp->instance].team;
        break;

    case OBJ_JUMP_NODE:
        team = Player_ship->team;
        break;

    case OBJ_FIREBALL:
    case OBJ_WAYPOINT:
    case OBJ_START:
    case OBJ_NONE:
    case OBJ_GHOST:
    case OBJ_SHOCKWAVE:
    case OBJ_BEAM:
        nprintf(("Warning", "Warning => Asking for a team for object type %d\n",
                 Object_type_names[objp->type]));
        team = -1;
        break;

    case OBJ_ASTEROID:
        team = TEAM_TRAITOR;
        break;

    default:
        Int3(); // can't happen
        break;
    } // end switch

    Assert(team != -1);
    return team;
}

// -------------------------------------------------------
// obj_add_pairs
//
// Add an element to the CheckObjects[] array, and update the
// object pairs.  This is called from obj_create(), and the restore
// save-game code.
//
void
obj_add_pairs(int objnum)
{
    object *objp;

    Assert(objnum != -1);
    objp = &Objects[objnum];

    // don't do anything if its already in the object pair list
    if (!(objp->flags & OF_NOT_IN_COLL)) {
        return;
    }

#ifdef OBJECT_CHECK
    CheckObjects[objnum].type = objp->type;
    CheckObjects[objnum].signature = objp->signature;
    CheckObjects[objnum].flags = objp->flags & ~(OF_NOT_IN_COLL);
    CheckObjects[objnum].parent_sig = objp->parent_sig;
    CheckObjects[objnum].parent_type = objp->parent_type;
#endif

    // Find all the objects that can collide with this and add
    // it to the collision pair list.
    object *A;
    for (A = GET_FIRST(&obj_used_list); A != END_OF_LIST(&obj_used_list);
         A = GET_NEXT(A)) {
        obj_add_pair(objp, A);
    }

    objp->flags &= ~OF_NOT_IN_COLL;
}

// Removes any occurances of object 'a' from
// the pairs list.
extern int Num_pairs;
extern obj_pair pair_used_list;
extern obj_pair pair_free_list;
void
obj_remove_pairs(object *a)
{
    obj_pair *parent, *tmp;

    a->flags |= OF_NOT_IN_COLL;
#ifdef OBJECT_CHECK
    CheckObjects[OBJ_INDEX(a)].flags |= OF_NOT_IN_COLL;
#endif

    if (a->num_pairs < 1) {
        //mprintf(( "OBJPAIR: No need to remove pairs 1!\n" ));
        return;
    }

    Num_pairs -= a->num_pairs;

    parent = &pair_used_list;
    tmp = parent->next;

    while (tmp != NULL) {
        if ((tmp->a == a) || (tmp->b == a)) {
            // Hmmm... a potenial compiler optimization problem here... either tmp->a or tmp->b
            // is equal to 'a' and we modify 'num_pairs' in one of these and then use the value
            // stored in 'a' later one... will the optimizer find that?  Hmmm...
            tmp->a->num_pairs--;
            Assert(tmp->a->num_pairs > -1);
            tmp->b->num_pairs--;
            Assert(tmp->b->num_pairs > -1);
            parent->next = tmp->next;
            tmp->a = tmp->b = NULL;
            tmp->next = pair_free_list.next;
            pair_free_list.next = tmp;
            tmp = parent->next;

            if (a->num_pairs == 0) {
                //mprintf(( "OBJPAIR: No need to remove pairs 2!\n" ));
                break;
            }
        }
        else {
            parent = tmp;
            tmp = tmp->next;
        }
    }
}

// reset all collisions
void
obj_reset_all_collisions()
{
    // clear checkobjects
#ifndef NDEBUG
    memset(CheckObjects, 0, sizeof(checkobject) * MAX_OBJECTS);
#endif

    // clear object pairs
    obj_reset_pairs();

    // now add every object back into the object collision pairs
    object *moveup;
    moveup = GET_FIRST(&obj_used_list);
    while (moveup != END_OF_LIST(&obj_used_list)) {
        // he's not in the collision list
        moveup->flags |= OF_NOT_IN_COLL;

        // recalc pairs for this guy
        obj_add_pairs(OBJ_INDEX(moveup));

        // next
        moveup = GET_NEXT(moveup);
    }
}
