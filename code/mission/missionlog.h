/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#ifndef _MISSIONLOG_H
#define _MISSIONLOG_H

// defined for different mission log entries

#define LOG_SHIP_DESTROYED				1
#define LOG_WING_DESTROYED				2
#define LOG_SHIP_ARRIVE					3
#define LOG_WING_ARRIVE					4
#define LOG_SHIP_DEPART					5
#define LOG_WING_DEPART					6
#define LOG_SHIP_DOCK					7
#define LOG_SHIP_SUBSYS_DESTROYED	8
#define LOG_SHIP_UNDOCK					9
#define LOG_SHIP_DISABLED				10
#define LOG_SHIP_DISARMED				11
#define LOG_PLAYER_REARM				12
#define LOG_PLAYER_REINFORCEMENT		13
#define LOG_GOAL_SATISFIED				14
#define LOG_GOAL_FAILED					15
#define LOG_PLAYER_REARM_ABORT		16
#define LOG_WAYPOINTS_DONE				17
#define LOG_CARGO_REVEALED				18
#define LOG_CAP_SUBSYS_CARGO_REVEALED 19
#define LOG_SELF_DESTRUCT				20

// structure definition for log entries

#define MLF_ESSENTIAL				(1<<0)		// this entry is essential for goal checking code
#define MLF_OBSOLETE					(1<<1)		// this entry is obsolete and will be removed
#define MLF_PRIMARY_FRIENDLY		(1<<2)		// primary object in this entry is friendly
#define MLF_PRIMARY_HOSTILE		(1<<3)		// primary object in this entry is hostile
#define MLF_SECONDARY_FRIENDLY	(1<<4)		// secondary object is friendly
#define MLF_SECONDARY_HOSTILE		(1<<5)		// secondary object is hostile
#define MLF_HIDDEN					(1<<6)		// entry doesn't show up in displayed log.

typedef struct {
	int		type;									// one of the log #defines in MissionLog.h
	int		flags;								// flags used for status of this log entry
	fix		timestamp;							// time in fixed seconds when entry was made from beginning of mission
	char		pname[NAME_LENGTH];				// name of primary object of this action
	char		sname[NAME_LENGTH];				// name of secondary object of this action
	int		index;								// a generic entry which can contain things like wave # (for wing arrivals), goal #, etc
} log_entry;

extern log_entry log_entries[];
extern int last_entry;
extern int Num_log_lines;

// function prototypes

// to be called before each mission starts
extern void mission_log_init();

// adds an entry to the mission log.  The name is a string identifier that is the object
// of the event.
extern void mission_log_add_entry(int type, char *pname, char *sname, int index = -1 );

// function to determine if event happened and what time it happened
extern int mission_log_get_time( int type, char *name, char *sname, fix *time);

// function to determine if event happend count times and return time that the count event
// happened
extern int mission_log_get_time_indexed( int type, char *name, char *sname, int count, fix *time);

// function to show all message log entries during or after mission
// (code stolen liberally from Alan!)
extern void mission_log_scrollback(float frametime);

void message_log_init_scrollback(int pw);
void message_log_shutdown_scrollback();
void mission_log_scrollback(int scroll_offset, int list_x, int list_y, int list_w, int list_h);

#endif
