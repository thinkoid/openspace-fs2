/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef _FS_STATISTICS_STATE_HEADER
#define _FS_STATISTICS_STATE_HEADER

#define MISSION_STATS	0
#define ALL_TIME_STATS	1

#include <stats/scoring.hh>

void show_stats_init();
void show_stats_close();

void show_stats_numbers(int stage, int sx, int sy, int dy=10,int add_mission = 0);
void show_stats_label(int stage, int sx, int sy, int dy=10);

#endif
