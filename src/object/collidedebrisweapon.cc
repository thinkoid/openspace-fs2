// -*- mode: c++; -*-

#include <defs.hh>

#include <asteroid/asteroid.hh>
#include <debris/debris.hh>
#include <math/fvi.hh>
#include <object/objcollide.hh>
#include <object/object.hh>
#include <weapon/weapon.hh>

// placeholder struct for ship_debris collisions
typedef struct ship_weapon_debris_struct {
    object* ship_object;
    object* debris_object;
    vec3d ship_collision_cm_pos;
    vec3d r_ship;
    vec3d collision_normal;
    int shield_hit_tri;
    vec3d shield_hit_tri_point;
    float impulse;
} ship_weapon_debris_struct;

/**
 * Checks debris-weapon collisions.
 * @param pair obj_pair pointer to the two objects. pair->a is debris and
 * pair->b is weapon.
 * @return 1 if all future collisions between these can be ignored
 */
int collide_debris_weapon (obj_pair* pair) {
    vec3d hitpos, hitnormal;
    int hit;
    object* pdebris = pair->a;
    object* weapon_obj = pair->b;

    ASSERT (pdebris->type == OBJ_DEBRIS);
    ASSERT (weapon_obj->type == OBJ_WEAPON);

    // first check the bounding spheres of the two objects.
    hit = fvi_segment_sphere (
        &hitpos, &weapon_obj->last_pos, &weapon_obj->pos, &pdebris->pos,
        pdebris->radius);
    if (hit) {
        hit = debris_check_collision (
            pdebris, weapon_obj, &hitpos, NULL, &hitnormal);

        if (!hit) return 0;

        weapon_hit (weapon_obj, pdebris, &hitpos, -1, &hitnormal);

        debris_hit (
            pdebris, weapon_obj, &hitpos,
            Weapon_info[Weapons[weapon_obj->instance].weapon_info_index]
            .damage);

        return 0;
    }
    else {
        return weapon_will_never_hit (weapon_obj, pdebris, pair);
    }
}

/**
 * Checks debris-weapon collisions.
 * @param pair obj_pair pointer to the two objects. pair->a is debris and
 * pair->b is weapon.
 * @return 1 if all future collisions between these can be ignored
 */
int collide_asteroid_weapon (obj_pair* pair) {
    if (!Asteroids_enabled) return 0;

    vec3d hitpos, hitnormal;
    int hit;
    object* pasteroid = pair->a;
    object* weapon_obj = pair->b;

    ASSERT (pasteroid->type == OBJ_ASTEROID);
    ASSERT (weapon_obj->type == OBJ_WEAPON);

    // first check the bounding spheres of the two objects.
    hit = fvi_segment_sphere (
        &hitpos, &weapon_obj->last_pos, &weapon_obj->pos, &pasteroid->pos,
        pasteroid->radius);
    if (hit) {
        hit = asteroid_check_collision (
            pasteroid, weapon_obj, &hitpos, NULL, &hitnormal);

        if (!hit)
            return 0;

        weapon_hit (weapon_obj, pasteroid, &hitpos, -1, &hitnormal);

        asteroid_hit (
            pasteroid, weapon_obj, &hitpos,
            Weapon_info[Weapons[weapon_obj->instance].weapon_info_index]
            .damage);

        return 0;
    }
    else {
        return weapon_will_never_hit (weapon_obj, pasteroid, pair);
    }
}
