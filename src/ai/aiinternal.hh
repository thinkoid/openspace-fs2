// -*- mode: c++; -*-

#ifndef FREESPACE2_AI_AIINTERNAL_HH
#define FREESPACE2_AI_AIINTERNAL_HH

#include <defs.hh>

#include <ship/ship.hh>
#include <weapon/weapon.hh>

// Returns true if the specified object is a stealth ship, false if not
bool is_object_stealth_ship (object* objp);

// Number of live turrets on the object attacking
int num_turrets_attacking (object* turret_parent, int target_objnum);

// Determine whether an object is targetable within a nebula (checks for
// stealth)
int object_is_targetable (object* target, ship* viewer);

// Returns the number of enemy fighters within threshold of pos.
int num_nearby_fighters (int enemy_team_mask, vec3d* pos, float threshold);

// Returns true if OK for *aip to fire its current weapon at its current
// target.
int check_ok_to_fire (int objnum, int target_objnum, weapon_info* wip);

// Does all the stuff needed to aim and fire a turret.
void ai_fire_from_turret (ship* shipp, ship_subsys* ss, int parent_objnum);

#endif // FREESPACE2_AI_AIINTERNAL_HH
