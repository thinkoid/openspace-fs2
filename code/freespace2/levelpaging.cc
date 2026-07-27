/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#include <freespace2/freespace.hh>
#include <bmpman/bmpman.hh>
#include <freespace2/levelpaging.hh>

// All the page in functions
extern void ship_page_in();
extern void debris_page_in();
extern void particle_page_in();
extern void stars_page_in();
extern void hud_page_in();
extern void radar_page_in();
extern void weapons_page_in();
extern void fireballs_page_in();
extern void shockwave_page_in();
extern void shield_hit_page_in();
extern void asteroid_page_in();
extern void training_mission_page_in();
extern void neb2_page_in();
extern void message_pagein_mission_messages();

// Pages in all the texutures for the currently
// loaded mission.  Call game_busy() occasionally...
void level_page_in()
{

	mprintf(( "Beginning level bitmap paging...\n" ));

	bm_page_in_start();

	// Most important ones first
	ship_page_in();
	weapons_page_in();
	fireballs_page_in();
	particle_page_in();
	debris_page_in();
	hud_page_in();
	radar_page_in();
	training_mission_page_in();
	stars_page_in();
	shockwave_page_in();
	shield_hit_page_in();
	asteroid_page_in();
	neb2_page_in();

	// preload mission messages if NOT running low-memory (greater than 48MB)
	if (game_using_low_mem() == false) {
		message_pagein_mission_messages();
	}

	bm_page_in_stop();

	mprintf(( "Ending level bitmap paging...\n" ));

}
