/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <physics/physics.hh>
#include <math/floating.hh>
#include <playerman/player.hh>
#include <freespace2/freespace.hh>
#include <globalincs/linklist.hh>
#include <io/timer.hh>
#include <io/key.hh>

// defines for physics functions
#define MAX_TURN_LIMIT 0.2618f // about 15 degrees

#define ROT_DEBUG
#define ROTVEL_TOL 0.1 // Amount of rotvel is decreased if over cap
#define ROTVEL_CAP 14.0 // Rotational velocity cap for live objects
#define DEAD_ROTVEL_CAP 16.3 // Rotational velocity cap for dead objects

#define MAX_SHIP_SPEED 300 // Maximum speed allowed after whack or shockwave
#define RESET_SHIP_SPEED                                                         \
    240 // Speed that a ship is reset to after exceeding MAX_SHIP_SPEED

#define SW_ROT_FACTOR 5 // increase in rotational time constant in shockwave
#define SW_BLAST_DURATION 2000 // maximum duration of shockwave
#define REDUCED_DAMP_FACTOR                                                      \
    10 // increase in side_slip and acceleration time constants (scaled according to reduced damp time)
#define REDUCED_DAMP_VEL                                                         \
    30 // change in velocity at which reduced_damp_time is 2000 ms
#define REDUCED_DAMP_TIME 2000 // ms (2.0 sec)
#define WEAPON_SHAKE_TIME                                                        \
    500 //  ms (0.5 sec)   viewer shake time after hit by weapon (implemented via afterburner shake)
#define SPECIAL_WARP_T_CONST                                                     \
    0.651 // special warp time constant (loose 99 % of excess speed in 3 sec)

#define PHYS_DEBUG // check if (vel > 500) or (displacement in one frame > 350)

void update_reduced_damp_timestamp(physics_info *pi, float impulse);

void
physics_init(physics_info *pi)
{
    memset(pi, 0, sizeof(physics_info));

    pi->mass = 10.0f; // This ship weighs 10 units
    pi->side_slip_time_const = 0.05f;
    pi->rotdamp = 0.1f;

    pi->max_vel.x = 100.0f; //sideways
    pi->max_vel.y = 100.0f; //up/down
    pi->max_vel.z = 100.0f; //forward
    pi->max_rear_vel = 100.0f; //backward -- controlled seperately

    pi->max_rotvel.x = 2.0f; //pitch
    pi->max_rotvel.y = 1.0f; //heading
    pi->max_rotvel.z = 2.0f; //bank

    pi->prev_ramp_vel.x = 0.0f;
    pi->prev_ramp_vel.y = 0.0f;
    pi->prev_ramp_vel.z = 0.0f;

    pi->desired_vel.x = 0.0f;
    pi->desired_vel.y = 0.0f;
    pi->desired_vel.z = 0.0f;

    pi->slide_accel_time_const =
        pi->side_slip_time_const; // slide using max_vel.x & .y
    pi->slide_decel_time_const =
        pi->side_slip_time_const; // slide using max_vel.x & .y

    pi->afterburner_decay = 1;
    pi->forward_thrust = 0.0f;

    pi->flags = 0;

    // default values for moment of inetaia
    vm_vec_make(&pi->I_body_inv.rvec, 1e-5f, 0.0f, 0.0f);
    vm_vec_make(&pi->I_body_inv.uvec, 0.0f, 1e-5f, 0.0f);
    vm_vec_make(&pi->I_body_inv.fvec, 0.0f, 0.0f, 1e-5f);
}

//==========================================================================
// apply_physics - This does correct physics independent of frame rate.
//
// Given:
//    damping = damping factor.  Setting this to zero make the object instantly
//              go to the target velocity.  Increasing it makes the object ramp
//              up or down to the target velocity.
//    desired_vel = the target velocity
//    initial_vel = velocity at last call
//    t = elapsed time since last call
//
// Returns:
//    new_vel = current velocity
//    delta_pos = delta position (framevec)
// You can extend this to 3d by calling it 3 times, once for each x,y,z component.

void
apply_physics(float damping, float desired_vel, float initial_vel, float t,
              float *new_vel, float *delta_pos)
{
    if (damping < 0.0001f) {
        if (delta_pos)
            *delta_pos = desired_vel * t;
        if (new_vel)
            *new_vel = desired_vel;
    }
    else {
        float dv, e;
        dv = initial_vel - desired_vel;
        e = (float)exp(-t / damping);
        if (delta_pos)
            *delta_pos = (1.0f - e) * dv * damping + desired_vel * t;
        if (new_vel)
            *new_vel = dv * e + desired_vel;
    }
}

float Physics_viewer_bank = 0.0f;
int Physics_viewer_direction = PHYSICS_VIEWER_FRONT;
static physics_info *Viewer_physics_info = NULL;

// If you would like Physics_viewer_bank to be tracked (Which is needed
// for rotating 3d bitmaps) call this and pass it a pointer to the
// viewer's physics info.
void
physics_set_viewer(physics_info *p, int dir)
{
    if ((Viewer_physics_info != p) || (Physics_viewer_direction != dir)) {
        Viewer_physics_info = p;
        Physics_viewer_bank = 0.0f;
        Physics_viewer_direction = dir;
    }
}

// -----------------------------------------------------------------------------------------------------------
// add rotational velocity & acceleration

void
physics_sim_rot(matrix *orient, physics_info *pi, float sim_time)
{
    angles tangles;
    vector new_vel;
    matrix tmp;
    float shock_amplitude;
    float rotdamp;
    float shock_fraction_time_left;

    Assert(is_valid_matrix(orient));
    Assert(is_valid_vec(&pi->rotvel));
    Assert(is_valid_vec(&pi->desired_rotvel));

    // Handle special case of shockwave
    shock_amplitude = 0.0f;
    if (pi->flags & PF_IN_SHOCKWAVE) {
        if (timestamp_elapsed(pi->shockwave_decay)) {
            pi->flags &= ~PF_IN_SHOCKWAVE;
            rotdamp = pi->rotdamp;
        }
        else {
            shock_fraction_time_left = timestamp_until(pi->shockwave_decay) /
                                       (float)SW_BLAST_DURATION;
            rotdamp = pi->rotdamp + pi->rotdamp * (SW_ROT_FACTOR - 1) *
                                        shock_fraction_time_left;
            shock_amplitude = pi->shockwave_shake_amp * shock_fraction_time_left;
        }
    }
    else {
        rotdamp = pi->rotdamp;
    }

    // Do rotational physics with given damping
    apply_physics(rotdamp, pi->desired_rotvel.x, pi->rotvel.x, sim_time,
                  &new_vel.x, NULL);
    apply_physics(rotdamp, pi->desired_rotvel.y, pi->rotvel.y, sim_time,
                  &new_vel.y, NULL);
    apply_physics(rotdamp, pi->desired_rotvel.z, pi->rotvel.z, sim_time,
                  &new_vel.z, NULL);

    /*
#ifdef ROT_DEBUG
   if (check_rotvel_limit( pi )) {
      nprintf(("Physics", "rotvel reset in physics_sim_rot\n"));
   }
#endif
*/
    Assert(is_valid_vec(&new_vel));

    pi->rotvel = new_vel;

    tangles.p = pi->rotvel.x * sim_time;
    tangles.h = pi->rotvel.y * sim_time;
    tangles.b = pi->rotvel.z * sim_time;

    // If this is the viewer_object, keep track of the
    // changes in banking so that rotated bitmaps look correct.
    // This is used by the g3_draw_rotated_bitmap function.
    if (pi == Viewer_physics_info) {
        switch (Physics_viewer_direction) {
        case PHYSICS_VIEWER_FRONT:
            Physics_viewer_bank -= tangles.b;
            break;

        case PHYSICS_VIEWER_UP:
            Physics_viewer_bank -= tangles.h;
            break;

        case PHYSICS_VIEWER_REAR:
            Physics_viewer_bank += tangles.b;
            break;

        case PHYSICS_VIEWER_LEFT:
            Physics_viewer_bank += tangles.p;
            break;

        case PHYSICS_VIEWER_RIGHT:
            Physics_viewer_bank -= tangles.p;
            break;

        default:
            Physics_viewer_bank -= tangles.b;
            break;
        }

        if (Physics_viewer_bank < 0.0f) {
            Physics_viewer_bank += 2.0f * PI;
        }

        if (Physics_viewer_bank > 2.0f * PI) {
            Physics_viewer_bank -= 2.0f * PI;
        }
    }

    /*   // Make ship shake due to afterburner.
   if (pi->flags & PF_AFTERBURNER_ON || !timestamp_elapsed(pi->afterburner_decay) ) {
      float max_speed;

      max_speed = vm_vec_mag_quick(&pi->max_vel);
      tangles.p += (float) (rand()-RAND_MAX/2)/RAND_MAX * pi->speed/max_speed/64.0f;
      tangles.h += (float) (rand()-RAND_MAX/2)/RAND_MAX * pi->speed/max_speed/64.0f;
      if ( pi->flags & PF_AFTERBURNER_ON ) {
         pi->afterburner_decay = timestamp(ABURN_DECAY_TIME);
      }
   }
*/

    // Make ship shake due to shockwave, decreasing in amplitude at the end of the shockwave
    if (pi->flags & PF_IN_SHOCKWAVE) {
        tangles.p += (float)(myrand() - RAND_MAX / 2) / RAND_MAX *
                     shock_amplitude;
        tangles.h += (float)(myrand() - RAND_MAX / 2) / RAND_MAX *
                     shock_amplitude;
    }

    vm_angles_2_matrix(&pi->last_rotmat, &tangles);
    vm_matrix_x_matrix(&tmp, orient, &pi->last_rotmat);
    *orient = tmp;

    vm_orthogonalize_matrix(orient);
}

// -----------------------------------------------------------------------------------------------------------
// add rotational velocity & acceleration

void
physics_sim_rot_editor(matrix *orient, physics_info *pi, float sim_time)
{
    angles tangles;
    vector new_vel;
    matrix tmp;
    angles t1, t2;

    apply_physics(pi->rotdamp, pi->desired_rotvel.x, pi->rotvel.x, sim_time,
                  &new_vel.x, NULL);

    apply_physics(pi->rotdamp, pi->desired_rotvel.y, pi->rotvel.y, sim_time,
                  &new_vel.y, NULL);

    apply_physics(pi->rotdamp, pi->desired_rotvel.z, pi->rotvel.z, sim_time,
                  &new_vel.z, NULL);

    pi->rotvel = new_vel;

    tangles.p = pi->rotvel.x * sim_time;
    tangles.h = pi->rotvel.y * sim_time;
    tangles.b = pi->rotvel.z * sim_time;

    t1 = t2 = tangles;
    t1.h = 0.0f;
    t1.b = 0.0f;
    t2.p = 0.0f;
    t2.b = 0.0f;

    // put in p & b like normal
    vm_angles_2_matrix(&pi->last_rotmat, &t1);
    vm_matrix_x_matrix(&tmp, orient, &pi->last_rotmat);

    // Put in heading separately
    vm_angles_2_matrix(&pi->last_rotmat, &t2);
    vm_matrix_x_matrix(orient, &pi->last_rotmat, &tmp);

    vm_orthogonalize_matrix(orient);
}

// Adds velocity to position
// finds velocity and displacement in local coords
void
physics_sim_vel(vector *position, physics_info *pi, float sim_time,
                matrix *orient)
{
    vector local_disp; // displacement in this frame
    vector local_v_in; // velocity in local coords at the start of this frame
    vector local_desired_vel; // desired velocity in local coords
    vector local_v_out; // velocity in local coords following this frame
    vector damp;

    //   Maybe clear the reduced_damp flag.
    //   This fixes the problem of the player getting near-instantaneous acceleration under unknown circumstances.
    //   The larger problem is probably that PF_USE_VEL is getting stuck set.
    if ((pi->flags & PF_REDUCED_DAMP) &&
        (timestamp_elapsed(pi->reduced_damp_decay))) {
        pi->flags &= ~PF_REDUCED_DAMP;
    }

    // Set up damping constants based on special conditions
    // ie. shockwave, collision, weapon, dead
    if (pi->flags & PF_DEAD_DAMP) {
        // side_slip_time_const is already quite large and now needs to be applied in all directions
        vm_vec_make(&damp, pi->side_slip_time_const, pi->side_slip_time_const,
                    pi->side_slip_time_const);
    }
    else if (pi->flags & PF_REDUCED_DAMP) {
        // case of shock, weapon, collide, etc.
        if (timestamp_elapsed(pi->reduced_damp_decay)) {
            vm_vec_make(&damp, pi->side_slip_time_const, pi->side_slip_time_const,
                        0.0f);
        }
        else {
            // damp is multiplied by fraction and not fraction^2, gives better collision separation
            float reduced_damp_fraction_time_left = timestamp_until(
                                                        pi->reduced_damp_decay) /
                                                    (float)REDUCED_DAMP_TIME;
            damp.x = pi->side_slip_time_const *
                     (1 + (REDUCED_DAMP_FACTOR - 1) *
                              reduced_damp_fraction_time_left);
            damp.y = pi->side_slip_time_const *
                     (1 + (REDUCED_DAMP_FACTOR - 1) *
                              reduced_damp_fraction_time_left);
            damp.z = pi->side_slip_time_const * reduced_damp_fraction_time_left *
                     REDUCED_DAMP_FACTOR;
        }
    }
    else {
        // regular damping
        vm_vec_make(&damp, pi->side_slip_time_const, pi->side_slip_time_const,
                    0.0f);
    }

    // Note: CANNOT maintain a *local velocity* since a rotation can occur in this frame.
    // thus the local velocity of in the next frame can be different (this would require rotate to change local vel
    // and this is not desired

    // get local components of current velocity
    vm_vec_rotate(&local_v_in, &pi->vel, orient);

    // get local components of desired velocity
    vm_vec_rotate(&local_desired_vel, &pi->desired_vel, orient);

    // find updated LOCAL velocity and position in the local x direction
    apply_physics(damp.x, local_desired_vel.x, local_v_in.x, sim_time,
                  &local_v_out.x, &local_disp.x);

    // find updated LOCAL velocity and position in the local y direction
    apply_physics(damp.y, local_desired_vel.y, local_v_in.y, sim_time,
                  &local_v_out.y, &local_disp.y);

    // find updated LOCAL velocity and position in the local z direction
    // for player ship, damp should normally be zero, but may be altered in a shockwave
    //  in death, shockwave,etc. we want damping time const large for all 3 axes
    // warp in test - make excessive speed drop exponentially from max allowed
    // become (0.01x in 3 sec)

    int special_warp_in = FALSE;
    float excess = local_v_in.z - pi->max_vel.z;
    if (excess > 5 && (pi->flags & PF_SPECIAL_WARP_IN)) {
        special_warp_in = TRUE;
        float exp_factor = float(exp(-sim_time / SPECIAL_WARP_T_CONST));
        local_v_out.z = pi->max_vel.z + excess * exp_factor;
        local_disp.z = (pi->max_vel.z * sim_time) +
                       excess *
                           (float(SPECIAL_WARP_T_CONST) * (1.0f - exp_factor));
    }
    else if (pi->flags & PF_SPECIAL_WARP_OUT) {
        float exp_factor = float(exp(-sim_time / SPECIAL_WARP_T_CONST));
        vector temp;
        vm_vec_rotate(&temp, &pi->prev_ramp_vel, orient);
        float deficeit = temp.z - local_v_in.z;
        local_v_out.z = local_v_in.z + deficeit * (1.0f - exp_factor);
        local_disp.z = (local_v_in.z * sim_time) +
                       deficeit * (sim_time - (float(SPECIAL_WARP_T_CONST) *
                                               (1.0f - exp_factor)));
    }
    else {
        apply_physics(damp.z, local_desired_vel.z, local_v_in.z, sim_time,
                      &local_v_out.z, &local_disp.z);
    }

    // maybe turn off special warp in flag
    if ((pi->flags & PF_SPECIAL_WARP_IN) && (excess < 5)) {
        pi->flags &= ~(PF_SPECIAL_WARP_IN);
    }

    // update world position from local to world coords using orient
    vector world_disp;
    vm_vec_unrotate(&world_disp, &local_disp, orient);
#ifdef PHYS_DEBUG
    // check for  excess velocity or translation
    // GET DaveA.
    if ((Game_mode & GM_IN_MISSION) && (Game_mode & GM_NORMAL)) {
        // Assert( (sim_time > 0.5f) || (vm_vec_mag_squared(&pi->vel) < 500*500) );
        // Assert( (sim_time > 0.5f) || (vm_vec_mag_squared(&world_disp) < 350*350) );
    }
#endif
    vm_vec_add2(position, &world_disp);

    // update world velocity
    vm_vec_unrotate(&pi->vel, &local_v_out, orient);

    if (special_warp_in) {
        vm_vec_rotate(&pi->prev_ramp_vel, &pi->vel, orient);
    }
}

// -----------------------------------------------------------------------------------------------------------
// Simulate a physics object for this frame
void
physics_sim(vector *position, matrix *orient, physics_info *pi, float sim_time)
{
    // check flag which tells us whether or not to do velocity translation
    if (pi->flags & PF_CONST_VEL) {
        vm_vec_scale_add2(position, &pi->vel, sim_time);
    }
    else {
        physics_sim_vel(position, pi, sim_time, orient);

        physics_sim_rot(orient, pi, sim_time);

        pi->speed = vm_vec_mag(
            &pi->vel); //  Note, cannot use quick version, causes cumulative error, increasing speed.
        pi->fspeed = vm_vec_dot(
            &orient->fvec,
            &pi->vel); // instead of vector magnitude -- use only forward vector since we are only interested in forward velocity
    }
}

// -----------------------------------------------------------------------------------------------------------
// Simulate a physics object for this frame.  Used by the editor.  The difference between
// this function and physics_sim() is that this one uses a heading change to rotate around
// the universal Y axis, rather than the local orientation's Y axis.  Banking is also ignored.
void
physics_sim_editor(vector *position, matrix *orient, physics_info *pi,
                   float sim_time)
{
    physics_sim_vel(position, pi, sim_time, orient);
    physics_sim_rot_editor(orient, pi, sim_time);
    pi->speed = vm_vec_mag_quick(&pi->vel);
    pi->fspeed = vm_vec_dot(
        &orient->fvec,
        &pi->vel); // instead of vector magnitude -- use only forward vector since we are only interested in forward velocity
}

// function to predict an object's position given the delta time and an objects physics info
void
physics_predict_pos(physics_info *pi, float delta_time, vector *predicted_pos)
{
    apply_physics(pi->side_slip_time_const, pi->desired_vel.x, pi->vel.x,
                  delta_time, NULL, &predicted_pos->x);

    apply_physics(pi->side_slip_time_const, pi->desired_vel.y, pi->vel.y,
                  delta_time, NULL, &predicted_pos->y);

    apply_physics(pi->side_slip_time_const, pi->desired_vel.z, pi->vel.z,
                  delta_time, NULL, &predicted_pos->z);
}

// function to predict an object's velocity given the parameters
void
physics_predict_vel(physics_info *pi, float delta_time, vector *predicted_vel)
{
    if (pi->flags & PF_CONST_VEL) {
        predicted_vel = &pi->vel;
    }
    else {
        apply_physics(pi->side_slip_time_const, pi->desired_vel.x, pi->vel.x,
                      delta_time, &predicted_vel->x, NULL);

        apply_physics(pi->side_slip_time_const, pi->desired_vel.y, pi->vel.y,
                      delta_time, &predicted_vel->y, NULL);

        apply_physics(pi->side_slip_time_const, pi->desired_vel.z, pi->vel.z,
                      delta_time, &predicted_vel->z, NULL);
    }
}

// function to predict position and velocity of an object
void
physics_predict_pos_and_vel(physics_info *pi, float delta_time,
                            vector *predicted_vel, vector *predicted_pos)
{
    apply_physics(pi->side_slip_time_const, pi->desired_vel.x, pi->vel.x,
                  delta_time, &predicted_vel->x, &predicted_pos->x);

    apply_physics(pi->side_slip_time_const, pi->desired_vel.y, pi->vel.y,
                  delta_time, &predicted_vel->y, &predicted_pos->y);

    apply_physics(pi->side_slip_time_const, pi->desired_vel.z, pi->vel.z,
                  delta_time, &predicted_vel->z, &predicted_pos->z);
}

// physics_read_flying_controls()
//
// parmeters:  *orient  ==>
//             *pi      ==>
//             *ci      ==>
// Adam: Uncomment-out this define to enable banking while turning.
#define BANK_WHEN_TURN

// function looks at the flying controls and the current velocity to determine a goal velocity
// function determines velocity in object's reference frame and goal velocity in object's reference frame
void
physics_read_flying_controls(matrix *orient, physics_info *pi, control_info *ci,
                             float sim_time, vector *wash_rot)
{
    vector
        goal_vel; // goal velocity in local coords, *not* accounting for ramping of velcity
    float ramp_time_const; // time constant for velocity ramping

    float velocity_ramp(float v_in, float v_goal, float time_const, float t);

    //   if ( keyd_pressed[KEY_LSHIFT] ) {
    //      keyd_pressed[KEY_LSHIFT] = 0;
    //      Int3();
    //   }

    ci->forward += (ci->forward_cruise_percent / 100.0f);

    //   mprintf(("ci->forward == %7.3f\n", ci->forward));

    // give control imput to cause rotation in engine wash
    extern int Wash_on;
    if (wash_rot && Wash_on) {
        ci->pitch += wash_rot->x;
        ci->bank += wash_rot->z;
        ci->heading += wash_rot->y;
    }

    if (ci->pitch > 1.0f)
        ci->pitch = 1.0f;
    else if (ci->pitch < -1.0f)
        ci->pitch = -1.0f;

    if (ci->vertical > 1.0f)
        ci->vertical = 1.0f;
    else if (ci->vertical < -1.0f)
        ci->vertical = -1.0f;

    if (ci->heading > 1.0f)
        ci->heading = 1.0f;
    else if (ci->heading < -1.0f)
        ci->heading = -1.0f;

    if (ci->sideways > 1.0f)
        ci->sideways = 1.0f;
    else if (ci->sideways < -1.0f)
        ci->sideways = -1.0f;

    if (ci->bank > 1.0f)
        ci->bank = 1.0f;
    else if (ci->bank < -1.0f)
        ci->bank = -1.0f;

    if (pi->flags & PF_AFTERBURNER_ON)
        ci->forward = 1.0f;

    if (ci->forward > 1.0f)
        ci->forward = 1.0f;
    else if (ci->forward < -1.0f)
        ci->forward = -1.0f;

    pi->desired_rotvel.x = ci->pitch * pi->max_rotvel.x;
    pi->desired_rotvel.y = ci->heading * pi->max_rotvel.y;

    float delta_bank;

#ifdef BANK_WHEN_TURN
    //   To change direction of bank, negate the whole expression.
    //   To increase magnitude of banking, decrease denominator.
    //   Adam: The following statement is all the math for banking while turning.
    delta_bank = -(ci->heading * pi->max_rotvel.y) / 2.0f;
#else
    delta_bank = 0.0f;
#endif

    pi->desired_rotvel.z = ci->bank * pi->max_rotvel.z + delta_bank;
    pi->forward_thrust = ci->forward;

    if (pi->flags & PF_AFTERBURNER_ON) {
        goal_vel.x = ci->sideways * pi->afterburner_max_vel.x;
        goal_vel.y = ci->vertical * pi->afterburner_max_vel.y;
        goal_vel.z = ci->forward * pi->afterburner_max_vel.z;
    }
    else {
        goal_vel.x = ci->sideways * pi->max_vel.x;
        goal_vel.y = ci->vertical * pi->max_vel.y;
        goal_vel.z = ci->forward * pi->max_vel.z;
    }

    if (goal_vel.z < -pi->max_rear_vel)
        goal_vel.z = -pi->max_rear_vel;

    if (pi->flags & PF_ACCELERATES) {
        //
        // Determine *resultant* DESIRED VELOCITY (desired_vel) accounting for RAMPING of velocity
        // Use LOCAL coordinates
        // if slide_enabled, ramp velocity for x and y, otherwise set goal (0)
        //    always ramp velocity for z
        //

        // If reduced damp in effect, then adjust ramp_velocity and desired_velocity can not change as fast.
        // Scale according to reduced_damp_time_expansion.
        float reduced_damp_ramp_time_expansion;
        if (pi->flags & PF_REDUCED_DAMP) {
            float reduced_damp_fraction_time_left = timestamp_until(
                                                        pi->reduced_damp_decay) /
                                                    (float)REDUCED_DAMP_TIME;
            reduced_damp_ramp_time_expansion =
                1.0f +
                (REDUCED_DAMP_FACTOR - 1) * reduced_damp_fraction_time_left;
        }
        else {
            reduced_damp_ramp_time_expansion = 1.0f;
        }

        //  if ( !use_descent && (Player_obj->phys_info.forward_accel_time_const < 0.1) && !(Ships[Player_obj->instance].flags & SF_DYING) && (Player_obj->type != OBJ_OBSERVER) )
        //        Int3();  // Get dave A

        if (pi->flags & PF_SLIDE_ENABLED) {
            // determine the local velocity
            // deterimine whether accelerating or decleration toward goal for x
            if (goal_vel.x > 0.0f) {
                if (goal_vel.x >= pi->prev_ramp_vel.x)
                    ramp_time_const = pi->slide_accel_time_const;
                else
                    ramp_time_const = pi->slide_decel_time_const;
            }
            else { // goal_vel.x <= 0.0
                if (goal_vel.x <= pi->prev_ramp_vel.x)
                    ramp_time_const = pi->slide_accel_time_const;
                else
                    ramp_time_const = pi->slide_decel_time_const;
            }
            // If reduced damp in effect, then adjust ramp_velocity and desired_velocity can not change as fast
            if (pi->flags & PF_REDUCED_DAMP) {
                ramp_time_const *= reduced_damp_ramp_time_expansion;
            }
            pi->prev_ramp_vel.x = velocity_ramp(pi->prev_ramp_vel.x, goal_vel.x,
                                                ramp_time_const, sim_time);

            // deterimine whether accelerating or decleration toward goal for y
            if (goal_vel.y > 0.0f) {
                if (goal_vel.y >= pi->prev_ramp_vel.y)
                    ramp_time_const = pi->slide_accel_time_const;
                else
                    ramp_time_const = pi->slide_decel_time_const;
            }
            else { // goal_vel.y <= 0.0
                if (goal_vel.y <= pi->prev_ramp_vel.y)
                    ramp_time_const = pi->slide_accel_time_const;
                else
                    ramp_time_const = pi->slide_decel_time_const;
            }
            // If reduced damp in effect, then adjust ramp_velocity and desired_velocity can not change as fast
            if (pi->flags & PF_REDUCED_DAMP) {
                ramp_time_const *= reduced_damp_ramp_time_expansion;
            }
            pi->prev_ramp_vel.y = velocity_ramp(pi->prev_ramp_vel.y, goal_vel.y,
                                                ramp_time_const, sim_time);
        }
        else {
            // slide not enabled
            pi->prev_ramp_vel.x = 0.0f;
            pi->prev_ramp_vel.y = 0.0f;
        }

        // find ramp velocity in the forward direction
        if (goal_vel.z >= pi->prev_ramp_vel.z) {
            if (pi->flags & PF_AFTERBURNER_ON)
                ramp_time_const = pi->afterburner_forward_accel_time_const;
            else
                ramp_time_const = pi->forward_accel_time_const;
        }
        else
            ramp_time_const = pi->forward_decel_time_const;

        // If reduced damp in effect, then adjust ramp_velocity and desired_velocity can not change as fast
        if (pi->flags & PF_REDUCED_DAMP) {
            ramp_time_const *= reduced_damp_ramp_time_expansion;
        }
        pi->prev_ramp_vel.z = velocity_ramp(pi->prev_ramp_vel.z, goal_vel.z,
                                            ramp_time_const, sim_time);

        // this translates local desired velocities to world velocities

        vm_vec_zero(&pi->desired_vel);
        vm_vec_scale_add2(&pi->desired_vel, &orient->rvec, pi->prev_ramp_vel.x);
        vm_vec_scale_add2(&pi->desired_vel, &orient->uvec, pi->prev_ramp_vel.y);
        vm_vec_scale_add2(&pi->desired_vel, &orient->fvec, pi->prev_ramp_vel.z);
    }
    else // object does not accelerate  (PF_ACCELERATES not set)
        pi->desired_vel = pi->vel;
}

// ----------------------------------------------------------------
// Do *dest = *delta unless:
//          *delta is pretty small
//    and   they are of different signs.
void
physics_set_rotvel_and_saturate(float *dest, float delta)
{
    /*
   if ((delta ^ *dest) < 0) {
      if (abs(delta) < F1_0/8) {
         // mprintf((0, "D"));
         *dest = delta/4;
      } else
         // mprintf((0, "d"));
         *dest = delta;
   } else {
      // mprintf((0, "!"));
      *dest = delta;
   }
   */
    *dest = delta;
}

// ----------------------------------------------------------------------------
// physics_apply_whack applies an instaneous whack on an object changing
// both the objects velocity and the rotational velocity based on the impulse
// being applied.
//
// input:   impulse     =>    impulse vector ( force*time = impulse = change in momentum (mv) )
//          pos         =>    vector from center of mass to location of where the force acts
//          pi          =>    pointer to phys_info struct of object getting whacked
//          orient      =>    orientation matrix (needed to set rotational impulse in body coords)
//          mass        =>    mass of the object (may be different from pi.mass if docked)
//
#define WHACK_LIMIT 0.001f
#define ROTVEL_WHACK_CONST 0.12
void
physics_apply_whack(vector *impulse, vector *pos, physics_info *pi,
                    matrix *orient, float mass)
{
    vector local_torque, torque;
    //   vector   npos;

    //   Detect null vector.
    if ((fl_abs(impulse->x) <= WHACK_LIMIT) &&
        (fl_abs(impulse->y) <= WHACK_LIMIT) &&
        (fl_abs(impulse->z) <= WHACK_LIMIT))
        return;

    // first do the rotational velocity
    // calculate the torque on the body based on the point on the
    // object that was hit and the momentum being applied to the object

    vm_vec_crossprod(&torque, pos, impulse);
    vm_vec_rotate(&local_torque, &torque, orient);

    vector delta_rotvel;
    vm_vec_rotate(&delta_rotvel, &local_torque, &pi->I_body_inv);
    vm_vec_scale(&delta_rotvel, (float)ROTVEL_WHACK_CONST);
    vm_vec_add2(&pi->rotvel, &delta_rotvel);

#ifdef ROT_DEBUG
    if (check_rotvel_limit(pi)) {
        nprintf(("Physics", "rotvel reset in physics_apply_whack\n"));
    }
#endif

    //mprintf(("Whack: %7.3f %7.3f %7.3f\n", pi->rotvel.x, pi->rotvel.y, pi->rotvel.z));

    // instant whack on the velocity
    // reduce damping on all axes
    pi->flags |= PF_REDUCED_DAMP;
    update_reduced_damp_timestamp(pi, vm_vec_mag(impulse));

    // find time for shake from weapon to end
    int dtime = timestamp_until(pi->afterburner_decay);
    if (dtime < WEAPON_SHAKE_TIME) {
        pi->afterburner_decay = timestamp(WEAPON_SHAKE_TIME);
    }

    vm_vec_scale_add2(&pi->vel, impulse, 1.0f / mass);
    if (!(pi->flags & PF_USE_VEL) &&
        (vm_vec_mag_squared(&pi->vel) > MAX_SHIP_SPEED * MAX_SHIP_SPEED)) {
        // Get DaveA
        nprintf(("Physics", "speed reset in physics_apply_whack [speed: %f]\n",
                 vm_vec_mag(&pi->vel)));
        //     Int3();
        vm_vec_normalize(&pi->vel);
        vm_vec_scale(&pi->vel, (float)RESET_SHIP_SPEED);
    }
    vm_vec_rotate(
        &pi->prev_ramp_vel, &pi->vel,
        orient); // set so velocity will ramp starting from current speed
    // ramped velocity is now affected by collision
}

// function generates a velocity ramp with a given time constant independent of frame rate
// uses an exponential approach to desired velocity and a cheat when close to improve closure speed
float
velocity_ramp(float v_in, float v_goal, float ramp_time_const, float t)
{
    float delta_v;
    float decay_factor;
    float dist;

    // JAS: If no time elapsed, change nothing
    if (t == 0.0f)
        return v_in;

    delta_v = v_goal - v_in;
    dist = (float)fl_abs(delta_v);

    // hack to speed up closure when close to goal
    if (dist < ramp_time_const / 3)
        ramp_time_const = dist / 3;

    // Rather than get a divide by zero, just go to the goal
    if (ramp_time_const < 0.0001f) {
        return v_goal;
    }

    // determine decay factor  (ranges from 0 for short times to 1 for long times)
    // when decay factor is near 0, the velocity in nearly unchanged
    // when decay factor in near 1, the velocity approaches goal
    decay_factor = (float)exp(-t / ramp_time_const);

    return (v_in + delta_v * (1.0f - decay_factor));
}

// ----------------------------------------------------------------------------
// physics_apply_shock applies applies a shockwave to an object.  This causes a velocity impulse and
// and a rotational impulse.  This is different than physics_apply_whack since a shock wave is a pressure
// wave which acts over the *surface* of the object, not a point.
//
// inputs:  direction_vec     =>    a position vector whose direction is from the center of the shock wave to the object
//          pressure          =>    the pressure of the shock wave at the object
//          pi                =>    physics_info structure
//          orient            =>    matrix orientation of the object
//          min               =>    vector of minimum values of the bounding box
//          max               =>    vector of maximum values of the bounding box
//          radius            =>    bounding box radius of the object, used for scaling rotation
//
// outputs: makes changes to physics_info structure rotvel and vel variables
//
#define STD_PRESSURE 1000 // amplitude of standard shockwave blasts
#define MIN_RADIUS 10 // radius within which full rotvel and shake applied
#define MAX_RADIUS 50 // radius at which no rotvel or shake applied
#define MAX_ROTVEL 0.4 // max rotational velocity
#define MAX_SHAKE 0.1 // max rotational amplitude of shake
#define MAX_VEL 8 // max vel from shockwave
void
physics_apply_shock(vector *direction_vec, float pressure, physics_info *pi,
                    matrix *orient, vector *min, vector *max, float radius)
{
    vector normal;
    vector local_torque, temp_torque, torque;
    vector impact_vec;
    vector area;
    vector sin;

    if (radius > MAX_RADIUS) {
        return;
    }

    vm_vec_normalize_safe(direction_vec);

    area.x = (max->y - min->z) * (max->z - min->z);
    area.y = (max->x - min->x) * (max->z - min->z);
    area.z = (max->x - min->x) * (max->y - min->y);

    normal.x = vm_vec_dotprod(direction_vec, &orient->rvec);
    normal.y = vm_vec_dotprod(direction_vec, &orient->uvec);
    normal.z = vm_vec_dotprod(direction_vec, &orient->fvec);

    sin.x = fl_sqrt(fl_abs(1.0f - normal.x * normal.x));
    sin.y = fl_sqrt(fl_abs(1.0f - normal.y * normal.y));
    sin.z = fl_sqrt(fl_abs(1.0f - normal.z * normal.z));

    vm_vec_make(&torque, 0.0f, 0.0f, 0.0f);

    // find the torque exerted due to the shockwave hitting each face
    //  model the effect of the shockwave as if the shockwave were a plane of projectiles,
    //  all moving in the direction direction_vec.  then find the torque as the cross prod
    //  of the force (pressure * area * normal * sin * scale * mass)
    //  normal takes account the fraction of the surface exposed to the shockwave
    //  the sin term is not technically needed but "feels" better
    //  scale factors out the increase in area with larger objects
    //  more massive objects get less rotation

    // find torque due to forces on the right/left face
    if (normal.x < 0.0f) // normal < 0, hits the right face
        vm_vec_copy_scale(&impact_vec, &orient->rvec,
                          max->x * pressure * area.x * normal.x * sin.x /
                              pi->mass);
    else // normal > 0, hits the left face
        vm_vec_copy_scale(&impact_vec, &orient->rvec,
                          min->x * pressure * area.x * -normal.x * sin.x /
                              pi->mass);

    vm_vec_crossprod(&temp_torque, &impact_vec, direction_vec);
    vm_vec_add2(&torque, &temp_torque);

    // find torque due to forces on the up/down face
    if (normal.y < 0.0f)
        vm_vec_copy_scale(&impact_vec, &orient->uvec,
                          max->y * pressure * area.y * normal.y * sin.y /
                              pi->mass);
    else
        vm_vec_copy_scale(&impact_vec, &orient->uvec,
                          min->y * pressure * area.y * -normal.y * sin.y /
                              pi->mass);

    vm_vec_crossprod(&temp_torque, &impact_vec, direction_vec);
    vm_vec_add2(&torque, &temp_torque);

    // find torque due to forces on the forward/backward face
    if (normal.z < 0.0f)
        vm_vec_copy_scale(&impact_vec, &orient->fvec,
                          max->z * pressure * area.z * normal.z * sin.z /
                              pi->mass);
    else
        vm_vec_copy_scale(&impact_vec, &orient->fvec,
                          min->z * pressure * area.z * -normal.z * sin.z /
                              pi->mass);

    vm_vec_crossprod(&temp_torque, &impact_vec, direction_vec);
    vm_vec_add2(&torque, &temp_torque);

    // compute delta rotvel, scale according to blast and radius
    float scale;
    vector delta_rotvel;
    vm_vec_rotate(&local_torque, &torque, orient);
    vm_vec_copy_normalize(&delta_rotvel, &local_torque);
    if (radius < MIN_RADIUS) {
        scale = 1.0f;
    }
    else {
        scale = (MAX_RADIUS - radius) / (MAX_RADIUS - MIN_RADIUS);
    }
    vm_vec_scale(&delta_rotvel,
                 (float)(MAX_ROTVEL * (pressure / STD_PRESSURE) * scale));
    // nprintf(("Physics", "rotvel scale %f\n", (MAX_ROTVEL*(pressure/STD_PRESSURE)*scale)));
    vm_vec_add2(&pi->rotvel, &delta_rotvel);

    // set shockwave shake amplitude, duration, flag
    pi->shockwave_shake_amp = (float)(MAX_SHAKE * (pressure / STD_PRESSURE) *
                                      scale);
    pi->shockwave_decay = timestamp(SW_BLAST_DURATION);
    pi->flags |= PF_IN_SHOCKWAVE;

    // set reduced translational damping, set flags
    float velocity_scale = (float)MAX_VEL * scale;
    pi->flags |= PF_REDUCED_DAMP;
    update_reduced_damp_timestamp(pi, velocity_scale * pi->mass);
    vm_vec_scale_add2(&pi->vel, direction_vec, velocity_scale);
    vm_vec_rotate(
        &pi->prev_ramp_vel, &pi->vel,
        orient); // set so velocity will ramp starting from current speed

    // check that kick from shockwave is not too large
    if (!(pi->flags & PF_USE_VEL) &&
        (vm_vec_mag_squared(&pi->vel) > MAX_SHIP_SPEED * MAX_SHIP_SPEED)) {
        // Get DaveA
        nprintf(("Physics", "speed reset in physics_apply_shock [speed: %f]\n",
                 vm_vec_mag(&pi->vel)));
        //     Int3();
        vm_vec_normalize(&pi->vel);
        vm_vec_scale(&pi->vel, (float)RESET_SHIP_SPEED);
    }

#ifdef ROT_DEBUG
    if (check_rotvel_limit(pi)) {
        nprintf(("Physics", "rotvel reset in physics_apply_shock\n"));
    }
#endif

    // ramped velocity is now affected by collision
}

// ----------------------------------------------------------------------------
// physics_collide_whack applies an instaneous whack on an object changing
// both the objects velocity and the rotational velocity based on the impulse
// being applied.
//
// input:   impulse              =>    impulse vector ( force*time = impulse = change in momentum (mv) )
//          world_delta_rotvel   =>    change in rotational velocity (already calculated)
//          pi                   =>    pointer to phys_info struct of object getting whacked
//          orient               =>    orientation matrix (needed to set rotational impulse in body coords)
//

// Warning:  Do not change ROTVEL_COLLIDE_WHACK_CONST.  This will mess up collision physics.
// If you need to change the rotation, change  COLLISION_ROTATION_FACTOR in collide_ship_ship.
#define ROTVEL_COLLIDE_WHACK_CONST 1.0
void
physics_collide_whack(vector *impulse, vector *world_delta_rotvel,
                      physics_info *pi, matrix *orient)
{
    vector body_delta_rotvel;

    //   Detect null vector.
    if ((fl_abs(impulse->x) <= WHACK_LIMIT) &&
        (fl_abs(impulse->y) <= WHACK_LIMIT) &&
        (fl_abs(impulse->z) <= WHACK_LIMIT))
        return;

    vm_vec_rotate(&body_delta_rotvel, world_delta_rotvel, orient);
    //   vm_vec_scale( &body_delta_rotvel, (float) ROTVEL_COLLIDE_WHACK_CONST );
    vm_vec_add2(&pi->rotvel, &body_delta_rotvel);

#ifdef ROT_DEBUG
    if (check_rotvel_limit(pi)) {
        nprintf(("Physics", "rotvel reset in physics_collide_whack\n"));
    }
#endif

    update_reduced_damp_timestamp(pi, vm_vec_mag(impulse));

    // find time for shake from weapon to end
    int dtime = timestamp_until(pi->afterburner_decay);
    if (dtime < WEAPON_SHAKE_TIME) {
        pi->afterburner_decay = timestamp(WEAPON_SHAKE_TIME);
    }

    pi->flags |= PF_REDUCED_DAMP;
    vm_vec_scale_add2(&pi->vel, impulse, 1.0f / pi->mass);
    // check that collision does not give ship too much speed
    // reset if too high
    if (!(pi->flags & PF_USE_VEL) &&
        (vm_vec_mag_squared(&pi->vel) > MAX_SHIP_SPEED * MAX_SHIP_SPEED)) {
        // Get DaveA
        nprintf(("Physics", "speed reset in physics_collide_whack [speed: %f]\n",
                 vm_vec_mag(&pi->vel)));
        //     Int3();
        vm_vec_normalize(&pi->vel);
        vm_vec_scale(&pi->vel, (float)RESET_SHIP_SPEED);
    }
    vm_vec_rotate(
        &pi->prev_ramp_vel, &pi->vel,
        orient); // set so velocity will ramp starting from current speed
    // ramped velocity is now affected by collision
    // rotate previous ramp velocity (in model coord) to be same as vel (in world coords)
}

int
check_rotvel_limit(physics_info *pi)
{
    if (0 == pi->flags) // weapon
        return 0;

    if (Fred_running)
        return 0;

    int change_made = 0;
    if (!(pi->flags & PF_DEAD_DAMP)) {
        // case of normal, live ship
        // -- Commented out by MK: Assert( vm_vec_mag_squared(&pi->max_rotvel) > ROTVEL_TOL );
        // Assert( (pi->max_rotvel.x <= ROTVEL_CAP) && (pi->max_rotvel.y <= ROTVEL_CAP) && (pi->max_rotvel.z <= ROTVEL_CAP) );
        //     Warning(LOCATION,"Excessive rotvel (wx: %f, wy: %f, wz:%f)\n", pi->rotvel.x, pi->rotvel.y, pi->rotvel.z);
        if (fl_abs(pi->rotvel.x) > pi->max_rotvel.x) {
            pi->rotvel.x = (pi->rotvel.x / fl_abs(pi->rotvel.x)) *
                           (pi->max_rotvel.x - (float)ROTVEL_TOL);
            change_made = 1;
        }
        if (fl_abs(pi->rotvel.y) > pi->max_rotvel.y) {
            pi->rotvel.y = (pi->rotvel.y / fl_abs(pi->rotvel.y)) *
                           (pi->max_rotvel.y - (float)ROTVEL_TOL);
            change_made = 1;
        }
        if (fl_abs(pi->rotvel.z) > pi->max_rotvel.z) {
            pi->rotvel.z = (pi->rotvel.z / fl_abs(pi->rotvel.z)) *
                           (pi->max_rotvel.z - (float)ROTVEL_TOL);
            change_made = 1;
        }
    }
    else {
        // case of dead ship
        if (fl_abs(pi->rotvel.x) > DEAD_ROTVEL_CAP) {
            pi->rotvel.x = (pi->rotvel.x / fl_abs(pi->rotvel.x)) *
                           (float)(DEAD_ROTVEL_CAP - ROTVEL_TOL);
            change_made = 1;
        }
        if (fl_abs(pi->rotvel.y) > DEAD_ROTVEL_CAP) {
            pi->rotvel.y = (pi->rotvel.y / fl_abs(pi->rotvel.y)) *
                           (float)(DEAD_ROTVEL_CAP - ROTVEL_TOL);
            change_made = 1;
        }
        if (fl_abs(pi->rotvel.z) > DEAD_ROTVEL_CAP) {
            pi->rotvel.z = (pi->rotvel.z / fl_abs(pi->rotvel.z)) *
                           (float)(DEAD_ROTVEL_CAP - ROTVEL_TOL);
            change_made = 1;
        }
    }
    return change_made;
}

// ----------------------------------------------------------------------------
// update_reduced_damp_timestamp()
//
void
update_reduced_damp_timestamp(physics_info *pi, float impulse)
{
    // Compute duration of reduced damp from present
    // Compare with current value and increase if greater, otherwise ignore
    int reduced_damp_decay_time;
    reduced_damp_decay_time = (int)(REDUCED_DAMP_TIME * impulse /
                                    (REDUCED_DAMP_VEL * pi->mass));
    if (reduced_damp_decay_time > REDUCED_DAMP_TIME)
        reduced_damp_decay_time = REDUCED_DAMP_TIME;

    // Reset timestamp if larger than current (if any)
    if (timestamp_valid(pi->reduced_damp_decay)) {
        int time_left = timestamp_until(pi->reduced_damp_decay);
        if (time_left > 0) {
            // increment old time, but apply cap
            int new_time = reduced_damp_decay_time + time_left;
            if (new_time < REDUCED_DAMP_TIME) {
                pi->reduced_damp_decay = timestamp(new_time);
            }
        }
        else {
            pi->reduced_damp_decay = timestamp(reduced_damp_decay_time);
        }
    }
    else {
        // set if not valid
        pi->reduced_damp_decay = timestamp(reduced_damp_decay_time);
    }
}
