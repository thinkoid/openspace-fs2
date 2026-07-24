/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef __SUBSYS_DAMAGE_H__
#define __SUBSYS_DAMAGE_H__

/////////////////////////////////////////
// engines
/////////////////////////////////////////
// % engine strength required to engage warp
#define SHIP_MIN_ENGINES_TO_WARP 0.3f
// if engines are below this level, still contribute this percent to total
// (unless destroyed, then contribute none).
#define ENGINE_MIN_STR 0.15f

/////////////////////////////////////////
// weapons
/////////////////////////////////////////
// 70% strength or better, weapons always fire
#define SUBSYS_WEAPONS_STR_FIRE_OK 0.7f
// below 20%, weapons will not fire
#define SUBSYS_WEAPONS_STR_FIRE_FAIL 0.2f

/////////////////////////////////////////
// sensors - targeting
/////////////////////////////////////////
// % strength of sensors at which no negative effects on targeting
#define SENSOR_STR_TARGET_NO_EFFECTS 0.3f
// % strength of sensors at which targeting ceases
// to function
#define MIN_SENSOR_STR_TO_TARGET 0.2f

/////////////////////////////////////////
// sensors - radar
/////////////////////////////////////////
// % strength of sensors at which no negative effects on radar
#define SENSOR_STR_RADAR_NO_EFFECTS 0.4f
// % strength of sensors at which radar ceases to function
#define MIN_SENSOR_STR_TO_RADAR 0.1f

/////////////////////////////////////////
// communications
/////////////////////////////////////////
// % strength of communications at which player
// is unable to use squadmate messaging
#define MIN_COMM_STR_TO_MESSAGE 0.3

#define COMM_DESTROYED 0
#define COMM_DAMAGED 1
#define COMM_OK 2

#endif
