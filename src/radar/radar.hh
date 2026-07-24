/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef _RADAR_H
#define _RADAR_H

extern int Radar_static_looping;

extern void radar_init();
extern void radar_plot_object(object *objp);
extern void radar_frame_init();
extern void radar_mission_init();
extern void radar_frame_render(float frametime);

// observer hud rendering code uses this function
void radar_draw_blips_sorted(int distort = 0);
void radar_draw_range();
void radar_blit_gauge();

#endif
