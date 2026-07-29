/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef _HUDRETICLE_H
#define _HUDRETICLE_H

#include <playerman/player.hh>

extern int Outer_circle_radius[GR_NUM_RESOLUTIONS];
extern int Hud_reticle_center[GR_NUM_RESOLUTIONS][2];

void hud_init_reticle();
void hud_update_reticle(player *pp);
void hud_show_reticle();

#endif
