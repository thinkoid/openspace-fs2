// -*- mode: c++; -*-

#include <defs.hh>
#include <cmeasure/cmeasure.hh>
#include <gamesnd/gamesnd.hh>
#include <hud/hud.hh>
#include <math/prng.hh>
#include <mission/missionparse.hh>
#include <object/object.hh>
#include <ship/ship.hh>
#include <weapon/weapon.hh>
#include <log/log.hh>

int Cmeasures_homing_check = 0;
int Countermeasures_enabled =
    1; // Debug, set to 0 means no one can fire countermeasures.
const float CMEASURE_DETONATE_DISTANCE = 40.0f;

// Used to set a countermeasure velocity after being launched from a ship as a
// countermeasure ie not as a primary or secondary.
void cmeasure_set_ship_launch_vel (object* objp, object* parent_objp, int) {
    vec3d vel, rand_vec;

    // Get cmeasure rear velocity in world
    vm_vec_scale_add (
        &vel, &parent_objp->phys_info.vel, &parent_objp->orient.vec.fvec,
        -25.0f);

    // Get random velocity vector
    rand_vec = fs2::prng::rand3f (0);

    // Add it to the rear velocity
    vm_vec_scale_add2 (&vel, &rand_vec, 2.0f);

    objp->phys_info.vel = vel;

    // Zero out this stuff so it isn't moving
    vm_vec_zero (&objp->phys_info.rotvel);
    vm_vec_zero (&objp->phys_info.max_vel);
    vm_vec_zero (&objp->phys_info.max_rotvel);

    // blow out his reverse thrusters. Or drag, same thing.
    objp->phys_info.rotdamp = 10000.0f;
    objp->phys_info.side_slip_time_const = 10000.0f;

    objp->phys_info.max_vel.xyz.z = -25.0f;
    vm_vec_copy_scale (
        &objp->phys_info.desired_vel, &objp->orient.vec.fvec,
        objp->phys_info.max_vel.xyz.z);
}

void cmeasure_select_next (ship* shipp) {
    ASSERT (shipp != NULL);
    int i, new_index;

    for (i = 1; i < Num_weapon_types; i++) {
        new_index = (shipp->current_cmeasure + i) % Num_weapon_types;

        if (Weapon_info[new_index].wi_flags[Weapon::Info_Flags::Cmeasure]) {
            shipp->current_cmeasure = new_index;
            return;
        }
    }

    WARNINGF (LOCATION, "Countermeasure type set to %i in frame %i",shipp->current_cmeasure, Framecount);
}

/**
 * @brief If this is a player countermeasure, let the player know they evaded a
 * missile.
 * @param objp [description]
 *
 * During single player games, this function notifies the player that evasion
 * has occurred. Multiplayer games ensure that
 * #send_countermeasure_success_packet() is called to notify the other player.
 */

void cmeasure_maybe_alert_success (object* objp) {
    // Is this a countermeasure, and does it have a parent
    if (objp->type != OBJ_WEAPON || objp->parent < 0) { return; }

    ASSERT (Weapon_info[Weapons[objp->instance].weapon_info_index]
                .wi_flags[Weapon::Info_Flags::Cmeasure]);

    if (objp->parent == OBJ_INDEX (Player_obj)) {
        hud_start_text_flash (XSTR ("Evaded", 1430), 800);
        snd_play (gamesnd_get_game_sound (
            ship_get_sound (Player_obj, GameSounds::MISSILE_EVADED_POPUP)));
    }
}
