/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef __FS2_MUZZLEFLASH_HEADER_FILE
#define __FS2_MUZZLEFLASH_HEADER_FILE

// ---------------------------------------------------------------------------------------------------------------------
// MUZZLE FLASH DEFINES/VARS
//

// prototypes
struct object;

// muzzle flash types
#define MAX_MUZZLE_FLASH_TYPES 10
extern int Num_mflash_types;

// ---------------------------------------------------------------------------------------------------------------------
// MUZZLE FLASH FUNCTIONS
//

// initialize muzzle flash stuff for the whole game
void mflash_game_init();

// initialize muzzle flash stuff for the level
void mflash_level_init();

// shutdown stuff for the level
void mflash_level_close();

// create a muzzle flash on the guy
void mflash_create(vector *gun_pos, vector *gun_dir, int mflash_type);

// process muzzle flash stuff
void mflash_process_all();

// render all muzzle flashes
void mflash_render_all();

// lookup type by name
int mflash_lookup(char *name);

#endif
