// -*- mode: c++; -*-

#ifndef FREESPACE2_WEAPON_MUZZLEFLASH_HH
#define FREESPACE2_WEAPON_MUZZLEFLASH_HH

#include <defs.hh>

#include <physics/physics.hh>

// ---------------------------------------------------------------------------------------------------------------------
// MUZZLE FLASH DEFINES/VARS
//

// prototypes
class object;
struct vec3d;

// ---------------------------------------------------------------------------------------------------------------------
// MUZZLE FLASH FUNCTIONS
//

// initialize muzzle flash stuff for the whole game
void mflash_game_init ();

// initialize muzzle flash stuff for the level
void mflash_level_init ();

// shutdown stuff for the level
void mflash_level_close ();

// create a muzzle flash on the guy
void mflash_create (
    vec3d* gun_pos, vec3d* gun_dir, physics_info* pip, int mflash_type,
    object* local = NULL);

// lookup type by name
int mflash_lookup (char* name);

// mark as used
void mflash_mark_as_used (int index = -1);

// level page in
void mflash_page_in (bool load_all = false);

#endif // FREESPACE2_WEAPON_MUZZLEFLASH_HH
