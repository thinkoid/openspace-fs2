/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#include <object/objcollide.hh>
#include <freespace2/freespace.hh>
#include <object/object.hh>
#include <weapon/weapon.hh>

#ifndef NDEBUG
//XSTR:OFF
char *lTeamNames[3] = { "Hostile", "Friendly", "Unknown" };
//XSTR:ON
#endif

#define BOMB_ARM_TIME 1.5f

// Checks weapon-weapon collisions.  pair->a and pair->b are weapons.
// Returns 1 if all future collisions between these can be ignored
int
collide_weapon_weapon(obj_pair *pair)
{
    float A_radius, B_radius;
    object *A = pair->a;
    object *B = pair->b;

    Assert(A->type == OBJ_WEAPON);
    Assert(B->type == OBJ_WEAPON);

    //   Don't allow ship to shoot down its own missile.
    if (A->parent_sig == B->parent_sig)
        return 1;

    //   Only shoot down teammate's missile if not traveling in nearly same direction.
    if (Weapons[A->instance].team == Weapons[B->instance].team)
        if (vm_vec_dot(&A->orient.fvec, &B->orient.fvec) > 0.7f)
            return 1;

    //   Ignore collisions involving a bomb if the bomb is not yet armed.
    weapon *wpA, *wpB;
    weapon_info *wipA, *wipB;

    wpA = &Weapons[A->instance];
    wpB = &Weapons[B->instance];
    wipA = &Weapon_info[wpA->weapon_info_index];
    wipB = &Weapon_info[wpB->weapon_info_index];

    A_radius = A->radius;
    B_radius = B->radius;

    if (wipA->wi_flags & WIF_BOMB) {
        A_radius *= 2; // Makes bombs easier to hit
        if (wipA->lifetime - wpA->lifeleft < BOMB_ARM_TIME)
            return 0;
    }

    if (wipB->wi_flags & WIF_BOMB) {
        B_radius *= 2; // Makes bombs easier to hit
        if (wipB->lifetime - wpB->lifeleft < BOMB_ARM_TIME)
            return 0;
    }

    //   Rats, do collision detection.
    if (collide_subdivide(&A->last_pos, &A->pos, A_radius, &B->last_pos, &B->pos,
                          B_radius)) {
        ship *sap, *sbp;

        sap = &Ships[Objects[A->parent].instance];
        sbp = &Ships[Objects[B->parent].instance];
        // MWA -- commented out next line because it was too long for output window on occation.
        // Yes -- I should fix the output window, but I don't have time to do it now.
        //nprintf(("AI", "[%s] %s's missile %i shot down by [%s] %s's laser %i\n", lTeamNames[sbp->team], sbp->ship_name, B->instance, lTeamNames[sap->team], sap->ship_name, A->instance));
        if (wipA->wi_flags & WIF_BOMB) {
            if (wipB->wi_flags & WIF_BOMB) { // Two bombs collide, detonate both.
                Weapons[A->instance].lifeleft = 0.01f;
                Weapons[B->instance].lifeleft = 0.01f;
                Weapons[A->instance].weapon_flags |= WF_DESTROYED_BY_WEAPON;
                Weapons[B->instance].weapon_flags |= WF_DESTROYED_BY_WEAPON;
            }
            else {
                A->hull_strength -= wipB->damage;
                if (A->hull_strength < 0.0f) {
                    Weapons[A->instance].lifeleft = 0.01f;
                    Weapons[A->instance].weapon_flags |= WF_DESTROYED_BY_WEAPON;
                }
            }
        }
        else if (wipB->wi_flags & WIF_BOMB) {
            B->hull_strength -= wipA->damage;
            if (B->hull_strength < 0.0f) {
                Weapons[B->instance].lifeleft = 0.01f;
                Weapons[B->instance].weapon_flags |= WF_DESTROYED_BY_WEAPON;
            }
        }

        float dist = 0.0f;
        if (Weapons[A->instance].lifeleft == 0.01f) {
            dist = vm_vec_dist_quick(&A->pos, &wpA->homing_pos);
            nprintf((
                "AI",
                "Frame %i: Weapon %s shot down. Dist: %.1f, inner: %.0f, outer: %.0f\n",
                Framecount, wipA->name, dist, wipA->inner_radius,
                wipA->outer_radius));
        }
        if (Weapons[B->instance].lifeleft == 0.01f) {
            dist = vm_vec_dist_quick(&A->pos, &wpB->homing_pos);
            nprintf((
                "AI",
                "Frame %i: Weapon %s shot down. Dist: %.1f, inner: %.0f, outer: %.0f\n",
                Framecount, wipB->name, dist, wipB->inner_radius,
                wipB->outer_radius));
        }
        return 1;
    }

    return 0;
}
