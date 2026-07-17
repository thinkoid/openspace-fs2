/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#include "objcollide.h"
#include "model.h"
#include "shiphit.h"
#include "player.h"
#include "hudshield.h"
#include "hud.h"
#include "hudwingmanstatus.h"
#include "timer.h"
#include "freespace.h"


extern float ai_endangered_time(object *ship_objp, object *weapon_objp);
int check_inside_radius_for_big_ships( object *ship, object *weapon, obj_pair *pair );
float estimate_ship_speed_upper_limit( object *ship, float time );
extern float flFrametime;


//	If weapon_obj is likely to hit ship_obj sooner than current aip->danger_weapon_objnum,
//	then update danger_weapon_objnum.
void update_danger_weapon(object *ship_obj, object *weapon_obj)
{
	ai_info	*aip;

	Assert(ship_obj->type == OBJ_SHIP);

	aip = &Ai_info[Ships[ship_obj->instance].ai_index];

	if (aip->danger_weapon_objnum == -1) {
		aip->danger_weapon_objnum = weapon_obj-Objects;
		aip->danger_weapon_signature = weapon_obj->signature;
	} else if (aip->danger_weapon_signature == Objects[aip->danger_weapon_objnum].signature) {
		float	danger_old_time, danger_new_time;

		danger_old_time = ai_endangered_time(ship_obj, &Objects[aip->danger_weapon_objnum]);
		danger_new_time = ai_endangered_time(ship_obj, weapon_obj);

		if (danger_new_time < danger_old_time) {
			aip->danger_weapon_objnum = weapon_obj-Objects;
			aip->danger_weapon_signature = weapon_obj->signature;
		}
	}
}

// function to actually deal with weapon-ship hit stuff.  separated from check_collision routine below
void ship_weapon_do_hit_stuff(object *ship_obj, object *weapon_obj, vector *world_hitpos, vector *hitpos, int quadrant_num, int submodel_num)
{
	weapon	*wp = &Weapons[weapon_obj->instance];
	weapon_info	*wip = &Weapon_info[wp->weapon_info_index];
	ship *shipp = &Ships[ship_obj->instance];	
	float damage;
	vector force;		

	// Apply hit & damage & stuff to weapon
	weapon_hit(weapon_obj, ship_obj,  world_hitpos);

	damage = wip->damage;

	// deterine whack whack
	float		blast = wip->mass;
	vm_vec_copy_scale(&force, &weapon_obj->phys_info.vel, blast );

	ship_apply_local_damage(ship_obj, weapon_obj, world_hitpos, damage, quadrant_num, CREATE_SPARKS, submodel_num);

	// let the hud shield gauge know when Player or Player target is hit
	hud_shield_quadrant_hit(ship_obj, quadrant_num);

	// Let wingman status gauge know a wingman ship was hit
	if ( (Ships[ship_obj->instance].wing_status_wing_index >= 0) && ((Ships[ship_obj->instance].wing_status_wing_pos >= 0)) ) {
		hud_wingman_status_start_flash(shipp->wing_status_wing_index, shipp->wing_status_wing_pos);
	}

	// Apply a wack.  This used to be inside of ship_hit... duh! Ship_hit
	// is to apply damage, not physics, so I moved it here.
	// apply a whack
	ship_apply_whack( &force, hitpos, ship_obj );
}

extern int Framecount;

int ship_weapon_check_collision(object * ship_obj, object * weapon_obj, float time_limit = 0.0f, int *next_hit=NULL)
{
	mc_info mc;
	int	num;
	ship	*shipp;
	weapon	*wp = &Weapons[weapon_obj->instance];
	weapon_info	*wip = &Weapon_info[wp->weapon_info_index];

	Assert( ship_obj->type == OBJ_SHIP );
	Assert( weapon_obj->type == OBJ_WEAPON );

	num = ship_obj->instance;
	Assert( num >= 0 );
	Assert( Ships[num].objnum == OBJ_INDEX(ship_obj));

	shipp = &Ships[num];

	// Make ships that are warping in not get collision detection done
	if ( shipp->flags & SF_ARRIVING ) return 0;

	// if one object is a capital, only check player and player weapons with
	// the capital -- too slow for now otherwise.
//	if ( Polygon_models[Ships[num].modelnum].use_grid && !( (other_obj == Player_obj) || (&Objects[other_obj->parent] == Player_obj)) )
//		return 0;

	//	If either of these objects doesn't get collision checks, abort.
	if (!(Ship_info[shipp->ship_info_index].flags & SIF_DO_COLLISION_CHECK))
		return 0;

	//	Return information for AI to detect incoming fire.
	//	Could perhaps be done elsewhere at lower cost --MK, 11/7/97
	float	dist = vm_vec_dist_quick(&ship_obj->pos, &weapon_obj->pos);
	if (dist < weapon_obj->phys_info.speed) {
		update_danger_weapon(ship_obj, weapon_obj);
	}
	
	ship_model_start(ship_obj);

	int	valid_hit_occured = 0;				// If this is set, then hitpos is set
	int	do_model_check = 1;					// Assume we need to check the model
	int	quadrant_num = -1;

	//	total time is flFrametime + time_limit (time_limit used to predict collisions into the future)
	vector weapon_end_pos;
	vm_vec_scale_add( &weapon_end_pos, &weapon_obj->pos, &weapon_obj->phys_info.vel, time_limit );

	memset(&mc, -1, sizeof(mc_info));
	mc.model_num = shipp->modelnum;			// Fill in the model to check
	mc.orient = &ship_obj->orient;			// The object's orient
	mc.pos = &ship_obj->pos;					// The object's position
	mc.p0 = &weapon_obj->last_pos;			// Point 1 of ray to check
	mc.p1 = &weapon_end_pos;					// Point 2 of ray to check

	polymodel *pm = model_get( shipp->modelnum );

	// Check the shields for an impact if necessary
#ifndef NDEBUG
	if (!(ship_obj->flags & OF_NO_SHIELDS) && New_shield_system && (pm->shield.ntris > 0)) {
#else
	if (!(ship_obj->flags & OF_NO_SHIELDS) &&  (pm->shield.ntris > 0)) {
#endif

		mc.flags = MC_CHECK_SHIELD;

		if ( model_collide(&mc) )	{
			quadrant_num = get_quadrant(&mc.hit_point);
			//	Note: This code is obviously stupid. We want to add the shield point if there is shield to hit, but:
			//		1. We want the size/color of the hit effect to indicate shield damage done.  (Ie, for already-weak shield, smaller effect.)
			//		2. Currently (8/9/97), apply_damage_to_shield() passes lefer damage to hull, which might not make sense.  If
			//			wouldn't have collided with hull, shouldn't do damage.  Once this is fixed, the code below needs to cast the
			//			vector through to the hull if there is leftover damage.
			if (!(shipp->flags & SF_DYING) && ship_is_shield_up(ship_obj,quadrant_num) ) {

				// AL 1-14-97: "Puncture" doesn't mean penetrate shield anymore, it means that it punctures
				//					hull do inflict maximum subsystem damage
/*
				if ( Weapon_info[Weapons[other_obj->instance].weapon_info_index].wi_flags & WIF_PUNCTURE )	{
					// If this weapon punctures the shield, then do
					// the hit effect, but act like a shield collision never occurred.
					quadrant_num = -1;	// ignore shield hit
					add_shield_point(obj-Objects, mc.shield_hit_tri, &mc.hit_point);
				} else {
*/
				valid_hit_occured = 1;
				// shield effect
				add_shield_point(ship_obj-Objects, mc.shield_hit_tri, &mc.hit_point);
				do_model_check = 0;	// since we hit the shield, no need to check the model

			} else {
				quadrant_num = -1;	// ignore shield hit
			}
		}
	} 

	// Check the model for an impact if necessary
	if ( do_model_check )	{			
		mc.flags = MC_CHECK_MODEL;			// flags

		if (model_collide(&mc))	{
			valid_hit_occured = 1;
		}
	}

	//nprintf(("AI", "Frame %i, Hit tri = %i\n", Framecount, mc.shield_hit_tri));
	ship_model_stop(ship_obj);

	// deal with predictive collisions.  Find their actual hit time and see if they occured in current frame
	if (next_hit && valid_hit_occured) {
		// find hit time
		*next_hit = (int) (1000.0f * (mc.hit_dist*(flFrametime + time_limit) - flFrametime) );
		if (*next_hit > 0)
			// if hit occurs outside of this frame, do not do damage 
			return 1;
	}

	if ( valid_hit_occured )	{
		ship_weapon_do_hit_stuff(ship_obj, weapon_obj, &mc.hit_point_world, &mc.hit_point, quadrant_num, mc.hit_submodel);
	} else if ((Missiontime - wp->creation_time > F1_0/2) && (wip->wi_flags & WIF_HOMING) && (wp->homing_object == ship_obj)) {
		if (dist < wip->inner_radius) {
			vector	vec_to_ship;

			vm_vec_normalized_dir(&vec_to_ship, &ship_obj->pos, &weapon_obj->pos);

			if (vm_vec_dot(&vec_to_ship, &weapon_obj->orient.fvec) < 0.0f) {
				// check if we're colliding against "invisible" ship
				if (!(Ship_info[shipp->ship_info_index].flags & SIF_DONT_COLLIDE_INVIS)) {
					wp->lifeleft = 0.001f;
					if (ship_obj == Player_obj)
						nprintf(("Jim", "Frame %i: Weapon %i set to detonate, dist = %7.3f.\n", Framecount, weapon_obj-Objects, dist));
					valid_hit_occured = 1;
				}
			}

		}
	}

	
	return valid_hit_occured;
}


// Checks ship-weapon collisions.  pair->a is ship and pair->b is weapon.
// Returns 1 if all future collisions between these can be ignored
int collide_ship_weapon( obj_pair * pair )
{
	int		did_hit;
	object *ship = pair->a;
	object *weapon = pair->b;
	
	Assert( ship->type == OBJ_SHIP );
	Assert( weapon->type == OBJ_WEAPON );

	// Don't check collisions for player if past first warpout stage.
	if ( Player->control_mode > PCM_WARPOUT_STAGE1)	{
		if ( ship == Player_obj )
			return 0;
	}

	// Cull lasers within big ship spheres by casting a vector forward for (1) exit sphere or (2) lifetime of laser
	// If it does hit, don't check the pair until about 200 ms before collision.  
	// If it does not hit and is within error tolerance, cull the pair.

	if ( (Ship_info[Ships[ship->instance].ship_info_index].flags & (SIF_BIG_SHIP | SIF_HUGE_SHIP)) && (Weapon_info[Weapons[weapon->instance].weapon_info_index].subtype == WP_LASER) ) {
//	if (  (ship->radius > 50) && (Weapon_info[Weapons[weapon->instance].weapon_info_index].subtype == WP_LASER) ) {
		// Check when within ~1.1 radii.  
		// This allows good transition between sphere checking (leaving the laser about 200 ms from radius) and checking
		// within the sphere with little time between.  There may be some time for "small" big ships
		if ( vm_vec_dist_squared(&ship->pos, &weapon->pos) < (1.2f*ship->radius*ship->radius) ) {
			return check_inside_radius_for_big_ships( ship, weapon, pair );
		}
	}


//	demo_do_rand_test();
	did_hit = ship_weapon_check_collision( ship, weapon );
//	demo_do_rand_test();
	if ( !did_hit )	{
		// Since we didn't hit, check to see if we can disable all future collisions
		// between these two.
		return weapon_will_never_hit( weapon, ship, pair );
	}

	return 0;
}

// ----------------------------------------------------------------------------
// upper limit estimate ship speed at end of time
float estimate_ship_speed_upper_limit( object *ship, float time ) 
{
	float exponent;
	float delta_v;
	float factor;

	delta_v = Ship_info[Ships[ship->instance].ship_info_index].max_vel.z - ship->phys_info.speed;
	if (ship->phys_info.forward_accel_time_const == 0) {
		return ship->phys_info.speed;
	}
	exponent = time / ship->phys_info.forward_accel_time_const;
	//Assert( exponent >= 0);


	factor = 1.0f - (float)exp( -exponent );
	return ship->phys_info.speed + factor*delta_v;
}

// maximum error allowed in detecting collisions between laser and big ship inside the radius
// this is set for a 150 m radius ship.  For ships with larger radius, the error scales according
// to the ration of radii, but is never less than 2 m
#define ERROR_STD	2	

// ----------------------------------------------------------------------------							
// check_inside_radius_for_big_ships()							
// when inside radius of big ship, check if we can cull collision pair
// determine the time when pair should next be checked
// return 1 if pair can be culled
// return 0 if pair can not be culled
int check_inside_radius_for_big_ships( object *ship, object *weapon, obj_pair *pair )
{
	vector error_vel;		// vel perpendicular to laser
	float error_vel_mag;	// magnitude of error_vel
	float time_to_max_error, time_to_exit_sphere;
	float ship_speed_at_exit_sphere, error_at_exit_sphere;	
	float max_error = (float) ERROR_STD / 150.0f * ship->radius;
	if (max_error < 2)
		max_error = 2.0f;

	time_to_exit_sphere = (ship->radius + vm_vec_dist(&ship->pos, &weapon->pos)) / (weapon->phys_info.max_vel.z - ship->phys_info.max_vel.z);
	ship_speed_at_exit_sphere = estimate_ship_speed_upper_limit( ship, time_to_exit_sphere );
	// update estimated time to exit sphere
	time_to_exit_sphere = (ship->radius + vm_vec_dist(&ship->pos, &weapon->pos)) / (weapon->phys_info.max_vel.z - ship_speed_at_exit_sphere);
	vm_vec_scale_add( &error_vel, &ship->phys_info.vel, &weapon->orient.fvec, -vm_vec_dotprod(&ship->phys_info.vel, &weapon->orient.fvec) );
	error_vel_mag = vm_vec_mag_quick( &error_vel );
	error_vel_mag += 0.5f * (ship->phys_info.max_vel.z - error_vel_mag)*(time_to_exit_sphere/ship->phys_info.forward_accel_time_const);
	// error_vel_mag is now average velocity over period
	error_at_exit_sphere = error_vel_mag * time_to_exit_sphere;
	time_to_max_error = max_error / error_at_exit_sphere * time_to_exit_sphere;

	// find the minimum time we can safely check into the future.
	// limited by (1) time to exit sphere (2) time to weapon expires
	// if ship_weapon_check_collision comes back with a hit_time > error limit, ok
	// if ship_weapon_check_collision comes finds no collision, next check time based on error time
	float limit_time;		// furthest time to check (either lifetime or exit sphere)
	if ( time_to_exit_sphere < Weapons[weapon->instance].lifeleft ) {
		limit_time = time_to_exit_sphere;
	} else {
		limit_time = Weapons[weapon->instance].lifeleft;
	}

	// Note:  when estimated hit time is less than 200 ms, look at every frame
	int hit_time;	// estimated time of hit in ms

	// modify ship_weapon_check_collision to do damage if hit_time is negative (ie, hit occurs in this frame)
	if ( ship_weapon_check_collision( ship, weapon, limit_time, &hit_time ) ) {
		// hit occured in while in sphere
		if (hit_time < 0) {
			// hit occured in the frame
			return 1;
		} else if (hit_time > 200) {
			pair->next_check_time = timestamp(hit_time - 200);
			return 0;
			// set next check time to time - 200
		} else {
			// set next check time to next frame
			pair->next_check_time = 1;
			return 0;
		}
	} else {
		if (limit_time > time_to_max_error) {
		// no hit, but beyond error tolerance
			if (1000*time_to_max_error > 200) {
				pair->next_check_time = timestamp( (int)(1000*time_to_max_error) - 200 );
			} else {
				pair->next_check_time = 1;
			}
			return 0;
		} else {
			// no hit and within error tolerance
			return 1;
		}
	}
}
